#define _IPTR(x) ((void *) (size_t) (x))


using re2jit::debug;


static inline void *_compile(re2::Prog *prog)
{
    debug::write("re2jit::it: vm mode\n");
    return prog;
}


static inline void _destroy(void *)
{
}


static inline void *_entry(void *prog)
{
    return _IPTR(((re2::Prog *) prog)->start());
}


static inline bool _run(void *prog, struct rejit_threadset_t *nfa, const char *input)
{
    re2::Prog *_prog = (re2::Prog *) prog;

    while (rejit_thread_dispatch(nfa, 1, 0)) {
        re2::Prog::Inst *op = _prog->inst((ssize_t) nfa->running->entry);

        if ((ssize_t) nfa->running->entry == _prog->start()) {
            nfa->running->groups[0] = nfa->input - input;
        }

        switch (op->opcode()) {
            case re2::kInstAlt:
                rejit_thread_fork(nfa, _IPTR(op->out1()));
                nfa->running->entry = _IPTR(op->out());
                break;

            case re2::kInstAltMatch:
                debug::write("re2jit::it: can't interpret kInstAltMatch\n");
                return 0;

            case re2::kInstByteRange: {
                char c = nfa->input[0];

                if (op->foldcase() && 'A' <= c && c <= 'Z') {
                    c += 'a' - 'A';
                }

                if (c < op->lo() || c > op->hi()) {
                    rejit_thread_fail(nfa);
                } else {
                    rejit_thread_wait(nfa, 1);
                }

                nfa->running->entry = _IPTR(op->out());
                break;
            }

            case re2::kInstCapture:
                if ((size_t) op->cap() < nfa->groups) {
                    nfa->running->groups[op->cap()] = nfa->input - input;
                }

                nfa->running->entry = _IPTR(op->out());
                break;

            case re2::kInstEmptyWidth:
                if (op->empty() & ~(nfa->empty)) {
                    rejit_thread_fail(nfa);
                }
                nfa->running->entry = _IPTR(op->out());
                break;

            case re2::kInstNop:
                nfa->running->entry = _IPTR(op->out());
                break;

            case re2::kInstMatch:
                nfa->running->groups[1] = nfa->input - input;
                rejit_thread_match(nfa);
                break;

            case re2::kInstFail:
                rejit_thread_fail(nfa);
                break;

            default:
                debug::write("re2jit::it: unknown opcode %d\n", op->opcode());
                return 0;
        }
    }

    return 1;
}

#undef _IPTR
