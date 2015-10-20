#include <deque>
#include <vector>

#include <stdint.h>
#include <stddef.h>
#include <sys/mman.h>

#include "it.h"
#include "debug.h"
#include "threads.h"
#include "rewriter.h"

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

    native(re2::Prog *prog)
    {
        size_t i;
        size_t n = prog->size();
        re2::Prog::Inst *op;

        std::vector< uint8_t > code;
        // A map of opcode offsets to actual offsets in the compiled code.
        // Populated while emitting code, used for linking later.
        std::vector< size_t > vtable(n);
        // For each opcode id, stores the list of positions in generated code where a jump
        // or a reference to that opcode is necessary. The linker should insert an appropriate
        // entry from `vtable` at each of these.
        std::vector< std::vector<size_t> > backrefs(n);

        // If the program starts with a failing opcode, we can use that to redirect
        // all conditional rets instead of jumping over them.
        XORL_EAX_EAX();
        RETQ();

        // How many empty transitions have the i-th opcode as a target. We have to maintain
        // a bit vector of visited states to avoid going into an infinite loop; however,
        // opcodes with indegree 1 can never be at the start of a loop, so we can avoid
        // some memory lookups on these.
        std::vector< unsigned > indegree(n);

        for (i = 0; i < n; i++) {
            RE2JIT_WITH_INST(op, prog, i,
                switch (op.opcode) {
                    default:
                        indegree[op.out]++;
                        break;
                },

                switch (op->opcode()) {
                    case re2::kInstAlt:
                    case re2::kInstAltMatch:
                        indegree[op->out1()]++;

                    case re2::kInstNop:
                    case re2::kInstCapture:
                    case re2::kInstByteRange:
                    case re2::kInstEmptyWidth:
                        indegree[op->out()]++;

                    default:
                        break;
                }
            );
        }

        // States not reachable from the entry point don't need to be compiled. Duh.
        std::vector< bool > reachable(n);

        {
            std::deque< size_t > visit;
            visit.push_back(prog->start());
            reachable[prog->start()] = true;

            while (!visit.empty()) {
                i = visit.front();
                visit.pop_front();

                op = prog->inst(i);

                switch (op->opcode()) {
                    case re2::kInstAlt:
                    case re2::kInstAltMatch:
                        if (!reachable[op->out1()] && indegree[op->out()]) {
                            reachable[op->out1()] = true;
                            visit.push_back(op->out1());
                        }

                    case re2::kInstNop:
                    case re2::kInstCapture:
                    case re2::kInstByteRange:
                    case re2::kInstEmptyWidth:
                        if (!reachable[op->out()] && indegree[op->out()]) {
                            reachable[op->out()] = true;
                            visit.push_back(op->out());
                        }

                    default:
                        break;
                }
            }
        }

        for (i = 0; i < n; i++) if (reachable[i]) {
            vtable[i] = code.size();

            // Each opcode should conform to System V ABI calling convention.
            //   %rdi :: struct rejit_threadset_t *
            //   %rsi, %rdx, %rcx, %r8, %r9 :: undefined
            // Return value:
            //   %rax :: int -- 1 if found a match, 0 otherwise

            // kInstFail will do `ret` anyway.
            if (prog->inst(i)->opcode() != re2::kInstFail && indegree[i] > 1) {
                //    mov (%rdi).visited, %rsi
                MOVB_MRDI_RSI(offsetof(struct rejit_threadset_t, visited));
                //    test 1<<(i%8), %rsi[i / 8]
                TEST_IMMB_MRSI(1 << (i % 8), i / 8);
                //    ret [if non-zero]
                RETQ_IF(JMP_NE);
                //    or 1<<(i%8), %rsi[i / 8]
                ORB_IMM_MRSI(1 << (i % 8), i / 8);
            }

            RE2JIT_WITH_INST(op, prog, i,
                switch (op.opcode) {
                    case re2jit::opcode::kUnicodeLetter:
                    case re2jit::opcode::kUnicodeNumber: {
                        //    push %rdi
                        PUSH_RDI();
                        //    call rejit_thread_read_utf8
                        CALL_IMM(&rejit_thread_read_utf8);
                        //    pop %rdi
                        POP_RDI();
                        //    mov %rax, %rdx
                        MOVQ_RAX_RDX();
                        //    shr $32, %rdx
                        SHRQ_IMM_RDX(32);
                        //    ret [if ==]
                        RETQ_IF(JMP_ZERO);
                        // TODO check the class of %eax
                        //    mov code+vtable[out], %rsi
                        MOVQ_TBL_RSI(op.out);
                        //    jmp rejit_thread_wait
                        JMPL_IMM(&rejit_thread_wait);
                        break;
                    }

                    default:
                        re2jit::debug::write("re2jit::x64: unknown extcode %hu\n", op.opcode);
                        RETQ();
                        break;
                },

                switch (op->opcode()) {
                    case re2::kInstAltMatch:
                    case re2::kInstAlt:
                        //    call code+vtable[out]
                        PUSH_RDI(); CALL_TBL(op->out()); POP_RDI();
                        //    test %eax, %eax -- non-zero if found a match
                        TEST_EAX_EAX();
                        //    jnz -> ret, skipping over `xor %eax, %eax`
                        JMP_ABS(JMP_NZ, 2L);

                        if ((size_t) op->out1() != i + 1) {
                            //    jmp  code+vtable[out1]
                            JMP_UNCOND_TBL(op->out1());
                        }

                        break;

                    case re2::kInstByteRange:
                        //    cmp $0, (%rdi).length
                        CMPB_IMM_MRDI(0, offsetof(struct rejit_threadset_t, length));
                        //    ret [if ==]
                        RETQ_IF(JMP_EQ);

                        //    mov (%rdi).input, %rax
                        MOVB_MRDI_RAX(offsetof(struct rejit_threadset_t, input));
                        //    mov (%rax), %cl
                        MOV__MRAX__CL();

                        if (op->foldcase()) {
                            //    cmp $'A', %cl
                            CMPB_IMM__CL('A');
                            //    jb skip
                            JMP_OVER(JMP_LT_U, {
                                //    cmp $'Z', %cl
                                CMPB_IMM__CL('Z');
                                //    ja  skip
                                JMP_OVER(JMP_GT_U,
                                    //    add $'a'-'A', %cl
                                    ADDB_IMM__CL('a' - 'A'));
                            }); // skip:
                        }

                        if (op->hi() == op->lo()) {
                            //    cmp lo, %cl
                            CMPB_IMM__CL(op->lo());
                            //    ret [if !=]
                            RETQ_IF(JMP_NE);
                        } else {
                            //    sub lo, %cl
                            SUBB_IMM__CL(op->lo());
                            //    cmp hi-lo, %cl
                            CMPB_IMM__CL(op->hi() - op->lo());
                            //    ret [if >]
                            RETQ_IF(JMP_GT_U);
                        }

                        //    mov code+vtable[out], %rsi
                        MOVQ_TBL_RSI(op->out());
                        //    mov $1, %edx
                        MOVL_IMM_EDX(1u);
                        //    jmp rejit_thread_wait
                        JMPL_IMM(&rejit_thread_wait);
                        break;

                    case re2::kInstCapture:
                        //    cmp cap, (%rdi).groups
                        CMPL_IMM_MRDI(op->cap(), offsetof(struct rejit_threadset_t, groups));
                        //    jbe code+vtable[out]
                        JMP_TBL(JMP_LE_U, op->out());

                        //    mov (%rdi).running, %rcx
                        MOVB_MRDI_RCX(offsetof(struct rejit_threadset_t, running));
                        //    mov (%rdi).offset, %rax
                        MOVB_MRDI_RAX(offsetof(struct rejit_threadset_t, offset));
                        //    mov (%rcx).groups[cap], %esi
                        MOVL_MRCX_ESI(offsetof(struct rejit_thread_t, groups) + sizeof(int) * op->cap());
                        //    mov %eax, (%rcx).groups[cap]
                        MOVL_EAX_MRCX(offsetof(struct rejit_thread_t, groups) + sizeof(int) * op->cap());

                        //    call code+vtable[out]
                        PUSH_RDI(); PUSH_RSI(); CALL_TBL(op->out()); POP_RSI(); POP_RDI();

                        //    mov (%rdi).running, %rcx
                        MOVB_MRDI_RCX(offsetof(struct rejit_threadset_t, running));
                        //    mov %esi, (%rcx).groups[cap]
                        MOVL_ESI_MRCX(offsetof(struct rejit_thread_t, groups) + sizeof(int) * op->cap());
                        //    ret
                        RETQ();
                        break;

                    case re2::kInstEmptyWidth:
                        //    mov (%rdi).empty, %eax
                        MOVB_MRDI_EAX(offsetof(struct rejit_threadset_t, empty));
                        //    not %eax
                        NOTL_EAX();
                        //    test empty, %eax
                        TEST_IMM_EAX(op->empty());
                        //    ret [if non-zero]
                        RETQ_IF(JMP_NZ);

                        if ((size_t) op->out() != i + 1) {
                            //    jmp code+vtable[out]
                            JMP_UNCOND_TBL(op->out());
                        }

                        break;

                    case re2::kInstNop:
                        if ((size_t) op->out() != i + 1) {
                            //    jmp code+vtable[out]
                            JMP_UNCOND_TBL(op->out());
                        }
                        break;

                    case re2::kInstMatch:
                        //    jmp rejit_thread_match
                        JMPL_IMM(&rejit_thread_match);
                        break;

                    case re2::kInstFail:
                        //    ret
                        RETQ();
                        break;

                    default:
                        re2jit::debug::write("re2jit::x64: unknown opcode %d\n", op->opcode());
                        return;
                }
            );
        }

        uint8_t *target = (uint8_t *) mmap(0, code.size(),
                                           PROT_READ | PROT_WRITE,
                                           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

        if (target == (uint8_t *) -1) {
            return;
        }

        {
            uint8_t *target2 = target;

            for (uint8_t byte : code) {
                *target2++ = byte;
            }
        }

        for (i = 0; i < n; i++) {
            for (size_t ref : backrefs[i]) {
                if (IS_JUMP_TARGET(ref)) {
                    int32_t offset = vtable[i] - (ref + 4);
                    // jumps use 32-bit signed offsets, thus the target is relative to ref+4.
                    memcpy(target + ref, &offset, sizeof(offset));
                } else {
                    uint64_t addr = (size_t) (target + vtable[i]);
                    // movq -- 64-bit absolute pointer
                    memcpy(target + ref, &addr, sizeof(addr));
                }
            }
        }

        if (mprotect(target, code.size(), PROT_READ | PROT_EXEC) == -1) {
            munmap(target, code.size());
            return;
        }

        _size  = code.size();
        _code  = target;
        _entry = target + vtable[prog->start()];
    }

   ~native()
    {
        if (_code) {
            munmap(_code, _size);
        }
    }

    rejit_entry_t entry() const
    {
        return (rejit_entry_t) _entry;
    }

    int run(struct rejit_threadset_t *nfa) const
    {
        if (!_code) {
            return 0;
        }

        rejit_thread_dispatch(nfa);
        return 1;
    };
};
