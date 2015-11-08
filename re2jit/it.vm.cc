#include <vector>
#include <unordered_set>
#define STACK_SIZE 1024


static thread_local re2jit::native *_this;
struct re2jit::native
{
    void * entry;
    void * state;
    re2::Prog *_prog;
    // In this backend, a state is a pointer to an instruction counter.
    // Since we can't make a pointer to a stack-allocated object's field
    // (as it's going to be destroyed soon), we'll preallocate an array
    // of possible values and refer to these.
    std::vector<int32_t> _numbers;
    std::unordered_set<uint32_t> _backrefs;

    native(re2::Prog *prog) : _prog(prog), _numbers(prog->size())
    {
        for (ssize_t i = 0; i < prog->size(); i++) {
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
        _this = const_cast<re2jit::native *>(this);
    }

    static void step(struct rejit_threadset_t *nfa, void *inst)
    {
        struct rejit_thread_t *t = nfa->running;

        union {
            // state >= 0 -- visit another state
            // state <  0 -- restore a captuing group's boundary
            struct { int32_t state, _____; };
            struct { int32_t group, index; };
        } stack[STACK_SIZE];

        stack[0].state = *(int32_t *) inst;

        int32_t new_bitmaps = 0;
        rejit_thread_bitmap_clear(nfa);

        for (size_t stkid = 1; stkid;) {
            int32_t i = stack[--stkid].state;

            if (i < 0) {
                if (_this->_backrefs.find(-i / 2) != _this->_backrefs.end()) {
                    rejit_thread_bitmap_restore(nfa);
                    new_bitmaps--;
                }

                t->groups[-i] = stack[stkid].index;
                continue;
            }

            if (nfa->visited[i / 8] & (1 << (i % 8)))
                continue;

            nfa->visited[i / 8] |= 1 << (i % 8);

            auto op  = _this->_prog->inst(i);
            auto vec = re2jit::is_extcode(_this->_prog, op);

            for (auto& op : vec) switch (op.opcode())
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
                        // no idea what that group matched
                        break;

                    int start = t->groups[op.arg() * 2];
                    int end   = t->groups[op.arg() * 2 + 1];

                    if (start == -1 || end < start)
                        // unmatched group.
                        break;

                    if (start == end) {
                        stack[stkid++].state = op.out();
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

            if (!vec.size()) switch (op->opcode())
            {
                case re2::kInstAltMatch:
                case re2::kInstAlt:
                    stack[stkid++].state = op->out1();
                    stack[stkid++].state = op->out();
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
                    if ((size_t) op->cap() < nfa->groups) {
                        stack[stkid].group = -op->cap();
                        stack[stkid].index = t->groups[op->cap()];
                        stkid++;

                        t->groups[op->cap()] = nfa->offset;

                        if (_this->_backrefs.find(op->cap() / 2) != _this->_backrefs.end()) {
                            rejit_thread_bitmap_save(nfa);
                            new_bitmaps++;
                        }
                    }

                    stack[stkid++].state = op->out();
                    break;

                case re2::kInstEmptyWidth:
                    if (!(op->empty() & ~(nfa->empty)))
                        stack[stkid++].state = op->out();
                    break;

                case re2::kInstNop:
                    stack[stkid++].state = op->out();
                    break;

                case re2::kInstMatch:
                    if (rejit_thread_match(nfa)) {
                        // this match is preferred to whatever is still on stack.
                        // have to deallocate the new bitmaps, though.
                        while (new_bitmaps--) rejit_thread_bitmap_restore(nfa);
                        return;
                    }

                    break;

                case re2::kInstFail:
                    break;
            }
        }
    }
};
