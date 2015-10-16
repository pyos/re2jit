#include <vector>
#include <iterator>

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
#define INSBACK(index, offset) \
    backrefs[index].push_back(code.size() + offset)


// some sw33t x86-64 opcodes!
#define INSCODE(...) do { \
    uint8_t __code[] = { __VA_ARGS__ }; \
    __insert_opcode(code, __code, sizeof(__code)); \
} while (0)


static inline void __insert_opcode(std::vector<uint8_t> &code, uint8_t *op, uint8_t size)
{
    code.reserve(code.size() + size);
    code.insert(code.end(), op, op + size);
}

// encode imm8/16/32/64 as little-endian
#define IMM8(p, off) ((uint8_t)((p)>>(off)))
#define IMM16(p) IMM8(p, 0), IMM8(p, 8)
#define IMM32(p) IMM8(p, 0), IMM8(p, 8), IMM8(p, 16), IMM8(p, 24)
#define IMM64(p) IMM8(p, 0), IMM8(p, 8), IMM8(p, 16), IMM8(p, 24), IMM8(p, 32), IMM8(p, 40), IMM8(p, 48), IMM8(p, 56)

// mov imm32/64, r64
#define MOVL_IMM_RSI(p) INSCODE(0x48, 0xC7, 0xC6, IMM32(p))
#define MOVQ_IMM_RAX(p) INSCODE(0x48, 0xB8, IMM64(p))
#define MOVQ_IMM_RCX(p) INSCODE(0x48, 0xB9, IMM64(p))
#define MOVQ_IMM_RDX(p) INSCODE(0x48, 0xBA, IMM64(p))
#define MOVQ_IMM_RSI(p) INSCODE(0x48, 0xBE, IMM64(p))
#define MOVQ_TBL_RAX(i) INSBACK(i, 2); MOVQ_IMM_RAX(0ULL)
#define MOVQ_TBL_RSI(i) INSBACK(i, 2); MOVQ_IMM_RSI(0ULL)

// mov displacement(%r64), r8/64
#define MOV__MRAX__CL()  INSCODE(      0x8a, 0x08)
#define MOVB_MRDI__CL(d) INSCODE(      0x8a, 0x4f, IMM8(d, 0))
#define MOVB_MRDI_RCX(d) INSCODE(0x48, 0x8b, 0x4f, IMM8(d, 0))
#define MOVB_MRDI_RAX(d) INSCODE(0x48, 0x8b, 0x47, IMM8(d, 0))
#define MOVB_MRDI_EAX(d) INSCODE(      0x8b, 0x47, IMM8(d, 0))
#define MOVL_MRDI__CL(d) INSCODE(      0x8a, 0x8f, IMM32(d))
#define MOVL_MRDI_RCX(d) INSCODE(0x48, 0x8b, 0x8f, IMM32(d))
#define MOVL_MRDI_RAX(d) INSCODE(0x48, 0x8b, 0x87, IMM32(d))
#define MOVL_MRDI_EAX(d) INSCODE(      0x8b, 0x87, IMM32(d))

// mov r64, displacement(%rcx)
#define MOVB_RAX_MRCX(d) INSCODE(0x48, 0x89, 0x41, IMM8(d, 0))
#define MOVB_EAX_MRCX(d) INSCODE(      0x89, 0x41, IMM8(d, 0))
#define MOVL_RAX_MRCX(d) INSCODE(0x48, 0x89, 0x81, IMM32(d))
#define MOVL_EAX_MRCX(d) INSCODE(      0x89, 0x81, IMM32(d))

// cmp imm8/32, r8/64
#define CMPB_IMM__CL(b) INSCODE(      0x80, 0xf9, IMM8(b, 0))
#define CMPB_IMM_RCX(b) INSCODE(0x48, 0x83, 0xf9, IMM8(b, 0))
#define CMPL_IMM_RCX(w) INSCODE(0x48, 0x81, 0xf9, IMM32(w))

// cmp imm64, displacement(%rdi)
#define CMPB_IMM_MRDI(imm, d) INSCODE(0x83, 0x7f, IMM8(d, 0), IMM8(imm, d))

// add imm8, r8
#define ADDB_IMM__CL(b) INSCODE(      0x80, 0xc1, IMM8(b, 0))

// not r32
#define NOTL_EAX() INSCODE(0xF7, 0xD0)

// test imm32, r32
#define TEST_IMM_EAX(v) INSCODE(0xa9, IMM32(v))

// test imm8, displacement(%rsi)
#define TEST_IMMB_MRSI(v, d) INSCODE(0xf6, 0x86, IMM32(d), IMM8(v, 0))

// or imm8, displacement(%rsi)
#define OR_IMMB_MRSI(v, d) INSCODE(0x80, 0x8e, IMM32(d), IMM8(v, 0))

