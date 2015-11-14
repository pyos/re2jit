#include <set>
#include <vector>
#include <sys/mman.h>

#include "asm64.h"

// `&NFA->input` -- like offsetof, but shorter and with 100% more undefined behavior.
static constexpr const struct rejit_threadset_t *NFA    = NULL;
static constexpr const struct rejit_thread_t    *THREAD = NULL;

struct re2jit::native
{
    void init() {}
    const void * entry = NULL;
    const void * state = NULL;
    size_t _size = 0;

    native(re2::Prog *prog)
    {
        std::vector<unsigned> stack(prog->size());
        unsigned *it;
        unsigned *visited;
        #define DFS(count) it      = &stack[0]; \
                           visited = &count[0]; \
                           visited[*it++ = prog->start()]++; \
                           while (it-- != &stack[0])
        #define VISIT(i) if (!visited[i]++) *it++ = i

        // dead code elimination pass:
        //   1. ignore all instructions with indegree = 0
        //   2. do not emit bitmap checks for those with indegree = 1
        //   3. find out changing which groups would require resetting the state bitmap
        std::vector<unsigned> indegree(prog->size());
        std::set   <unsigned> backrefs;

        DFS(indegree) {
            auto op  = prog->inst(*it);
            auto ext = re2jit::is_extcode(prog, op);

            for (auto& op : ext) switch (op.opcode()) {
                case re2jit::inst::kBackReference:
                    backrefs.insert(op.arg());

                case re2jit::inst::kUnicodeType:
                    VISIT(op.out());
            }

            if (!ext.size()) switch (op->opcode()) {
                case re2::kInstAlt:
                case re2::kInstAltMatch:
                    VISIT(op->out1());

                default:
                    VISIT(op->out());

                case re2::kInstFail:
                case re2::kInstMatch:
                    break;

                case re2::kInstByteRange:
                    size_t i;

                    do  // non-extcode byte ranges are concatenated into one block of code
                        // => intermediate insts are unreachable by themselves
                        op = prog->inst(i = op->out());
                    while (op->opcode() == re2::kInstByteRange && !re2jit::maybe_extcode(op));

                    VISIT(i);
            }
        }

        // compiler pass:
        //   emitted code is an `int(struct rejit_threadset_t* rdi, const void* rsi)`.
        //   states are opcodes; each is an `int(struct rejit_threadset_t* rdi)`.
        //   return value is 1 iff a matching state is reachable through epsilon transitions.
        as::code  code;
        as::label fail, fail_likely, /* always unlikely: */ succeed;
        std::vector<as::label> labels(prog->size());
        std::vector<unsigned> emitted(prog->size());
        // Intel Optimization Reference Manual, page 3-9, 3.4.1.6: Branch Selection
        //   ...if indirect branches are common but they cannot be predicted by branch
        //      prediction hardware, then follow the indirect branch with a UD2
        //      instruction, which will stop the processor from decoding down the
        //      fall-through path.
        code.jmp(as::rsi).ud2();
        // Intel Optimization Reference Manual, page 3-6, 3.4.1.3: Static Prediction
        //   ...make the fall-through code following a conditional branch be the unlikely
        //      target for a branch with a backward target.
        code.mark(fail_likely).xor_(as::eax, as::eax).ret();

        DFS(emitted) {
            auto op  = prog->inst(*it);
            auto ext = re2jit::is_extcode(prog, op);
            as::label next_extcode;

            code.mark(labels[*it]);

            // kInstFail will do `ret` anyway.
            if (op->opcode() != re2::kInstFail && indegree[*it] > 1)
                // if (bit(nfa->bitmap, *it) == 1) return; bit(nfa->bitmap, *it) = 1;
                code.mov  (as::mem(as::rdi + &NFA->bitmap), as::rsi)
                    .test (as::i8(1 << (*it % 8)), as::mem(as::rsi + *it / 8)).jmp(fail_likely, as::not_zero)
                    .or_  (as::i8(1 << (*it % 8)), as::mem(as::rsi + *it / 8));

            for (auto op = ext.crbegin(); op != ext.crend(); ++op) {
                code.mark(next_extcode);

                if (op != ext.crend() - 1)
                    code.push(as::rdi)
                        .call(next_extcode = as::label())
                        .pop (as::rdi);

                switch (op->opcode()) {
                    case re2jit::inst::kUnicodeType:
                        // chr = rejit_read_utf8(nfa->input, nfa->length);
                        code.push (as::rdi)
                            .mov  (as::mem(as::rdi + &NFA->length), as::rsi)
                            .mov  (as::mem(as::rdi + &NFA->input),  as::rdi)
                            .call (&rejit_read_utf8)
                            .pop  (as::rdi)
                        // if ((len = chr >> 32) == 0) return;
                            .mov  (as::rax, as::rdx)
                            .shr  (32,      as::rdx).jmp(fail, as::zero)
                        // if ((rejit_unicode_category(chr) & UNICODE_CATEGORY_GENERAL) != arg) return;
                            .push (as::rdi)
                            .push (as::rdx)
                            .mov  (as::eax, as::edi)
                            .call (&rejit_unicode_category)  // inlining is hard.
                            .pop  (as::rdx)
                            .pop  (as::rdi)
                            .and_ (UNICODE_CATEGORY_GENERAL, as::al)
                            .cmp  (op->arg(), as::al).jmp(fail, as::not_equal)
                        // return rejit_thread_wait(nfa, &out, len);
                            .mov  (labels[op->out()], as::rsi)
                            .jmp  (&rejit_thread_wait);
                        VISIT(op->out());
                        break;

                    case re2jit::inst::kBackReference:
                        // if (nfa->groups <= arg * 2) return;
                        code.cmp(as::i32(op->arg() * 2), as::mem(as::rdi + &NFA->groups))
                            .jmp(fail, as::less_equal_u)
                        // if (start < 0 || end < start) return; if (end == start) goto empty;
                            .mov (as::mem(as::rdi + &NFA->running), as::rsi)
                            .mov (as::mem(as::rsi + &THREAD->groups[op->arg() * 2 + 1]), as::ecx)
                            .mov (as::mem(as::rsi + &THREAD->groups[op->arg() * 2]),     as::esi)
                            .test(as::esi, as::esi).jmp(fail, as::negative)
                            .sub (as::esi, as::ecx).jmp(fail, as::less)
                            .jmp (labels[op->out()], as::equal)
                        // if (nfa->length < end - start) return;
                            .cmp(as::rcx, as::mem(as::rdi + &NFA->length)).jmp(fail, as::less_u)
                        // if (memcmp(nfa->input, nfa->input + start - nfa->offset, end - start)) return;
                            .push(as::rdi)
                            .sub (as::mem(as::rdi + &NFA->offset), as::rsi)
                            .mov (as::mem(as::rdi + &NFA->input),  as::rdi)
                            .add (as::rdi, as::rsi)
                            .mov (as::ecx, as::edx)
                            .repz().cmpsb().pop(as::rdi).jmp(fail, as::not_equal)
                        // return rejit_thread_wait(nfa, &out, end - start);
                            .mov (labels[op->out()], as::rsi)
                            .jmp (&rejit_thread_wait);
                        VISIT(op->out());
                        break;
                }
            }

            if (!ext.size()) switch (op->opcode()) {
                case re2::kInstAltMatch:
                case re2::kInstAlt:
                    // if (out(nfa)) return 1;
                    code.push  (as::rdi)
                        .call  (labels[op->out()])
                        .pop   (as::rdi)
                        .test  (as::eax, as::eax).jmp(succeed, as::not_zero);
                    VISIT(op->out());
                    VISIT(op->out1()); else code.jmp(labels[op->out1()]);
                    break;

                case re2::kInstByteRange: {
                    auto i = 0, len = 0, r = 0;
                    auto end = op;

                    do
                        len++, end = prog->inst(r = end->out());
                    while (end->opcode() == re2::kInstByteRange && !re2jit::maybe_extcode(end));

                    // if (nfa->length < len) return; else rsi = nfa->input;
                    code.cmp(as::i32(len), as::mem(as::rdi + &NFA->length)).jmp(fail, as::less_u)
                        .mov(as::mem(as::rdi + &NFA->input), as::rsi);

                    do {
                        // Intel Optimization Reference Manual, page 3-26, 3.5.1.8:
                        //   Break dependencies on portions of registers [...] by operating
                        //   on 32-bit registers instead of partial registers. For moves,
                        //   this can be accomplished [...] by using MOVZX.
                        // al = rsi[i];
                        code.movzb(as::mem(as::rsi + i++), as::eax);

                        if (op->foldcase())
                            // if ('A' <= al && al <= 'Z') al = al - 'A' + 'a';
                            code.mov(as::rax - 'A' + 'a', as::edx)
                                .mov(as::rax - 'A',       as::ecx)
                                .cmp('Z' - 'A', as::cl)
                                .mov(as::edx, as::eax, as::less_equal_u);

                        if (op->hi() == op->lo())
                            // if (al != lo) return;
                            code.cmp(op->lo(), as::al).jmp(fail_likely, as::not_equal);
                        else
                            // if (al < lo || hi < al) return;
                            code.sub(op->lo(),            as::al)
                                .cmp(op->hi() - op->lo(), as::al).jmp(fail_likely, as::more_u);

                        op = prog->inst(op->out());
                    } while (op != end);

                    // return rejit_thread_wait(nfa, &out, len);
                    code.mov(labels[r], as::rsi)
                        .mov(len,       as::edx)
                        .jmp(&rejit_thread_wait);
                    VISIT(r);
                    break;
                }

                case re2::kInstCapture:
                    // if (nfa->groups <= cap) goto out;
                    code.cmp  (as::i32(op->cap()), as::mem(as::rdi + &NFA->groups))
                        .jmp  (labels[op->out()], as::less_equal_u)
                    // if ((edx = nfa->running->groups[cap]) == nfa->offset) goto out;
                        .mov  (as::mem(as::rdi + &NFA->running), as::rcx)
                        .mov  (as::mem(as::rdi + &NFA->offset),  as::eax)
                        .mov  (as::mem(as::rcx + &THREAD->groups[op->cap()]), as::edx)
                        .cmp  (as::eax, as::edx).jmp(labels[op->out()], as::equal)
                    // nfa->running->groups[cap] = nfa->offset;
                        .push (as::rdx)
                        .push (as::rcx)
                        .mov  (as::eax, as::mem(as::rcx + &THREAD->groups[op->cap()]));

                    if (backrefs.find(op->cap() / 2) != backrefs.end())
                        code.push(as::rdi).call(&rejit_thread_bitmap_save)   .pop(as::rdi)
                            .push(as::rdi).call(labels[op->out()])           .pop(as::rdi)
                            .push(as::rax).call(&rejit_thread_bitmap_restore).pop(as::rax);
                    else
                        code.call(labels[op->out()]);

                    // nfa->running->groups[cap] = edx; return eax;
                    code.pop(as::rcx)
                        .pop(as::rdx)
                        .mov(as::edx, as::mem(as::rcx + &THREAD->groups[op->cap()]))
                        .ret();
                    VISIT(op->out());
                    break;

                case re2::kInstEmptyWidth:
                    // if (nfa->empty & empty) return;
                    code.test(as::i8(op->empty()), as::mem(as::rdi + &NFA->empty))
                        .jmp (fail, as::not_zero);
                    VISIT(op->out()); else code.jmp(labels[op->out()]);
                    break;

                case re2::kInstNop:
                    VISIT(op->out()); else code.jmp(labels[op->out()]);
                    break;

                case re2::kInstMatch:
                    code.jmp(&rejit_thread_match);
                    break;

                case re2::kInstFail:
                    code.ret();
                    break;
            }
        }

        code.mark(fail).xor_(as::eax, as::eax).mark(succeed).ret();
        #undef VISIT
        #undef DFS

        void *m = mmap(NULL, code.size(), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

        if (m == MAP_FAILED)
            return;

        entry = m;
        _size = code.size();

        if (!code.write(m) || mprotect(m, _size, PROT_READ | PROT_EXEC) == -1)
            return;

        state = labels[prog->start()](m);
    }

   ~native()
    {
        munmap((void *) entry, _size);
    }
};
