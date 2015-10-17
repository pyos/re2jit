#include <vector>

#include <stdint.h>
#include <stddef.h>
#include <sys/mman.h>


using re2jit::debug;


struct _x64_native
{
    void *code;
    void *entry;
    size_t size;
};


// note down that `$+offset` should be linked to a vtable index.
// see `_compile` for definition of `backrefs`, `vtable`, and `code`.
#define INSBACK(index, offset) backrefs[index].push_back(code.size() + offset)


// some sw33t x86-64 opcodes!
#define INSCODE(...) do { \
    uint8_t __code[] = { __VA_ARGS__ }; \
    code.reserve(code.size() + sizeof(__code)); \
    code.insert(code.end(), __code, __code + sizeof(__code)); \
} while (0)


// encode imm8/16/32/64 as comma-separated bytes in little-endian
#define IMM8(p, off) ((uint8_t)((p)>>(off)))
#define IMM16(p) IMM8(p, 0), IMM8(p, 8)
#define IMM32(p) IMM8(p, 0), IMM8(p, 8), IMM8(p, 16), IMM8(p, 24)
#define IMM64(p) IMM8(p, 0), IMM8(p, 8), IMM8(p, 16), IMM8(p, 24), IMM8(p, 32), IMM8(p, 40), IMM8(p, 48), IMM8(p, 56)

// mov imm32/64, r64
#define MOVL_IMM_RAX(p) INSCODE(0x48, 0xC7, 0xC0, IMM32(p))
#define MOVL_IMM_RDX(p) INSCODE(0x48, 0xC7, 0xC2, IMM32(p))
#define MOVL_IMM_RSI(p) INSCODE(0x48, 0xC7, 0xC6, IMM32(p))
#define MOVQ_IMM_RAX(p) INSCODE(0x48, 0xB8, IMM64(p))
#define MOVQ_IMM_RCX(p) INSCODE(0x48, 0xB9, IMM64(p))
#define MOVQ_IMM_RDX(p) INSCODE(0x48, 0xBA, IMM64(p))
#define MOVQ_IMM_RSI(p) INSCODE(0x48, 0xBE, IMM64(p))
#define MOVQ_TBL_RAX(p) INSBACK(p, 2); MOVQ_IMM_RAX(0ULL)
#define MOVQ_TBL_RSI(p) INSBACK(p, 2); MOVQ_IMM_RSI(0ULL)

// mov m64=8/32(r64), r8/32/64
#define MOV__MRAX__CL()  INSCODE(      0x8a, 0x08)
#define MOVB_MRDI__CL(p) INSCODE(      0x8a, 0x4f, IMM8(p, 0))
#define MOVB_MRDI_EAX(p) INSCODE(      0x8b, 0x47, IMM8(p, 0))
#define MOVB_MRDI_RAX(p) INSCODE(0x48, 0x8b, 0x47, IMM8(p, 0))
#define MOVB_MRDI_RCX(p) INSCODE(0x48, 0x8b, 0x4f, IMM8(p, 0))
#define MOVB_MRDI_RSI(p) INSCODE(0x48, 0x8b, 0x77, IMM8(p, 0))
#define MOVL_MRDI__CL(p) INSCODE(      0x8a, 0x8f, IMM32(p))
#define MOVL_MRDI_EAX(p) INSCODE(      0x8b, 0x87, IMM32(p))
#define MOVL_MRDI_RAX(p) INSCODE(0x48, 0x8b, 0x87, IMM32(p))
#define MOVL_MRDI_RCX(p) INSCODE(0x48, 0x8b, 0x8f, IMM32(p))

// mov r32/64, m64=imm8/32(r64)
#define MOVB_RAX_MRCX(p) INSCODE(0x48, 0x89, 0x41, IMM8(p, 0))
#define MOVB_EAX_MRCX(p) INSCODE(      0x89, 0x41, IMM8(p, 0))
#define MOVL_RAX_MRCX(p) INSCODE(0x48, 0x89, 0x81, IMM32(p))
#define MOVL_EAX_MRCX(p) INSCODE(      0x89, 0x81, IMM32(p))

