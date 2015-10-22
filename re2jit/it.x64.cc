#include <vector>
#include <sys/mman.h>

#include "it.x64.asm.h"


static const struct rejit_threadset_t *NFA    = NULL;  // used for offset calculation
static const struct rejit_thread_t    *THREAD = NULL;

struct re2jit::native
{
    void *_code;
    void *_entry;
    size_t _size;

    native(re2::Prog *prog) : _code(NULL), _entry(NULL), _size(0)
    {
        size_t i;
        size_t n = prog->size();

        as code;
        std::vector< as::label* > labels(n);

        for (auto& ref : labels) ref = &code.mark();

        // If the program starts with a failing opcode, we can use that to redirect
        // all conditional rets instead of jumping over them.
        as::label& fail    = code.mark();
        as::label& succeed = code.mark();
        code.xor_(as::eax, as::eax).mark(succeed).ret();

        // How many transitions have the i-th opcode as a target. We have to maintain
        // a bit vector of visited states to avoid going into an infinite loop; however,
        // opcodes with indegree 1 can never be at the start of a loop, so we can avoid
        // some memory lookups on these.
        std::vector< unsigned > indegree(n);

        ssize_t *stack = new ssize_t[prog->size()];
        ssize_t *stptr = stack;
        indegree[*stptr++ = prog->start()]++;

        while (stptr != stack)
            RE2JIT_WITH_INST(op, prog, *--stptr,
                // re2jit::inst *
                if (!indegree[op->out()]++)
                    *stptr++ = op->out();

                // re2::Prog::Inst *
              , switch (op->opcode()) {
                    case re2::kInstAlt:
                    case re2::kInstAltMatch:
                        if (!indegree[op->out1()]++)
                            *stptr++ = op->out1();

                    default:
                        if (!indegree[op->out()]++)
                            *stptr++ = op->out();

                    case re2::kInstFail:
                    case re2::kInstMatch:
                        break;
                }
            );

        delete[] stack;

        for (i = 0; i < n; i++) if (indegree[i]) {
            code.mark(*labels[i]);
            // Each opcode should conform to System V ABI calling convention.
            //   %rdi = struct rejit_threadset_t *nfa -- argument
            //   %rax = int has_matched               -- return value

            // kInstFail will do `ret` anyway.
            if (prog->inst(i)->opcode() != re2::kInstFail && indegree[i] > 1)
                code// if (bit(nfa->visited, i) == 1) return; bit(nfa->visited, i) = 1;
                    .mov  (as::mem{as::rdi} + &NFA->visited, as::rsi)
                    .test ((as::i8) (1 << (i % 8)), as::mem{as::rsi} + i / 8).jmp(fail, as::not_zero)
                    .or_  ((as::i8) (1 << (i % 8)), as::mem{as::rsi} + i / 8);

            RE2JIT_WITH_INST(op, prog, i,
                switch (op->opcode()) {
                    case re2jit::inst::kUnicodeType:
                        code// utf8_chr = rejit_read_utf8(nfa->input, nfa->length);
                            .push (as::rdi)
                            .mov  (as::mem{as::rdi} + &NFA->length, as::rsi)
                            .mov  (as::mem{as::rdi} + &NFA->input,  as::rdi)
                            .call ((void *) &rejit_read_utf8)
                            .pop  (as::rdi)
                            // if ((utf8_length = utf8_chr >> 32) == 0) return;
                            .mov  (as::rax, as::rdx)
                            .shr  (32u,     as::rdx) .jmp(fail, as::zero)
                            // if ((UNICODE_CODEPOINT_TYPE[utf8_chr] & UNICODE_GENERAL) != arg) return;
                            .mov  ((uint64_t) UNICODE_CODEPOINT_TYPE, as::rsi)
                            .mov  (as::eax, as::eax)  // zero upper 32 bits
                            .add  (as::rsi, as::rax)
                            .mov  (as::mem{as::rax}, as::cl)
                            .and_ ((as::i8) UNICODE_GENERAL, as::cl)
                            .cmp  ((as::i8) op->arg(), as::cl).jmp(fail, as::not_equal)
                            // return rejit_thread_wait(nfa, &out, utf8_length);
                            .mov  (*labels[op->out()], as::rsi)
                            .jmp  ((void *) &rejit_thread_wait);

                        break;

                    case re2jit::inst::kBackReference:
                        code// if (nfa->groups <= arg * 2) return;
                            .cmp  ((as::i32) op->arg() * 2, as::mem{as::rdi} + &NFA->groups)
                            .jmp  (fail, as::less_equal_u)  // wasn't enough space to record that group

                            .mov  (as::mem{as::rdi} + &NFA->running, as::rsi)
                            .mov  (as::mem{as::rsi} + &THREAD->groups[op->arg() * 2],     as::eax)
                            .mov  (as::mem{as::rsi} + &THREAD->groups[op->arg() * 2 + 1], as::ecx)
                            // if (start == -1 || end < start) return;
                            .cmp  ((as::i8) -1, as::eax).jmp(fail, as::equal)
                            .sub  (as::eax,     as::ecx).jmp(fail, as::less)
                            // if (start == end) goto out;
                            .jmp  (*labels[op->out()], as::equal)  // empty subgroup = empty transition
                            // if (nfa->length < end - start) return;
                            .cmp  (as::rcx, as::mem{as::rdi} + &NFA->length).jmp(fail, as::less_u)
                            // if (memcmp(nfa->input, nfa->input + start - nfa->offset, end - start)) return;
                            .mov  (as::ecx, as::edx)
                            .mov  (as::rdi, as::r8)
                            .mov  (as::mem{as::r8} + &NFA->input, as::rdi)
                            .mov  (as::rdi, as::rsi)
                            .add  (as::rax, as::rsi)
                            .sub  (as::mem{as::r8} + &NFA->offset, as::rsi)
                            // compare bytes at (%rdi) and (%rsi) until a != b or %ecx is 0
                            .repz().cmpsb()
                            .mov  (as::r8, as::rdi)
                            .jmp  (fail, as::not_equal)
                            // return rejit_thread_wait(nfa, &out, end - start);
                            .mov  (*labels[op->out()], as::rsi)
                            .jmp  ((void *) &rejit_thread_wait);

                        break;

                    default:
                        re2jit::debug::write("re2jit::x64: unknown extcode %hu\n", op->opcode());
                        code.ret();
                        break;
                },

                switch (op->opcode()) {
                    case re2::kInstAltMatch:
                    case re2::kInstAlt:
                        code// if (out(nfa)) return 1;
                            .push  (as::rdi)
                            .call  (*labels[op->out()])
                            .pop   (as::rdi)
                            .test  (as::eax, as::eax).jmp(succeed, as::not_zero);

                        if ((size_t) op->out1() != i + 1)
                            // else goto out1;
                            code.jmp(*labels[op->out1()]);

                        break;

                    case re2::kInstByteRange:
                        code// if (nfa->length == 0) return;
                            .cmp  ((as::i8) 0, as::mem{as::rdi} + &NFA->length)
                            .jmp  (fail, as::equal)
                            // cl = nfa->input[0];
                            .mov  (as::mem{as::rdi} + &NFA->input, as::rax)
                            .mov  (as::mem{as::rax}, as::cl);

                        if (op->foldcase()) {
                            as::label& skip_caseconv = code.mark();

                            code// if ('A' <= cl && cl <= 'Z') cl = cl - 'A' + 'a';
                                .cmp  ((as::i8) 'A', as::cl).jmp(skip_caseconv, as::less_u)
                                .cmp  ((as::i8) 'Z', as::cl).jmp(skip_caseconv, as::more_u)
                                .add  ((as::i8) ('a' - 'A'), as::cl)
                                .mark (skip_caseconv);
                        }

                        if (op->hi() == op->lo())
                            code// if (cl != lo) return;
                                .cmp((as::i8) op->lo(), as::cl).jmp(fail, as::not_equal);

                        else
                            code// if (cl < lo || hi < cl) return;
                                .sub  ((as::i8)  op->lo(),             as::cl)
                                .cmp  ((as::i8) (op->hi() - op->lo()), as::cl).jmp(fail, as::more_u);

                        code// return rejit_thread_wait(nfa, &out, 1);
                            .mov  (*labels[op->out()], as::rsi)
                            .mov  (1u, as::edx)
                            .jmp  ((void *) &rejit_thread_wait);

                        break;

                    case re2::kInstCapture:
                        code// if (nfa->groups <= cap) goto out;
                            .cmp  ((as::i32) op->cap(), as::mem{as::rdi} + &NFA->groups)
                            .jmp  (*labels[op->out()], as::less_equal_u)
                            // esi = nfa->running->groups[cap]; nfa->running->groups[cap] = nfa->offset;
                            .mov  (as::mem{as::rdi} + &NFA->running, as::rcx)
                            .mov  (as::mem{as::rdi} + &NFA->offset,  as::rax)
                            .mov  (as::mem{as::rcx} + &THREAD->groups[op->cap()], as::esi)
                            .mov  (as::eax, as::mem{as::rcx} + &THREAD->groups[op->cap()])
                            // eax = out(nfa);
                            .push (as::rdi)
                            .push (as::rsi)
                            .call (*labels[op->out()])
                            .pop  (as::rsi)
                            .pop  (as::rdi)
                            // nfa->running->groups[cap] = esi; return eax;
                            .mov  (as::mem{as::rdi} + &NFA->running, as::rcx)
                            .mov  (as::esi, as::mem{as::rcx} + &THREAD->groups[op->cap()])
                            .ret  ();

                        break;

                    case re2::kInstEmptyWidth:
                        code// if (~nfa->empty & empty) return;
                            .mov  (as::mem{as::rdi} + &NFA->empty, as::eax)
                            .not_ (as::eax)
                            .test ((as::i32) op->empty(), as::eax).jmp(fail, as::not_zero);

                        if ((size_t) op->out() != i + 1)
                            // else goto out;
                            code.jmp(*labels[op->out()]);

                        break;

                    case re2::kInstNop:
                        if ((size_t) op->out() != i + 1)
                            // goto out;
                            code.jmp(*labels[op->out()]);

                        break;

                    case re2::kInstMatch:
                        // return rejit_thread_match(nfa);
                        code.jmp((void *) &rejit_thread_match);
                        break;

                    default:
                        re2jit::debug::write("re2jit::x64: unknown opcode %d\n", op->opcode());

                    case re2::kInstFail:
                        code.ret();
                        break;
                }
            );
        }

        uint8_t *target = (uint8_t *) mmap(0, code.size(),
                                           PROT_READ | PROT_WRITE,
                                           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

        if (target == (uint8_t *) -1)
            return;

        code.write(target);

        if (mprotect(target, code.size(), PROT_READ | PROT_EXEC) == -1) {
            munmap(target, code.size());
            return;
        }

        _code  = target;
        _entry = target + labels[prog->start()]->offset;
        _size  = code.size();
    }

   ~native()
    {
        munmap(_code, _size);
    }

    rejit_entry_t entry() const
    {
        return (rejit_entry_t) _entry;
    }

    void run(struct rejit_threadset_t *nfa) const
    {
        rejit_thread_dispatch(nfa);
    }
};