// call imm32, relative near
#define CALL_REL(p) INSCODE(0xE8, IMM32(p))
#define CALL_TBL(i) INSBACK(i, 1); CALL_REL(0)

// call/jump imm64, absolute near through %rax
#define CALL_IMM(p) MOVQ_IMM_RAX((uint64_t) p); INSCODE(0xFF, 0xD0)
#define JMPQ_IMM(p) MOVQ_IMM_RAX((uint64_t) p); INSCODE(0xFF, 0xE0)
#define RETQ() INSCODE(0xC3)

// jump imm32, relative near
#define JMP_UNCOND_REL(off) INSCODE(0xE9, IMM32(off))
#define JMP_UNCOND_TBL(ind) INSBACK(i, 1); JMP_UNCOND_REL(0L)
#define JMP_UNCOND_ABS(off) JMP_UNCOND_ABS(off - code.size() - 5)

// conditional jump imm32, relative to `code.begin()`
#define JMP_REL(type, off) INSCODE(0x0F, 0x80 | type, IMM32(off))
#define JMP_TBL(type, ind) INSBACK(i, 2); JMP_REL(type, 0L)
#define JMP_ABS(type, off) JMP_REL(type, (off - code.size() - 6))
#define JMP_EQ 0x4
#define JMP_NE 0x5
#define JMP_LT 0xC
#define JMP_GE 0xD
#define JMP_LE 0xE
#define JMP_GT 0xF
#define JMP_ZF JMP_NE

// conditional return
#ifdef RE2JIT_X64_NO_RETQ_REDIRECT
#define RETQ_IF_NE() JMP_REL(JMP_EQ, 1L); RETQ()
#define RETQ_IF_EQ() JMP_REL(JMP_NE, 1L); RETQ()
#define RETQ_IF_LT() JMP_REL(JMP_GE, 1L); RETQ()
#define RETQ_IF_GT() JMP_REL(JMP_LE, 1L); RETQ()
#else
// assuming the program always starts with a ret
#define RETQ_IF_NE() JMP_ABS(JMP_NE, 0L)
#define RETQ_IF_EQ() JMP_ABS(JMP_EQ, 0L)
#define RETQ_IF_LT() JMP_ABS(JMP_LT, 0L)
#define RETQ_IF_GT() JMP_ABS(JMP_GT, 0L)
#endif

// mov instructions require an absolute address, jmp - an offset.
#define IS_JUMP_TARGET(i) (code[(i) - 1] == 0xE9 || code[(i) - 1] == 0xE8 || code[(i) - 2] == 0x0F)


