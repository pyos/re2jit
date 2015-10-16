#include <vector>

using re2jit::debug;


static inline void *_compile(re2::Prog *prog)
{
    debug::write("re2jit::it: vm mode\n");
    return prog;
}


static inline void _destroy(void *)
{
}


static inline rejit_entry_t _entry(void *prog)
{
    // When in VM mode, entry points are
    return (rejit_entry_t) ((re2::Prog *) prog)->start();
}

#define BIT_INDEX(x, i) (i / (sizeof((x)[0]) * 8))
#define BIT_SHIFT(x, i) (i % (sizeof((x)[0]) * 8))
#define BIT_GET(x, i) ((x)[BIT_INDEX(x, i)] & (1LL << BIT_SHIFT(x, i)))
#define BIT_SET(x, i) ((x)[BIT_INDEX(x, i)] |= 1LL << BIT_SHIFT(x, i))


static inline bool _run(void *prog, struct rejit_threadset_t *nfa)
{
    re2::Prog *_prog = (re2::Prog *) prog;
    re2::Prog::Inst *op;
    ssize_t stack[256];
    ssize_t stkid = 0;
    size_t  visited_size = (_prog->size() + sizeof(size_t) * 8 - 1) / (sizeof(size_t) * 8);
    size_t *visited = new size_t[visited_size];
    int *capture;

    while (rejit_thread_dispatch(nfa, 1)) {
        stkid = 0;
        stack[stkid++] = (ssize_t) nfa->running->entry;
        capture = nfa->running->groups;

        memset(visited, 0, sizeof(size_t) * visited_size);

        while (stkid--) {
            if (BIT_GET(visited, stack[stkid])) {
                continue;
            }

            BIT_SET(visited, stack[stkid]);
            op = _prog->inst(stack[stkid]);

            if (stack[stkid] == _prog->start()) {
                capture[0] = nfa->offset;
            }

            switch (op->opcode()) {
                case re2::kInstAlt:
                    stack[stkid++] = op->out1();
                    stack[stkid++] = op->out();
                    break;

                case re2::kInstAltMatch:
                    debug::write("re2jit::it: can't interpret kInstAltMatch\n");
                    delete[] visited;
                    return 0;

                case re2::kInstByteRange: {
                    char c = nfa->input[0];

                    if (op->foldcase() && 'A' <= c && c <= 'Z') {
                        c += 'a' - 'A';
                    }

                    if (c < op->lo() || c > op->hi()) {
                        break;
                    }

                    rejit_thread_wait(nfa, op->out(), 1);
                    break;
                }

                case re2::kInstCapture:
                    // TODO
                    stack[stkid++] = op->out();
                    break;

                case re2::kInstEmptyWidth:
                    if (!(op->empty() & ~(nfa->empty))) {
                        stack[stkid++] = op->out();
                    }
                    break;

                case re2::kInstNop:
                    stack[stkid++] = op->out();
                    break;

                case re2::kInstMatch:
                    capture[1] = nfa->offset;

                    if (rejit_thread_match(nfa)) {
                        // this match is preferred to whatever is still on stack.
                        stkid = 0;
                    }

                    break;

                case re2::kInstFail:
                    break;

                default:
                    debug::write("re2jit::it: unknown opcode %d\n", op->opcode());
                    delete[] visited;
                    return 0;
            }
        }
    }

    delete[] visited;
    return 1;
}