// cmp imm8/32, r8/64
#define CMPB_IMM__CL(p) INSCODE(      0x80, 0xF9, IMM8(p, 0))
#define CMPB_IMM_RCX(p) INSCODE(0x48, 0x83, 0xF9, IMM8(p, 0))
#define CMPL_IMM_RCX(p) INSCODE(0x48, 0x81, 0xF9, IMM32(p))

// cmp imm8/32, m64=imm8(r64)
#define CMPB_IMM_MRDI(imm, d) INSCODE(0x83, 0x7F, IMM8(d, 0), IMM8(imm, 0))
#define CMPL_IMM_MRDI(imm, d) INSCODE(0x81, 0x7F, IMM8(d, 0), IMM32(imm))

// add/sub imm8, r8
#define ADDB_IMM__CL(p) INSCODE(0x80, 0xC1, IMM8(p, 0))
#define SUBB_IMM__CL(p) INSCODE(0x80, 0xE9, IMM8(p, 0))

// not r32
#define NOTL_EAX() INSCODE(0xF7, 0xD0)

// test r32/imm32, r32
#define TEST_IMM_EAX(p) INSCODE(0xA9, IMM32(p))
#define TEST_EAX_EAX(p) INSCODE(0x85, 0xC0)

// test imm8, m8=imm32(r64)
#define TEST_IMMB_MRSI(imm, d) INSCODE(0xF6, 0x86, IMM32(d), IMM8(imm, 0))

// xor r32, r32
#define XORL_EAX_EAX() INSCODE(0x31, 0xC0)

// or imm8, m8=imm32(r64)
#define OR_IMMB_MRSI(imm, d) INSCODE(0x80, 0x8E, IMM32(d), IMM8(imm, 0))

// stack r64
#define PUSH_RSI() INSCODE(0x56)
#define PUSH_RDI() INSCODE(0x57)
#define POP_RSI()  INSCODE(0x5E)
#define POP_RDI()  INSCODE(0x5F)

// call imm32, relative near
#define CALL_REL(p) INSCODE(0xE8, IMM32(p))
#define CALL_TBL(k) INSBACK(k, 1); CALL_REL(0)
#define RETQ() INSCODE(0xC3)

// call/jump imm32, absolute indirect through %rax
#define CALL_IMM(p) MOVL_IMM_RAX((uint64_t) p); INSCODE(0xFF, 0xD0)
#define JMPL_IMM(p) MOVL_IMM_RAX((uint64_t) p); INSCODE(0xFF, 0xE0)

// jump imm32, relative near
#define JMP_UNCOND_REL(p) INSCODE(0xE9, IMM32(p))  // rel. to next opcode
#define JMP_UNCOND_TBL(k) INSBACK(k, 1); JMP_UNCOND_REL(0L)  // abs. to vtable entry
#define JMP_UNCOND_ABS(p) JMP_UNCOND_ABS(p - code.size() - 5)  // rel. to code start

// jump imm32, conditional relative near
#define JMP_REL(type, p) INSCODE(0x0F, 0x80 | (type), IMM32(p))
#define JMP_TBL(type, k) INSBACK(k, 2); JMP_REL(type, 0L)
#define JMP_ABS(type, p) JMP_REL(type, p - code.size() - 6)
#define JMP_LT_U 0x2  // note that `xor 1` inverts the condition
#define JMP_GE_U 0x3
#define JMP_EQ   0x4
#define JMP_NE   0x5
#define JMP_LE_U 0x6
#define JMP_GT_U 0x7
#define JMP_LT   0xC
#define JMP_GE   0xD
#define JMP_LE   0xE
#define JMP_GT   0xF
#define JMP_ZERO JMP_EQ
#define JMP_NZ   JMP_NE

