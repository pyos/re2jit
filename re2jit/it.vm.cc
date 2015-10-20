#include "it.h"
#include "debug.h"
#include "threads.h"
#include "rewriter.h"
#define STACK_SIZE 1024


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

                if (nfa->visited[i / 8] & (1 << (i % 8))) {
                    continue;
                }

                nfa->visited[i / 8] |= 1 << (i % 8);

                #if !RE2JIT_NO_EXTCODES
                    std::vector<re2jit::fake_inst> fs = re2jit::is_extcode(_prog, i);

                    if (fs.size()) for (auto f : fs) {
                        if (f >= re2jit::MATCH_UNICODE_CLASS_START && f <= re2jit::MATCH_UNICODE_CLASS_END) {
                            rejit_uni_char_t c;

                            int len = rejit_read_utf8((const uint8_t *) nfa->input, nfa->length, &c);
                            if (len == -1)
                                // not a valid utf-8 character
                                continue;

                            // TODO check the class of `c`
                            rejit_thread_wait(nfa, f.out, len);
                        }

                        else {
                            re2jit::debug::write("re2jit::vm: unsupported extcode %hu\n", f.op);
                        }

                        continue;
                    }
                #endif

                op = _prog->inst(i);

                switch (op->opcode()) {
                    case re2::kInstAltMatch:
                    case re2::kInstAlt:
                        stack[stkid++] = op->out1();
                        stack[stkid++] = op->out();
                        break;

                    case re2::kInstByteRange: {
                        if (!nfa->length) {
                            break;
                        }

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
