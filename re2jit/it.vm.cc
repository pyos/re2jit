#include "it.h"
#include "debug.h"
#include "threads.h"


struct re2jit::native
{
    re2::Prog prog;
};


static inline re2jit::native *_compile(re2::Prog *prog)
{
    return (re2jit::native *) prog;
}


static inline void _destroy(re2jit::native *)
{
}


static inline rejit_entry_t _entry(re2jit::native *native)
{
    return native->prog.start();
}


static inline bool _run(re2jit::native *native, struct rejit_threadset_t *nfa)
{
    re2::Prog::Inst *op;
    ssize_t stack[256];
    ssize_t stkid = 0;
    int *capture;

    while (rejit_thread_dispatch(nfa)) {
        stkid = 0;
        stack[stkid++] = (ssize_t) nfa->running->entry;
        capture = nfa->running->groups;

        while (stkid--) {
            if (BIT_GET(nfa->states_visited, stack[stkid])) {
                continue;
            }

            BIT_SET(nfa->states_visited, stack[stkid]);
            op = native->prog.inst(stack[stkid]);

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
                        capture[op->cap()] = nfa->offset;
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
