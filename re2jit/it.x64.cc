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

// encode imm16/32/64 as little-endian
#define _BYTE(p, off) ((uint8_t)((p)>>(off)))
#define IMM16(p) _BYTE(p, 0), _BYTE(p, 8)
#define IMM32(p) _BYTE(p, 0), _BYTE(p, 8), _BYTE(p, 16), _BYTE(p, 24)
#define IMM64(p) _BYTE(p, 0), _BYTE(p, 8), _BYTE(p, 16), _BYTE(p, 24), _BYTE(p, 32), _BYTE(p, 40), _BYTE(p, 48), _BYTE(p, 56)

// mov imm32/64, r64
#define MOVL_IMM_RSI(p) INSCODE(0x48, 0xC7, 0xC6, IMM32(p))
#define MOVQ_IMM_RAX(p) INSCODE(0x48, 0xB8, IMM64(p))
#define MOVQ_IMM_RCX(p) INSCODE(0x48, 0xB9, IMM64(p))
#define MOVQ_IMM_RSI(p) INSCODE(0x48, 0xBE, IMM64(p))
#define MOVQ_TBL_RAX(i) INSBACK(i, 2); MOVQ_IMM_RAX(0ULL)
#define MOVQ_TBL_RSI(i) INSBACK(i, 2); MOVQ_IMM_RSI(0ULL)

// mov displacement(%r64), r8/64
#define MOV__MRAX__CL()  INSCODE(      0x8a, 0x08)
#define MOVB_MRDI__CL(d) INSCODE(      0x8a, 0x4f, _BYTE(d, 0))
#define MOVB_MRDI_RCX(d) INSCODE(0x48, 0x8b, 0x4f, _BYTE(d, 0))
#define MOVB_MRDI_RAX(d) INSCODE(0x48, 0x8b, 0x47, _BYTE(d, 0))
#define MOVB_MRDI_EAX(d) INSCODE(      0x8b, 0x47, _BYTE(d, 0))
#define MOVL_MRDI__CL(d) INSCODE(      0x8a, 0x8f, IMM32(d))
#define MOVL_MRDI_RCX(d) INSCODE(0x48, 0x8b, 0x8f, IMM32(d))
#define MOVL_MRDI_RAX(d) INSCODE(0x48, 0x8b, 0x87, IMM32(d))
#define MOVL_MRDI_EAX(d) INSCODE(      0x8b, 0x87, IMM32(d))

// mov r64, displacement(%rcx)
#define MOVB_RAX_MRCX(d) INSCODE(0x48, 0x89, 0x41, _BYTE(d, 0))
#define MOVB_EAX_MRCX(d) INSCODE(      0x89, 0x41, _BYTE(d, 0))
#define MOVL_RAX_MRCX(d) INSCODE(0x48, 0x89, 0x81, IMM32(d))
#define MOVL_EAX_MRCX(d) INSCODE(      0x89, 0x81, IMM32(d))

// cmp imm8/32, r8/64
#define CMPB_IMM__CL(b) INSCODE(      0x80, 0xf9, _BYTE(b, 0))
#define CMPB_IMM_RCX(b) INSCODE(0x48, 0x83, 0xf9, _BYTE(b, 0))
#define CMPL_IMM_RCX(w) INSCODE(0x48, 0x81, 0xf9, IMM32(w))

// add imm8, r8
#define ADDB_IMM__CL(b) INSCODE(      0x80, 0xc1, _BYTE(b, 0))

// not r32
#define NOTL_EAX() INSCODE(0xF7, 0xD0)

// test imm32, r32
#define TEST_IMM_EAX(v) INSCODE(0xa9, IMM32(v))

// call/jump imm64, absolute near through %rax
#define CALL_IMM(p) MOVQ_IMM_RAX((uint64_t) p); INSCODE(0xFF, 0xD0)
#define JMPQ_IMM(p) MOVQ_IMM_RAX((uint64_t) p); INSCODE(0xFF, 0xE0)
#define JMPQ_TBL(i) MOVQ_TBL_RAX(i); INSCODE(0xFF, 0xE0)

