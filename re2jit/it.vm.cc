#include "it.h"
#include "debug.h"
#include "threads.h"
#define STACK_SIZE 256


struct re2jit::native
{
    re2::Prog *_prog;

    rejit_entry_t entry() const
    {
        return _prog->start();
    }

    int run(struct rejit_threadset_t *nfa) const
    {
        re2::Prog::Inst *op;
        ssize_t stack[STACK_SIZE];
        ssize_t stkid;

        while (rejit_thread_dispatch(nfa)) {
            stkid = 0;
            stack[stkid++] = nfa->running->entry;

            while (stkid--) {
                if (BIT_GET(nfa->visited, stack[stkid])) {
                    continue;
                }

                BIT_SET(nfa->visited, stack[stkid]);
                op = _prog->inst(stack[stkid]);

                switch (op->opcode()) {
                    case re2::kInstAlt:
                        stack[stkid++] = op->out1();
                        stack[stkid++] = op->out();
                        break;

                    case re2::kInstAltMatch:
                        re2jit::debug::write("re2jit::vm: can't interpret kInstAltMatch\n");
                        return 0;

                    case re2::kInstByteRange: {
                        if (!nfa->length) {
                            break;
                        }

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
                        if ((size_t) op->cap() < nfa->groups) {
                            nfa->running->groups[op->cap()] = nfa->offset;
                        }

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
                        if (rejit_thread_match(nfa)) {
                            // this match is preferred to whatever is still on stack.
                            stkid = 0;
                        }

                        break;

                    case re2::kInstFail:
                        break;

                    default:
                        re2jit::debug::write("re2jit::vm: unknown opcode %d\n", op->opcode());
                        return 0;
                }
            }
        }

        return 1;
    }
};
