#include <vector>
#include <unordered_set>
#define STACK_SIZE 1024


static thread_local const re2jit::native *_this;
struct re2jit::native
{
    const void * entry;
    const void * state;  // pointer to a `_number`
    re2::Prog  * _prog;
    // some insts are stack-allocated -- can't save pointers to their `out()` field.
    // instead, nfa state will point to elements of this vector.
    std::vector       <int> _numbers;
    std::unordered_set<int> _backrefs;

    native(re2::Prog *prog) : _prog(prog), _numbers(prog->size())
    {
        for (int i = 0; i < prog->size(); i++) {
            _numbers[i] = i;

            for (auto op : re2jit::is_extcode(prog, prog->inst(i)))
                if (op.opcode() == re2jit::inst::kBackReference)
                    _backrefs.insert(op.arg());
        }

        state = &_numbers[prog->start()];
        entry = (void *) &step;
    }

    void init() const
    {
        _this = this;
    }

    static void step(struct rejit_threadset_t *nfa, const void *inst)
    {
        int new_bitmaps = 0;
        struct rejit_thread_t *t = nfa->running;
        // state >= 0 -- visit another state
        // state <  0 -- restore a captuing group's boundary
        struct _st { int state, index, refd; };
        struct _st stack[STACK_SIZE];
        struct _st *it = stack;
        (it++)->state = *(const int *) inst;

        while (it != stack) {
            int i = (--it)->state;

            if (i < 0) {
                if (it->refd) {
                    rejit_thread_bitmap_restore(nfa);
                    new_bitmaps--;
                }

                t->groups[-i] = it->index;
                continue;
            }

            if (nfa->bitmap[i / 8] & (1 << (i % 8)))
                continue;

            nfa->bitmap[i / 8] |= 1 << (i % 8);

            auto op  = _this->_prog->inst(i);
            auto ext = re2jit::is_extcode(_this->_prog, op);

            for (auto& op : ext) switch (op.opcode())
            {
                case re2jit::inst::kUnicodeType: {
                    uint64_t x = rejit_read_utf8((const uint8_t *) nfa->input, nfa->length);

                    if (!x)
                        break;

                    uint8_t cls = rejit_unicode_category((uint32_t) x);

                    if ((cls & UNICODE_CATEGORY_GENERAL) != op.arg())
                        break;

                    rejit_thread_wait(nfa, &_this->_numbers[op.out()], x >> 32);
                    break;
                }

                case re2jit::inst::kBackReference: {
                    if (op.arg() * 2 >= nfa->groups)
                        break;

                    int start = t->groups[op.arg() * 2];
                    int end   = t->groups[op.arg() * 2 + 1];

                    if (start == -1 || end < start)
                        break;

                    if (start == end) {
                        (it++)->state = op.out();
                        break;
                    }

                    if (nfa->length < (size_t) (end - start))
                        break;

                    if (memcmp(nfa->input, nfa->input - nfa->offset + start, end - start))
                        break;

                    rejit_thread_wait(nfa, &_this->_numbers[op.out()], end - start);
                    break;
                }
            }

            if (!ext.size()) switch (op->opcode())
            {
                case re2::kInstAltMatch:
                case re2::kInstAlt:
                    (it++)->state = op->out1();
                    (it++)->state = op->out();
                    break;

                case re2::kInstByteRange: {
                    if (!nfa->length)
                        break;

                    uint8_t c = nfa->input[0];

                    if (op->foldcase() && 'A' <= c && c <= 'Z')
                        c += 'a' - 'A';

                    if (c < op->lo() || c > op->hi())
                        break;

                    rejit_thread_wait(nfa, &_this->_numbers[op->out()], 1);
                    break;
                }

                case re2::kInstCapture:
                    if ((size_t) op->cap() < nfa->groups && (size_t) t->groups[op->cap()] != nfa->offset) {
                        it->state = -op->cap();
                        it->index = t->groups[op->cap()];
                        it->refd  = _this->_backrefs.find(op->cap() / 2) != _this->_backrefs.end();

                        t->groups[op->cap()] = nfa->offset;

                        if (it->refd) {
                            rejit_thread_bitmap_save(nfa);
                            new_bitmaps++;
                        }

                        it++;
                    }

                    (it++)->state = op->out();
                    break;

                case re2::kInstEmptyWidth:
                    if (!(op->empty() & nfa->empty))
                        (it++)->state = op->out();
                    break;

                case re2::kInstNop:
                    (it++)->state = op->out();
                    break;

                case re2::kInstMatch:
                    if (rejit_thread_match(nfa)) {
                        // this match is preferred to whatever is still on stack.
                        // have to deallocate the new bitmaps, though.
                        while (new_bitmaps--) rejit_thread_bitmap_restore(nfa);
                        return;
                    }

                case re2::kInstFail:
                    break;
            }
        }
    }
};
