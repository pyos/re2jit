#include <vector>
#include <sys/mman.h>

#include "it.x64.asm.h"


uint64_t rejit_thread_read_utf8(const rejit_threadset_t *nfa)
{
    return rejit_read_utf8((const uint8_t *) nfa->input, nfa->length);
}


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
        as::label& fail        = code.mark();
        as::label& fail_no_xor = code.mark();
        code.xor_(as::eax, as::eax).mark(fail_no_xor).ret();

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
            //   %rdi :: struct rejit_threadset_t *
            //   %rsi, %rdx, %rcx, %r8, %r9 :: undefined
            // Return value:
            //   %rax :: int -- 1 if found a match, 0 otherwise

            // kInstFail will do `ret` anyway.
            if (prog->inst(i)->opcode() != re2::kInstFail && indegree[i] > 1)
                code.mov  (as::mem{as::rdi} + offsetof(struct rejit_threadset_t, visited), as::rsi)
                    .test ((uint8_t) (1 << (i % 8)), as::mem{as::rsi} + i / 8)
                    .jmp  (fail, as::not_zero)
                    .or_  ((uint8_t) (1 << (i % 8)), as::mem{as::rsi} + i / 8);

            RE2JIT_WITH_INST(op, prog, i,
                switch (op->opcode()) {
                    case re2jit::inst::kUnicodeType:
                        code.push (as::rdi)
                            .call ((void *) &rejit_thread_read_utf8)
                            .pop  (as::rdi)
                            .mov  (as::rax, as::rdx)
                            .shr  (32u, as::rdx)
                            .jmp  (fail, as::zero)
                            .mov  (as::eax, as::eax)  // zero upper 32 bits
                            .mov  ((uint64_t) UNICODE_CODEPOINT_TYPE, as::rsi)
                            .add  (as::rsi, as::rax)
                            .mov  (as::mem{as::rax}, as::cl)
                            .and_ ((uint8_t) UNICODE_GENERAL, as::cl)
                            .cmp  ((uint8_t) op->arg(), as::cl)
                            .jmp  (fail, as::not_equal)
                            .mov  (*labels[op->out()], as::rsi)
                            .jmp  ((void *) &rejit_thread_wait);

                        break;

                    case re2jit::inst::kBackReference:
                        code.cmp  ((uint32_t) op->arg() * 2, as::mem{as::rdi} + offsetof(struct rejit_threadset_t, groups))
                            .jmp  (fail, as::less_equal_u)  // wasn't enough space to record that group

                            .mov  (as::mem{as::rdi} + offsetof(struct rejit_threadset_t, running), as::rsi)
                            .mov  (as::mem{as::rsi} + offsetof(struct rejit_thread_t, groups) + sizeof(int) * (op->arg() * 2), as::eax)
                            .mov  (as::mem{as::rsi} + offsetof(struct rejit_thread_t, groups) + sizeof(int) * (op->arg() * 2 + 1), as::ecx)

                            .cmp  ((uint8_t) -1, as::eax)
                            .jmp  (fail, as::equal)

                            .sub  (as::eax, as::ecx)
                            .jmp  (fail, as::less)
                            .jmp  (*labels[op->out()], as::equal)  // empty subgroup = empty transition

                            .mov  (as::ecx, as::edx)
                            .mov  (as::rdi, as::r8)
                            .mov  (as::mem{as::r8} + offsetof(struct rejit_threadset_t, input), as::rdi)
                            .mov  (as::rdi, as::rsi)
                            .sub  (as::mem{as::r8} + offsetof(struct rejit_threadset_t, offset), as::rsi)
                            .add  (as::rax, as::rsi)
                            .repz().cmpsb()
                            // ^    ^-- compare bytes at (%rdi) and (%rsi), increment both
                            // \-- repeat while ZF is set and %rcx is non-zero
                            // Essentially, this opcode is `ZF, SF = memcmp(%rdi, %rsi, %rcx)`.
                            .mov  (as::r8, as::rdi)
                            .jmp  (fail, as::not_equal)

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
                        code.push  (as::rdi)
                            .call  (*labels[op->out()])
                            .pop   (as::rdi)
                            .test  (as::eax, as::eax)  // non-zero if found a match
                            .jmp   (fail_no_xor, as::not_zero);

                        if ((size_t) op->out1() != i + 1)
                            code.jmp(*labels[op->out1()]);

                        break;

                    case re2::kInstByteRange:
                        code.cmp  ((uint8_t) 0, as::mem{as::rdi} + offsetof(struct rejit_threadset_t, length))
                            .jmp  (fail, as::equal)

                            .mov  (as::mem{as::rdi} + offsetof(struct rejit_threadset_t, input), as::rax)
                            .mov  (as::mem{as::rax}, as::cl);

                        if (op->foldcase()) {
                            as::label& skip_caseconv = code.mark();

                            code.cmp  ((uint8_t) 'A', as::cl)
                                .jmp  (skip_caseconv, as::less_u)
                                .cmp  ((uint8_t) 'Z', as::cl)
                                .jmp  (skip_caseconv, as::more_u)
                                .add  ((uint8_t) ('a' - 'A'), as::cl)
                                .mark (skip_caseconv);
                        }

                        if (op->hi() == op->lo())
                            code.cmp  ((uint8_t) op->lo(), as::cl)
                                .jmp  (fail, as::not_equal);

                        else
                            code.sub  ((uint8_t) op->lo(), as::cl)
                                .cmp  ((uint8_t) (op->hi() - op->lo()), as::cl)
                                .jmp  (fail, as::more_u);

                        code.mov  (*labels[op->out()], as::rsi)
                            .mov  (1u, as::edx)
                            .jmp  ((void *) &rejit_thread_wait);
                        break;

                    case re2::kInstCapture:
                        code.cmp  ((uint32_t) op->cap(), as::mem{as::rdi} + offsetof(struct rejit_threadset_t, groups))
                            .jmp  (*labels[op->out()], as::less_equal_u)

                            .mov  (as::mem{as::rdi} + offsetof(struct rejit_threadset_t, running), as::rcx)
                            .mov  (as::mem{as::rdi} + offsetof(struct rejit_threadset_t, offset),  as::rax)
                            .mov  (as::mem{as::rcx} + offsetof(struct rejit_thread_t, groups) + sizeof(int) * op->cap(), as::esi)
                            .mov  (as::eax, as::mem{as::rcx} + offsetof(struct rejit_thread_t, groups) + sizeof(int) * op->cap())

                            .push (as::rdi)
                            .push (as::rsi)
                            .call (*labels[op->out()])
                            .pop  (as::rsi)
                            .pop  (as::rdi)

                            .mov  (as::mem{as::rdi} + offsetof(struct rejit_threadset_t, running), as::rcx)
                            .mov  (as::esi, as::mem{as::rcx} + offsetof(struct rejit_thread_t, groups) + sizeof(int) * op->cap())
                            .ret  ();
                        break;

                    case re2::kInstEmptyWidth:
                        code.mov  (as::mem{as::rdi} + offsetof(struct rejit_threadset_t, empty), as::eax)
                            .not_ (as::eax)
                            .test ((uint32_t) op->empty(), as::eax)
                            .jmp  (fail, as::not_zero);

                        if ((size_t) op->out() != i + 1)
                            code.jmp(*labels[op->out()]);

                        break;

                    case re2::kInstNop:
                        if ((size_t) op->out() != i + 1)
                            code.jmp(*labels[op->out()]);

                        break;

                    case re2::kInstMatch:
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