#define JMP_OVER(type, body) do { \
    JMP_REL(type, 0L);            \
    size_t __q = code.size() - 4; \
    size_t __r = code.size();     \
    body;                         \
    __r = code.size() - __r;      \
    code[__q++] = IMM8(__r, 0);   \
    code[__q++] = IMM8(__r, 8);   \
    code[__q++] = IMM8(__r, 16);  \
    code[__q++] = IMM8(__r, 32);  \
} while (0)


// assuming the program always starts with a ret
#define RETQ_IF(type) JMP_ABS(type, 0L)


// whether `i` points to an argument to a (relative near) jump/call
#define IS_JUMP_TARGET(i) ( \
     code[(i) - 1] == 0xE8 /* unconditional call  */ \
  || code[(i) - 1] == 0xE9 /* unconditional jump */ \
  || ((code[(i) - 1] & 0xF0) == 0x80 && code[(i) - 2] == 0x0F) /* conditional jump */)


static inline void *_compile(re2::Prog *prog)
{
    debug::write("re2jit::it: target = x86_64 System V ABI\n");
    struct _x64_native *storage = new _x64_native{};

    size_t i;
    size_t n = prog->size();

    std::vector< uint8_t > code;
    // For each opcode id, stores the list of positions in generated code where a jump
    // or a call to that opcode is necessary. The linker should insert an appropriate
    // reference from `vtable` at each of these positions.
    std::vector< std::vector<size_t> > backrefs(n);
    // A map of opcode offsets to actual offsets in the compiled code.
    // Populated while emitting code, used for linking later.
    std::vector< size_t > vtable(n);
    // How many empty transitions have the i-th opcode as a target. We have to maintain
    // a bit vector of visited states to avoid going into an infinite loop; however,
    // opcodes only targeted by a single transition can only occur in the middle
    // of a loop and are therefore always safe to enter.
    std::vector< unsigned > is_jump_target(n);
    // If the program starts with a failing opcode, we can use that to redirect
    // all conditional rets instead of jumping over them.
    XORL_EAX_EAX();
    RETQ();

    for (i = 0; i < n; i++) {
        re2::Prog::Inst *op = prog->inst(i);

        switch (op->opcode()) {
            case re2::kInstAlt:
            case re2::kInstAltMatch:
                is_jump_target[op->out1()]++;
                // fallthrough

            case re2::kInstNop:
            case re2::kInstCapture:
            case re2::kInstByteRange:
            case re2::kInstEmptyWidth:
                is_jump_target[op->out()]++;
                break;

            default: break;
        }
    }

    std::vector< bool > reachable(n);

    {  // prune unnecessary states
        std::deque< size_t > visit;
        visit.push_back(prog->start());
        reachable[prog->start()] = true;

        while (!visit.empty()) {
            i = visit.front();
            visit.pop_front();

            re2::Prog::Inst *op = prog->inst(i);

            switch (op->opcode()) {
                case re2::kInstFail:
                case re2::kInstMatch:
                    break;

                case re2::kInstAlt:
                case re2::kInstAltMatch:
                    if (!reachable[op->out1()]) {
                        reachable[op->out1()] = true;
                        visit.push_back(op->out1());
                    }
                    // fallthrough

                default:
                    if (!reachable[op->out()]) {
                        reachable[op->out()] = true;
                        visit.push_back(op->out());
                    }
            }
        }
    }

    for (i = 0; i < n; i++) {
        re2::Prog::Inst *op = prog->inst(i);

        vtable[i] = code.size();

        if (!reachable[i]) continue;

        // In sysv abi, first 6 args are %rdi, %rsi, %rdx, %rcx, %r8, %r9. On entry
        // into an opcode, %rdi points to the `rejit_threadset_t`, and the topmost value
        // on the stack is the return address into `rejit_thread_dispatch`.

        #define _BYTE_ARRAY ((uint8_t *) 0)
        // kInstFail will do `ret` anyway.
        if (op->opcode() != re2::kInstFail && is_jump_target[i] > 1) {
            //    mov (%rdi).states_visited, %rsi
            MOVB_MRDI_RSI(offsetof(struct rejit_threadset_t, states_visited));
            //    test offset(i), %rsi[index(i)]
            TEST_IMMB_MRSI(1 << BIT_SHIFT(_BYTE_ARRAY, i), BIT_INDEX(_BYTE_ARRAY, i));
            //    ret [if non-zero]
            RETQ_IF(JMP_NE);
            //    or offset(i), %rsi[index(i)]
            OR_IMMB_MRSI(1 << BIT_SHIFT(_BYTE_ARRAY, i), BIT_INDEX(_BYTE_ARRAY, i));
        }
        #undef _BYTE_ARRAY

        switch (op->opcode()) {
            case re2::kInstAlt:
                PUSH_RDI();
                //    call code+vtable[out]
                CALL_TBL(op->out());
                POP_RDI();
                // %eax == 1 if found a match in that branch, 0 otherwise
                TEST_EAX_EAX(); RETQ_IF(JMP_NZ);

                if ((size_t) op->out1() != i + 1) {
                    //    jmp  code+vtable[out1]
                    JMP_UNCOND_TBL(op->out1());
                }

                break;

            case re2::kInstAltMatch:
                // TODO find out what exactly this opcode is
                debug::write("re2jit::it: unsupported opcode kInstAltMatch\n");
                return storage;

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
                    JMP_OVER(JMP_LE_U, {
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
                //    mov $1, %rdx
                MOVL_IMM_RDX(1ULL);
                //    jmp rejit_thread_wait
                JMPL_IMM(&rejit_thread_wait);
                break;

            case re2::kInstCapture:
                //    cmp cap, (%rdi).groups
                CMPL_IMM_MRDI(op->cap(), offsetof(struct rejit_threadset_t, groups));
                //    jae code+vtable[out]
                JMP_TBL(JMP_LE_U, op->out());

                //    mov (%rdi).running, %rcx
                MOVB_MRDI_RCX(offsetof(struct rejit_threadset_t, running));
                //    mov (%rdi).offset, %rax
                MOVB_MRDI_RAX(offsetof(struct rejit_threadset_t, offset));

                {
                    //    mov %eax, (%rcx).groups[cap]
                    size_t off = offsetof(struct rejit_thread_t, groups) + sizeof(int) * op->cap();

                    if (off < 256) {
                        MOVB_EAX_MRCX(off);
                    } else {
                        MOVL_EAX_MRCX(off);
                    }
                }

                if ((size_t) op->out() != i + 1) {
                    //    jmp code+vtable[out]
                    JMP_UNCOND_TBL(op->out());
                }

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
                debug::write("re2jit::it: unknown opcode %d\n", op->opcode());
                return storage;
        }
    }

    uint8_t *target = (uint8_t *) mmap(0, code.size(),
                                       PROT_READ | PROT_WRITE,
                                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (target == (uint8_t *) -1) {
        return storage;
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
        return storage;
    }

    storage->size  = code.size();
    storage->code  = target;
    storage->entry = target + vtable[prog->start()];
    return storage;
}


static inline void _destroy(void *st)
{
    struct _x64_native *storage = (struct _x64_native *) st;

    munmap(storage->code, storage->size);
    delete storage;
}


static inline rejit_entry_t _entry(void *st)
{
    return (rejit_entry_t) ((struct _x64_native *) st)->entry;
}


static inline bool _run(void *st, struct rejit_threadset_t *nfa)
{
    struct _x64_native *storage = (struct _x64_native *) st;

    if (!storage->code) {
        return 0;
    }

    while (rejit_thread_dispatch(nfa, 4096)) { }

    return 1;
}
