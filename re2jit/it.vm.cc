#define STACK_SIZE 1024


struct re2jit::native
{
    re2::Prog *_prog;

    rejit_entry_t entry() const
    {
        return _prog->start();
    }

    void run(struct rejit_threadset_t *nfa) const
    {
        ssize_t stack[STACK_SIZE];
        ssize_t stkid;

        ssize_t restore[STACK_SIZE];
        ssize_t rstid;

        while (rejit_thread_dispatch(nfa)) {
            struct rejit_thread_t *t = nfa->running;

            stkid = rstid = 0;
            stack[stkid++] = t->entry;

            while (stkid--) {
                ssize_t i = stack[stkid];

                if (i < 0) {
                    t->groups[-i] = restore[--rstid];
                    continue;
                }

                if (nfa->visited[i / 8] & (1 << (i % 8)))
                    continue;

                nfa->visited[i / 8] |= 1 << (i % 8);

                RE2JIT_WITH_INST(op, _prog, i,
                    switch (op->opcode()) {
                        case re2jit::inst::kUnicodeType: {
                            uint64_t x = rejit_read_utf8((const uint8_t *) nfa->input, nfa->length);

                            if (0 == x)
                                // not a valid utf-8 character
                                break;

                            rejit_uni_type_t cls = UNICODE_CODEPOINT_TYPE[x & 0xFFFFFFFF];

                            if ((cls & UNICODE_GENERAL) != op->arg())
                                break;

                            rejit_thread_wait(nfa, op->out(), x >> 32);
                            break;
                        }

                        case re2jit::inst::kBackReference: {
                            if (op->arg() * 2 >= nfa->groups)
                                // no idea what that group matched
                                break;

                            int start = t->groups[op->arg() * 2];
                            int end   = t->groups[op->arg() * 2 + 1];

                            if (start == -1 || end < start)
                                // unmatched group.
                                break;

                            if (start == end) {
                                stack[stkid++] = op->out();
                                break;
                            }

                            if (nfa->length < (size_t) (end - start))
                                break;

                            if (memcmp(nfa->input, nfa->input - nfa->offset + start, end - start))
                                break;

                            rejit_thread_wait(nfa, op->out(), end - start);
                            break;
                        }

                        default:
                            re2jit::debug::write("re2jit::vm: unknown extcode %hu\n", op->opcode());
                            break;
                    },

                    switch (op->opcode()) {
                        case re2::kInstAltMatch:
                        case re2::kInstAlt:
                            stack[stkid++] = op->out1();
                            stack[stkid++] = op->out();
                            break;

                        case re2::kInstByteRange: {
                            if (!nfa->length)
                                break;

                            uint8_t c = nfa->input[0];

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
                            if ((size_t) op->cap() < nfa->groups) {
                                restore[rstid++] = t->groups[op->cap()];
                                stack[stkid++] = -op->cap();

                                t->groups[op->cap()] = nfa->offset;
                            }

                            stack[stkid++] = op->out();
                            break;

                        case re2::kInstEmptyWidth:
                            if (!(op->empty() & ~(nfa->empty)))
                                stack[stkid++] = op->out();

                            break;

                        case re2::kInstNop:
                            stack[stkid++] = op->out();
                            break;

                        case re2::kInstMatch:
                            if (rejit_thread_match(nfa))
                                // this match is preferred to whatever is still on stack.
                                stkid = 0;

                            break;

                        default:
                            re2jit::debug::write("re2jit::vm: unknown opcode %d\n", op->opcode());

                        case re2::kInstFail:
                            break;
                    }
                );
            }
        }
    }
};
