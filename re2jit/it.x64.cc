#include <set>
#include <vector>
#include <sys/mman.h>

#include "asm.h"

// `&NFA->input` -- like offsetof, but with 100% more undefined behavior.
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
        size_t n = prog->size();
        // dead code elimination pass:
        //   1. ignore all instructions with indegree = 0
        //   2. do not emit bitmap checks for those with indegree = 1
        //   3. find out changing which groups would require resetting the state bitmap
        std::vector<unsigned> stack(n);
        std::vector<unsigned> emitted(n);
        std::vector<unsigned> indegree(n);
        std::set   <unsigned> backrefs;
        auto it = stack.begin();
        #define VISIT(a, i) if (!a[i]++) *it++ = i

        VISIT(indegree, prog->start());

        while (it != stack.begin()) {
            auto op  = prog->inst(*--it);
            auto ext = re2jit::is_extcode(prog, op);

            for (auto& op : ext) switch (op.opcode()) {
                case re2jit::inst::kBackReference:
                    backrefs.insert(op.arg());

                case re2jit::inst::kUnicodeType:
                    VISIT(indegree, op.out());
            }

            if (!ext.size()) switch (op->opcode()) {
                case re2::kInstAlt:
                case re2::kInstAltMatch:
                    VISIT(indegree, op->out1());

                default:
                    VISIT(indegree, op->out());

                case re2::kInstFail:
                case re2::kInstMatch:
                    break;

                case re2::kInstByteRange:
                    size_t i;

                    do  // non-extcode byte ranges are concatenated into one block of code
                        // => intermediate insts are unreachable by themselves
                        op = prog->inst(i = op->out());
                    while (op->opcode() == re2::kInstByteRange && !re2jit::maybe_extcode(op));

                    VISIT(indegree, i);
            }
        }

        as::code code;
        as::label fail;
        as::label succeed;
        std::vector<as::label> labels(n);
        // compiler pass:
        //   1. 0(code) -- entry point; %rdi = struct rejit_threadset_t *nfa, %rsi = void *state
        //   2. (state) -- some opcode; %rdi = struct rejit_threadset_t *nfa
        code.jmp(as::rsi).mark(fail).xor_(as::eax, as::eax).mark(succeed).ret();

        VISIT(emitted, prog->start());

        while (it != stack.begin()) {
            auto op  = prog->inst(*--it);
            auto ext = re2jit::is_extcode(prog, op);

            code.mark(labels[*it]);

            // kInstFail will do `ret` anyway.
            if (op->opcode() != re2::kInstFail && indegree[*it] > 1)
                // if (bit(nfa->bitmap, *it) == 1) return; bit(nfa->bitmap, *it) = 1;
                code.mov  (as::mem(as::rdi + &NFA->bitmap), as::rsi)
                    .test (as::i8(1 << (*it % 8)), as::mem(as::rsi + *it / 8)).jmp(fail, as::not_zero)
                    .or_  (as::i8(1 << (*it % 8)), as::mem(as::rsi + *it / 8));

            for (auto &op : ext) {
                // a match at any opcode in `ext` is ok, so instead of straight up
                // failing, jump to the next one.
                as::label fail;

                switch (op.opcode()) {
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
                            .cmp  (op.arg(), as::al).jmp(fail, as::not_equal)
                        // rejit_thread_wait(nfa, &out, len);
                            .mov  (labels[op.out()], as::rsi)
                            .push (as::rdi)
                            .call (&rejit_thread_wait)
                            .pop  (as::rdi)
                            .jmp  (fail);
                        VISIT(emitted, op.out());
                        break;

                    case re2jit::inst::kBackReference: {
                        as::label empty;

                        // if (nfa->groups <= arg * 2) return;
                        code.cmp(as::i32(op.arg() * 2), as::mem(as::rdi + &NFA->groups))
                            .jmp(fail, as::less_equal_u)
                        // if (start < 0 || end < start) return; if (end == start) goto empty;
                            .mov (as::mem(as::rdi + &NFA->running), as::rsi)
                            .mov (as::mem(as::rsi + &THREAD->groups[op.arg() * 2 + 1]), as::ecx)
                            .mov (as::mem(as::rsi + &THREAD->groups[op.arg() * 2]),     as::esi)
                            .test(as::esi, as::esi).jmp(fail, as::negative)
                            .sub (as::esi, as::ecx).jmp(fail, as::less)
                            .jmp (empty, as::equal)
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
                            .mov (labels[op.out()], as::rsi)
                            .push(as::rdi)
                            .call(&rejit_thread_wait)
                            .pop (as::rdi)
                            .jmp (fail)
                        // empty: eax = out(nfa);
                            .mark(empty)
                            .push(as::rdi)
                            .call(labels[op.out()])
                            .pop (as::rdi);
                        VISIT(emitted, op.out());
                        break;
                    }
                }

                code.test(as::eax, as::eax).jmp(succeed, as::not_zero).mark(fail);
            }

            if (ext.size()) code.jmp(fail); else switch (op->opcode()) {
                case re2::kInstAltMatch:
                case re2::kInstAlt:
                    // if (out(nfa)) return 1;
                    code.push  (as::rdi)
                        .call  (labels[op->out()])
                        .pop   (as::rdi)
                        .test  (as::eax, as::eax).jmp(succeed, as::not_zero);
                    VISIT(emitted, op->out());
                    VISIT(emitted, op->out1()); else code.jmp(labels[op->out1()]);
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
                        // al = rsi[i];
                        code.mov(as::mem(as::rsi + i++), as::al);

                        if (op->foldcase())
                            // if ('A' <= al && al <= 'Z') al = al - 'A' + 'a';
                            code.mov(as::rax - 'A' + 'a', as::edx)
                                .mov(as::rax - 'A',       as::ecx)
                                .cmp('Z' - 'A', as::cl)
                                .mov(as::edx, as::eax, as::less_equal_u);

                        if (op->hi() == op->lo())
                            // if (al != lo) return;
                            code.cmp(op->lo(), as::al).jmp(fail, as::not_equal);
                        else
                            // if (al < lo || hi < al) return;
                            code.sub(op->lo(),            as::al)
                                .cmp(op->hi() - op->lo(), as::al).jmp(fail, as::more_u);

                        op = prog->inst(op->out());
                    } while (op != end);

                    // return rejit_thread_wait(nfa, &out, len);
                    code.mov(labels[r], as::rsi)
                        .mov(len,       as::edx)
                        .jmp(&rejit_thread_wait);
                    VISIT(emitted, r);
                    break;
                }

                case re2::kInstCapture:
                    // if (nfa->groups <= cap) goto out;
                    code.cmp  (as::i32(op->cap()), as::mem(as::rdi + &NFA->groups))
                        .jmp  (labels[op->out()], as::less_equal_u)
                    // push(nfa->running->groups[cap]); nfa->running->groups[cap] = nfa->offset;
                        .mov  (as::mem(as::rdi + &NFA->running), as::rcx)
                        .mov  (as::mem(as::rdi + &NFA->offset),  as::eax)
                        .push (as::mem(as::rcx + &THREAD->groups[op->cap()]))
                        .push (as::rcx)
                        .mov  (as::eax, as::mem(as::rcx + &THREAD->groups[op->cap()]));

                    if (backrefs.find(op->cap() / 2) != backrefs.end())
                        // rejit_thread_bitmap_save(nfa); eax = out(nfa);
                        // rejit_thread_bitmap_restore(nfa);
                        code.push (as::rdi).call(&rejit_thread_bitmap_save)    .pop(as::rdi)
                            .push (as::rdi).call(labels[op->out()])            .pop(as::rdi)
                            .push (as::rax).call(&rejit_thread_bitmap_restore) .pop(as::rax);
                    else
                        // eax = out(nfa);
                        code.call(labels[op->out()]);

                    // pop(nfa->running->groups[cap]); return eax;
                    code.pop(as::rcx)
                        .pop(as::mem(as::rcx + &THREAD->groups[op->cap()]))
                        .jmp(succeed);
                    VISIT(emitted, op->out());
                    break;

                case re2::kInstEmptyWidth:
                    // if (nfa->empty & empty) return;
                    code.test(as::i8(op->empty()), as::mem(as::rdi + &NFA->empty))
                        .jmp (fail, as::not_zero);
                    VISIT(emitted, op->out()); else code.jmp(labels[op->out()]);
                    break;

                case re2::kInstNop:
                    VISIT(emitted, op->out()); else code.jmp(labels[op->out()]);
                    break;

                case re2::kInstMatch:
                    code.jmp(&rejit_thread_match);
                    break;

                case re2::kInstFail:
                    code.ret();
                    break;
            }
        }

        #undef VISIT

        void *m = mmap(NULL, code.size(), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

        if (m == (void *) -1)
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