// conditional jump imm32, relative to `code.begin()`
#define JL_REL(off)  INSCODE(0x0F, 0x8C, IMM32(off - code.size() - 6))
#define JG_REL(off)  INSCODE(0x0F, 0x8F, IMM32(off - code.size() - 6))
#define JNZ_REL(off) INSCODE(0x0F, 0x85, IMM32(off - code.size() - 6))
#define JMP_REL(off) INSCODE(0xE9, IMM32(off - code.size() - 5))
#define JMP_TBL(off) INSBACK(i, 1); INSCODE(0xE9, IMM32(0ULL))
#define JGE_TBL(off) INSBACK(i, 2); INSCODE(0x0F, 0x8D, IMM32(0ULL))
#define JNE_REL(off) JNZ_REL(off)

// mov instructions require an absolute address, jmp - an offset.
#define IS_JUMP_TARGET(i) (code[(i) - 1] == 0xE9 || code[(i) - 2] == 0x0F)


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

    size_t fail_offset = code.size();

    // Conditional jumps cannot be indirect, so we'll jump here first.
    JMPQ_IMM(&rejit_thread_fail);

    for (i = 0; i < n; i++) {
        re2::Prog::Inst *op = prog->inst(i);

        vtable[i] = code.size();

        // In sysv abi, first 6 args are %rdi, %rsi, %rdx, %rcx, %r8, %r9.
        // On entry into an opcode, %rdi points to the `rejit_threadset_t`, and the
        // topmost value on the stack is the return address into `rejit_thread_dispatch`.

        switch (op->opcode()) {
            case re2::kInstAlt:
                //    mov  code+vtable[out1], %rsi
                MOVQ_TBL_RSI(op->out1());
                //    call rejit_thread_fork
                CALL_IMM(&rejit_thread_fork);

                if ((size_t) op->out() != i + 1) {
                    //    jmp  code+vtable[out]
                    JMP_TBL(op->out());
                }

                break;

            case re2::kInstAltMatch:
                //    jmp rejit_thread_free  <-- still no idea what this is
                JMPQ_IMM(&rejit_thread_free);
                break;

            case re2::kInstByteRange:
                //    mov offsetof(rejit_threadset_t, input)(%rdi), %rax
                MOVB_MRDI_RAX(offsetof(struct rejit_threadset_t, input));
                //    mov (%rax), %cl
                MOV__MRAX__CL();

                if (op->foldcase()) {
                    //    cmp $'A', %cl
                    CMPB_IMM__CL('A');
                    //    jl __skip
                    JL_REL(18UL + code.size());
                    //    cmp $'Z', %cl
                    CMPB_IMM__CL('Z');
                    //    jg __skip
                    JG_REL(9UL + code.size());
                    //    add $'a'-'A', %cl
                    ADDB_IMM__CL('a' - 'A');
                    // __skip:
                }

                if (op->hi() == op->lo()) {
                    //    cmp lo, %cl
                    CMPB_IMM__CL(op->lo());
                    //    jne rejit_thread_fail
                    JNE_REL(fail_offset);
                } else {
                    //    cmp lo, %cl
                    CMPB_IMM__CL(op->lo());
                    //    jl  rejit_thread_fail
                    JL_REL(fail_offset);
                    //    cmp hi, %cl
                    CMPB_IMM__CL(op->hi());
                    //    jg  rejit_thread_fail
                    JG_REL(fail_offset);
                }

                //    mov offsetof(rejit_threadset_t, running)(%rdi), %rcx
                MOVB_MRDI_RCX(offsetof(struct rejit_threadset_t, running));
                //    mov code+vtable[out], %rax  <-- can't mov imm64, m64
                MOVQ_TBL_RAX(op->out());
                //    mov %rax, offsetof(rejit_thread_t, entry)(%rcx)
                MOVB_RAX_MRCX(offsetof(struct rejit_thread_t, entry));
                //    mov $1, %rsi
                MOVL_IMM_RSI(1);
                //    jmp rejit_thread_wait
                JMPQ_IMM(&rejit_thread_wait);
                break;

            case re2::kInstCapture:
                //    mov offsetof(rejit_threadset_t, groups)(%rdi), %rcx
                MOVB_MRDI_RCX(offsetof(struct rejit_threadset_t, groups));
                //    cmp cap, %rcx
                CMPL_IMM_RCX(op->cap());
                //    jge code+vtable[out]
                JGE_TBL(op->out());
                //    mov offsetof(rejit_threadset_t, running)(%rdi), %rcx
                MOVB_MRDI_RCX(offsetof(struct rejit_threadset_t, running));
                //    mov offsetof(rejit_threadset_t, offset)(%rdi), %rax
                MOVB_MRDI_RAX(offsetof(struct rejit_threadset_t, offset));
                //    mov %eax, offsetof(rejit_thread_t, groups)+sizeof(int)*cap(%rcx)
                MOVL_EAX_MRCX(offsetof(struct rejit_thread_t, groups) + sizeof(int) * op->cap());
                if ((size_t) op->out() != i + 1) {
                    //    jmp code+vtable[out]
                    JMP_TBL(op->out());
                }
                break;

            case re2::kInstEmptyWidth:
                //    mov offsetof(rejit_threadset_t, empty)(%rdi), %eax
                MOVL_MRDI_EAX(offsetof(struct rejit_threadset_t, empty));
                //    not %eax
                NOTL_EAX();
                //    test empty, %eax
                TEST_IMM_EAX(op->empty());
                //    jnz rejit_thread_fail
                JNZ_REL(fail_offset);

                if ((size_t) op->out() != i + 1) {
                    //    jmp code+vtable[out]
                    JMP_TBL(op->out());
                }

                break;

            case re2::kInstNop:
                if ((size_t) op->out() != i + 1) {
                    //    jmp code+vtable[out]
                    JMP_TBL(op->out());
                }
                break;

            case re2::kInstMatch:
                //    jmp rejit_thread_match
                JMPQ_IMM(&rejit_thread_match);
                break;

            case re2::kInstFail:
                //    jmp rejit_thread_fail
                JMPQ_IMM(&rejit_thread_fail);
                break;
        }
    }

    uint8_t *target = (uint8_t *) mmap(0, code.size(),
                                       PROT_READ | PROT_WRITE,
                                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (target == (uint8_t *) -1) {
        return storage;
    }

    printf("\nbase = %p\n", target);

    {
        uint8_t *target2 = target;

        for (uint8_t byte : code) {
            *target2++ = byte;
        }
    }

    for (i = 0; i < n; i++) {
        for (size_t ref : backrefs[i]) {
            if (IS_JUMP_TARGET(ref)) {
                // jumps use 32-bit signed offsets, thus the target is relative to ref+4.
                size_t offset = vtable[i] - (ref + 4);

                target[ref++] = _BYTE(offset, 0);
                target[ref++] = _BYTE(offset, 8);
                target[ref++] = _BYTE(offset, 16);
                target[ref++] = _BYTE(offset, 24);
            } else {
                size_t addr = (size_t) (target + vtable[i]);
                // movq -- 64-bit absolute pointer
                target[ref++] = _BYTE(addr, 0);
                target[ref++] = _BYTE(addr, 8);
                target[ref++] = _BYTE(addr, 16);
                target[ref++] = _BYTE(addr, 24);
                target[ref++] = _BYTE(addr, 32);
                target[ref++] = _BYTE(addr, 40);
                target[ref++] = _BYTE(addr, 48);
                target[ref++] = _BYTE(addr, 56);
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