static inline void *_compile(re2::Prog *prog)
{
    debug::write("re2jit::it: target = x64 SystemV ABI\n");
    struct _x64_native *storage = new _x64_native{};

    size_t i;
    size_t n = prog->size();

    // Temporary container for machine code.
    std::vector< uint8_t > code;
    // A map of opcode offsets to positions in `code` where an address of that offset
    // is requested. During compilation, all internal jumps are set to 0, and this map
    // is used later for linking.
    std::vector< std::vector<size_t> > backrefs(n);
    // A map of opcode offsets to actual offsets in the compiled code.
    std::vector< size_t > vtable(n);
    // Whether an opcode is targeted by a branching jump. Normally, we have to track
    // which states we visit to avoid going into an infinite loop; however, we only
    // need to do so once per loop, not per opcode. Opcodes not targeted by jumps, or only
    // targeted by a single jump, are thus always safe to evaluate and need not be tracked.
    std::vector< uint8_t > is_jump_target(n);
    // If the program starts with a failing opcode, we can use that to redirect
    // all conditional rets instead of jumping over them.
    if (prog->inst(0)->opcode() != re2::kInstFail) {
        RETQ();
    }

    for (i = 0; i < n; i++) {
        re2::Prog::Inst *op = prog->inst(i);

        switch (op->opcode()) {
            case re2::kInstAlt:
            case re2::kInstAltMatch:
                is_jump_target[op->out1()]++;
                // fallthrough

            case re2::kInstNop:
            case re2::kInstCapture:
            case re2::kInstEmptyWidth:
                is_jump_target[op->out()]++;
                break;

            default: break;
        }
    }

    for (i = 0; i < n; i++) {
        re2::Prog::Inst *op = prog->inst(i);

        vtable[i] = code.size();

        // In sysv abi, first 6 args are %rdi, %rsi, %rdx, %rcx, %r8, %r9. On entry
        // into an opcode, %rdi points to the `rejit_threadset_t`, %rsi is a bit vector
        // of states visited on this run through the queue, and the topmost value on
        // the stack is the return address into `rejit_thread_dispatch`.

        #define _BYTE_ARRAY ((uint8_t *) 0)
        // kInstFail will do `ret` anyway.
        if (op->opcode() != re2::kInstFail && is_jump_target[i] > 1) {
            //    test offset(i), %rsi[index(i)]
            TEST_IMMB_MRSI(1 << BIT_SHIFT(_BYTE_ARRAY, i), BIT_INDEX(_BYTE_ARRAY, i));
            //    ret [if non-zero]
            RETQ_IF_NE();
            //    or offset(i), %rsi[index(i)]
            OR_IMMB_MRSI(1 << BIT_SHIFT(_BYTE_ARRAY, i), BIT_INDEX(_BYTE_ARRAY, i));
        }
        #undef _BYTE_ARRAY

        switch (op->opcode()) {
            case re2::kInstAlt:
                //    call code+vtable[out]
                CALL_TBL(op->out());

                if ((size_t) op->out1() != i + 1) {
                    //    jmp  code+vtable[out1]
                    JMP_UNCOND_TBL(op->out1());
                }

                break;

            case re2::kInstAltMatch:
                //    jmp rejit_thread_free  <-- still no idea what this is
                JMPQ_IMM(&rejit_thread_free);
                break;

            case re2::kInstByteRange:
                //    cmp $0, (%rdi).length
                CMPB_IMM_MRDI(0, offsetof(struct rejit_threadset_t, length));
                //    ret [if ==]
                RETQ_IF_EQ();

                //    mov (%rdi).input, %rax
                MOVB_MRDI_RAX(offsetof(struct rejit_threadset_t, input));
                //    mov (%rax), %cl
                MOV__MRAX__CL();

                if (op->foldcase()) {
                    //    cmp $'A', %cl
                    CMPB_IMM__CL('A');
                    //    jl __skip
                    JMP_REL(JMP_LT, 12L);
                    //    cmp $'Z', %cl
                    CMPB_IMM__CL('Z');
                    //    jg __skip
                    JMP_REL(JMP_GT, 3L);
                    //    add $'a'-'A', %cl
                    ADDB_IMM__CL('a' - 'A');
                    // __skip:
                }

                if (op->hi() == op->lo()) {
                    //    cmp lo, %cl
                    CMPB_IMM__CL(op->lo());
                    //    ret [if !=]
                    RETQ_IF_NE();
                } else {
                    //    cmp lo, %cl
                    CMPB_IMM__CL(op->lo());
                    //    ret [if <]
                    RETQ_IF_LT();
                    //    cmp hi, %cl
                    CMPB_IMM__CL(op->hi());
                    //    ret [if >]
                    RETQ_IF_GT();
                }

                //    mov code+vtable[out], %rsi
                MOVQ_TBL_RSI(op->out());
                //    mov $1, %rdx
                MOVQ_IMM_RDX(1ULL);
                //    jmp rejit_thread_wait
                JMPQ_IMM(&rejit_thread_wait);
                break;

            case re2::kInstCapture:
                //    mov (%rdi).groups, %rcx
                MOVB_MRDI_RCX(offsetof(struct rejit_threadset_t, groups));
                //    cmp cap, %rcx
                CMPL_IMM_RCX(op->cap());
                //    jge code+vtable[out]
                JMP_TBL(JMP_GE, op->out());

                //    mov (%rdi).running, %rcx
                MOVB_MRDI_RCX(offsetof(struct rejit_threadset_t, running));
                //    mov (%rdi).offset, %rax
                MOVB_MRDI_RAX(offsetof(struct rejit_threadset_t, offset));
                //    mov %eax, (%rcx).groups[cap]
                MOVL_EAX_MRCX(offsetof(struct rejit_thread_t, groups) + sizeof(int) * op->cap());

                if ((size_t) op->out() != i + 1) {
                    //    jmp code+vtable[out]
                    JMP_UNCOND_TBL(op->out());
                }

                break;

            case re2::kInstEmptyWidth:
                //    mov (%rdi).empty, %eax
                MOVL_MRDI_EAX(offsetof(struct rejit_threadset_t, empty));
                //    not %eax
                NOTL_EAX();
                //    test empty, %eax
                TEST_IMM_EAX(op->empty());
                //    ret [if !ZF]
                RETQ_IF_NE();

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
                //    call rejit_thread_match
                CALL_IMM(&rejit_thread_match);
                //    TODO prevent kAltMatch from exploring out1 if %eax = 1
                //    ret
                RETQ();
                break;

            case re2::kInstFail:
                //    ret
                RETQ();
                break;
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

    FILE *out = fopen("asd.bin", "wb");
    fwrite(target, 1, code.size(), out);
    fclose(out);

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

    while (rejit_thread_dispatch(nfa, 256)) { }

    return 1;
}
