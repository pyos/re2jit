#include "debug.h"
#include "threads.h"


#define _IPTR(x) ((void *) (size_t) (x))


namespace re2jit {
    status it::run_nfa(const re2::StringPiece& text, RE2::Anchor anchor,
                             re2::StringPiece *match, int nmatch) const
    {
        auto *nfa = rejit_thread_init(text.data(), text.size(), _IPTR(_prog->start()),
            anchor == RE2::ANCHOR_START  ? RE2JIT_ANCHOR_START :
            anchor == RE2::ANCHOR_BOTH   ? RE2JIT_ANCHOR_START | RE2JIT_ANCHOR_END : 0,
            nmatch * 2 + 2);

        while (rejit_thread_dispatch(nfa, 1)) {
            re2::Prog::Inst *op = _prog->inst((ssize_t) nfa->running->entry);

            switch (op->opcode()) {
                case re2::kInstAlt:
                    rejit_thread_fork(nfa, _IPTR(op->out1()));
                    nfa->running->entry = _IPTR(op->out());
                    break;

                case re2::kInstAltMatch:
                    debug::write("re2jit::it: can't interpret kInstAltMatch\n");
                    rejit_thread_free(nfa);
                    return FAILED;

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
                    if (op->cap() < nmatch * 2 + 2) {
                        nfa->running->groups[op->cap()] = nfa->input - text.begin();
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
                    rejit_thread_match(nfa);
                    break;

                case re2::kInstFail:
                    rejit_thread_fail(nfa);
                    break;

                default:
                    debug::write("re2jit::it: unknown opcode %d\n", op->opcode());
                    rejit_thread_free(nfa);
                    return FAILED;
            }
        }

        int *gs = NULL, r = rejit_thread_result(nfa, &gs);

        for (int i = 0; i < nmatch; i++) {
            if (gs == NULL || gs[2 * i + 2] == -1)
                match[i].set((const char *) NULL, 0);
            else
                match[i].set(text.data() + gs[2 * i + 2], gs[2 * i + 3] - gs[2 * i + 2]);
        }

        debug::write("re2jit::it: interpreted with result %d\n", r);
        rejit_thread_free(nfa);
        return r == 0 ? REJECT : ACCEPT;
    }
};

#undef _IPTR
