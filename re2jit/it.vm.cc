#include <set>
#include <vector>

#if RE2JIT_ENABLE_SUBROUTINES
#include <map>
#endif


struct re2jit::native
{
    re2::Prog       *_prog;
    re2::Prog::Inst *state;
    std::size_t      space;  // = 1 bit per state
    std::set<int>    _backrefs;

    #if RE2JIT_ENABLE_SUBROUTINES
    std::map<unsigned, int> _subcalls;
    #endif

    native(re2::Prog *prog) : _prog(prog)
                            , state(prog->inst(prog->start()))
                            , space((prog->size() + 7) / 8)
    {
        for (int i = 0; i < prog->size(); i++) {
            for (auto op : re2jit::get_extcode(prog, prog->inst(i))) {
                if (op.opcode == re2jit::kBackreference)
                    _backrefs.insert(op.arg);

                #if RE2JIT_ENABLE_SUBROUTINES
                else if (op.opcode == re2jit::kSubroutine) {
                    int i = 0;

                    for (; i < prog->size(); i++) {
                        auto inst = prog->inst(i);

                        if (inst->opcode() == re2::kInstCapture && inst->cap() % 2 == 0
                                                                && inst->cap() / 2 == op.arg)
                            break;
                    }

                    if (i == prog->size()) {
                        // fatal: invalid group id
                        state = NULL;
                        return;
                    }

                    _subcalls.insert({ op.arg, i });
                }
                #endif
            }
        }
    }

    static void entry(struct rejit_threadset_t *nfa, const void *state)
    {
        auto *st = (native *) nfa->data;
        auto *op = (re2::Prog::Inst *) state;
        auto i   = op->id(st->_prog);

        if (nfa->bitmap[i / 8] & (1 << (i % 8)))
            return;

        nfa->bitmap[i / 8] |= 1 << (i % 8);

        auto ext = re2jit::get_extcode(st->_prog, op);

        for (auto& op : ext) switch (op.opcode)
        {
            case re2jit::kUnicodeTypeGeneral:
            case re2jit::kUnicodeTypeSpecific:
            case re2jit::kUnicodeTypeGeneralNegated:
            case re2jit::kUnicodeTypeSpecificNegated: {
                uint32_t x = rejit_read_utf8((const uint8_t *) nfa->input, nfa->length);

                if (!x)
                    break;

                uint8_t cls = rejit_unicode_category(x);

                if (op.opcode == re2jit::kUnicodeTypeGeneral)
                    cls &= UNICODE_CATEGORY_GENERAL;

                bool neg = op.opcode == re2jit::kUnicodeTypeGeneralNegated
                        || op.opcode == re2jit::kUnicodeTypeSpecificNegated;

                if ((cls != op.arg) ^ neg)
                    break;

                rejit_thread_wait(nfa, st->_prog->inst(op.out), x >> 24);
                break;
            }

            #if RE2JIT_ENABLE_SUBROUTINES
            case re2jit::kSubroutine: {
                rejit_thread_subcall_push(nfa, st->_prog->inst(st->_subcalls[op.arg]),
                                               st->_prog->inst(op.out), op.arg);
                break;
            }
            #endif

            case re2jit::kBackreference: {
                if (op.arg * 2 >= nfa->groups)
                    break;

                int start = nfa->running->groups[op.arg * 2];
                int end   = nfa->running->groups[op.arg * 2 + 1];

                if (start == -1 || end < start)
                    break;

                if (start == end) {
                    entry(nfa, st->_prog->inst(op.out));
                    break;
                }

                if (nfa->length < (size_t) (end - start))
                    break;

                if (memcmp(nfa->input, nfa->input - nfa->offset + start, end - start))
                    break;

                rejit_thread_wait(nfa, st->_prog->inst(op.out), end - start);
                break;
            }
        }

        if (!ext.size()) switch (op->opcode())
        {
            case re2::kInstAltMatch:
            case re2::kInstAlt:
                entry(nfa, st->_prog->inst(op->out()));
                entry(nfa, st->_prog->inst(op->out1()));
                break;

            case re2::kInstByteRange: {
                if (!nfa->length)
                    break;

                uint8_t c = nfa->input[0];

                if (op->foldcase() && 'A' <= c && c <= 'Z')
                    c += 'a' - 'A';

                if (c < op->lo() || c > op->hi())
                    break;

                rejit_thread_wait(nfa, st->_prog->inst(op->out()), 1);
                break;
            }

            case re2::kInstCapture: {
                unsigned restore = nfa->running->groups[op->cap()];

                if ((unsigned) op->cap() >= nfa->groups || restore == nfa->offset) {
                    entry(nfa, st->_prog->inst(op->out()));
                    break;
                }

                nfa->running->groups[op->cap()] = nfa->offset;

                #if RE2JIT_ENABLE_SUBROUTINES
                if (op->cap() % 2 == 0 || rejit_thread_subcall_pop(nfa, op->cap() / 2)) {
                #endif

                bool refd = st->_backrefs.find(op->cap() / 2) != st->_backrefs.end();

                if (refd)
                    rejit_thread_bitmap_save(nfa);

                entry(nfa, st->_prog->inst(op->out()));

                if (refd)
                    rejit_thread_bitmap_restore(nfa);

                #if RE2JIT_ENABLE_SUBROUTINES
                }
                #endif

                nfa->running->groups[op->cap()] = restore;
                break;
            }

            case re2::kInstMatch:
                rejit_thread_match(nfa);
                break;

            case re2::kInstEmptyWidth:
                if (!rejit_thread_satisfies(nfa, (enum RE2JIT_EMPTY_FLAGS) op->empty()))
                    break;

            case re2::kInstNop:
                entry(nfa, st->_prog->inst(op->out()));

            case re2::kInstFail:
                break;
        }
    }
};
