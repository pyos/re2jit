#include <vector>

#define STACK_SIZE 1024


struct re2jit::native
{
    void * entry;
    void * state;
    re2::Prog *_prog;
    // In this backend, a state is a pointer to an instruction counter.
    // Since we can't make a pointer to a stack-allocated object's field
    // (as it's going to be destroyed soon), we'll preallocate an array
    // of possible values and refer to these.
    std::vector<int64_t> _numbers;

    native(re2::Prog *prog) : _prog(prog), _numbers(prog->size())
    {
        for (ssize_t i = 0; i < prog->size(); i++)
            _numbers[i] = i;

        state = &_numbers[prog->start()];
        entry = NULL;
     // entry = ??? need to somehow bind `step` to `this`
    }

    void step(struct rejit_threadset_t *nfa, void *inst)
    {
        struct rejit_thread_t *t = nfa->running;

        union {
            int64_t state;
            // state >= 0 -- visit another state
            // state <  0 -- restore a captuing group's boundary
            struct {
                int32_t group;
                int32_t index;
            };
        } stack[STACK_SIZE];

        stack[0].state = *(int64_t *) inst;

        for (size_t stkid = 1; stkid; stkid--) {
            int64_t i = stack[stkid].state;

            if (i < 0) {
                t->groups[-stack[stkid].group] = stack[stkid].index;
                continue;
            }

            if (nfa->visited[i / 8] & (1 << (i % 8)))
                continue;

            nfa->visited[i / 8] |= 1 << (i % 8);

            auto op  = _prog->inst(i);
            auto vec = re2jit::is_extcode(_prog, op);

            for (auto& op : vec) switch (op.opcode())
            {
                case re2jit::inst::kUnicodeType: {
                    uint64_t x = rejit_read_utf8((const uint8_t *) nfa->input, nfa->length);

                    if (!x)
                        break;

                    uint8_t cls = rejit_unicode_category((uint32_t) x);

                    if ((cls & UNICODE_CATEGORY_GENERAL) != op.arg())
                        break;

                    rejit_thread_wait(nfa, &_numbers[op.out()], x >> 32);
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

                    rejit_thread_wait(nfa, &_numbers[op->out()], 1);
                    break;
                }

                case re2::kInstCapture:
                    if ((size_t) op->cap() < nfa->groups) {
                        stack[stkid].group = -op->cap();
                        stack[stkid].index = t->groups[op->cap()];
                        stkid++;

                        t->groups[op->cap()] = nfa->offset;
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
                    if (rejit_thread_match(nfa))
                        // this match is preferred to whatever is still on stack.
                        return;
                    break;

                case re2::kInstFail:
                    break;
            }
        }
    }
};
