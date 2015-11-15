#include <set>
#include <vector>


struct re2jit::native
{
    re2::Prog       *_prog;
    re2::Prog::Inst *state;
    std::size_t      space;  // = 1 bit per state
    std::set<int>    _backrefs;

    native(re2::Prog *prog) : _prog(prog)
                            , state(prog->inst(prog->start()))
                            , space((prog->size() + 7) / 8)
    {
        for (int i = 0; i < prog->size(); i++)
            for (auto op : re2jit::is_extcode(prog, prog->inst(i)))
                if (op.opcode() == re2jit::inst::kBackReference)
                    _backrefs.insert(op.arg());
    }

    static void entry(struct rejit_threadset_t *nfa, const void *state)
    {
        auto *st = (native *) nfa->data;
        auto *op = (re2::Prog::Inst *) state;
        auto i   = op->id(st->_prog);

        if (nfa->bitmap[i / 8] & (1 << (i % 8)))
            return;

        nfa->bitmap[i / 8] |= 1 << (i % 8);

        auto ext = re2jit::is_extcode(st->_prog, op);

        for (auto& op : ext) switch (op.opcode())
        {
            case re2jit::inst::kUnicodeGeneralType:
            case re2jit::inst::kUnicodeSpecificType: {
                uint64_t x = rejit_read_utf8((const uint8_t *) nfa->input, nfa->length);

                if (!x)
                    break;

                uint8_t cls = rejit_unicode_category((uint32_t) x);

                if (op.opcode() == re2jit::inst::kUnicodeGeneralType) {
                    if ((cls & UNICODE_CATEGORY_GENERAL) != op.arg())
                        break;
                }
                else if (cls != op.arg())
                    break;

                rejit_thread_wait(nfa, st->_prog->inst(op.out()), x >> 32);
                break;
            }

            case re2jit::inst::kBackReference: {
                if (op.arg() * 2 >= nfa->groups)
                    break;

                int start = nfa->running->groups[op.arg() * 2];
                int end   = nfa->running->groups[op.arg() * 2 + 1];

                if (start == -1 || end < start)
                    break;

                if (start == end) {
                    entry(nfa, st->_prog->inst(op.out()));
                    break;
                }

                if (nfa->length < (size_t) (end - start))
                    break;

                if (memcmp(nfa->input, nfa->input - nfa->offset + start, end - start))
                    break;

                rejit_thread_wait(nfa, st->_prog->inst(op.out()), end - start);
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
                int restore = nfa->running->groups[op->cap()];

                if ((size_t) op->cap() >= nfa->groups || (size_t) restore == nfa->offset) {
                    entry(nfa, st->_prog->inst(op->out()));
                    break;
                }

                bool refd = st->_backrefs.find(op->cap() / 2) != st->_backrefs.end();

                nfa->running->groups[op->cap()] = nfa->offset;
                if (refd)
                    rejit_thread_bitmap_save(nfa);

                entry(nfa, st->_prog->inst(op->out()));

                nfa->running->groups[op->cap()] = restore;
                if (refd)
                    rejit_thread_bitmap_restore(nfa);

                break;
            }

            case re2::kInstMatch:
                rejit_thread_match(nfa);
                break;

            case re2::kInstEmptyWidth:
                if (op->empty() & nfa->empty)
                    break;

            case re2::kInstNop:
                entry(nfa, st->_prog->inst(op->out()));

            case re2::kInstFail:
                break;
        }
    }
};
