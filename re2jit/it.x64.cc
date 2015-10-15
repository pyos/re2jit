#include <vector>

#include <stdint.h>
#include <sys/mman.h>


using re2jit::debug;


struct _x64_native
{
    void *code;
    void *entry;
    size_t size;
};


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

    for (i = 0; i < n; i++) {
        re2::Prog::Inst *op = prog->inst(i);

        vtable[i] = code.size();

        // In sysv abi, first 6 args are %rdi, %rsi, %rdx, %rcx, %r8, %r9.
        // On entry into an opcode, %rdi points to the `rejit_threadset_t`, and the
        // topmost value on the stack is the return address into `rejit_thread_dispatch`.

        switch (op->opcode()) {
            case re2::kInstAlt:
                //    mov  code+vtable[out1], %rsi
                //    call rejit_thread_fork
                //    jmp  code+vtable[out]
                return storage;

            case re2::kInstAltMatch:
                //    jmp rejit_thread_free  // still no idea what this is
                return storage;

            case re2::kInstByteRange:
                //    mov offsetof(rejit_threadset_t, input)(%rdi), %rcx
                //    cmp lo, (%rcx)
                //    jl  rejit_thread_fail
                //    cmp hi, (%rcx)
                //    jg  rejit_thread_fail
                //    mov offsetof(rejit_threadset_t, running)(%rdi), %rcx
                //    mov code+vtable[out], offsetof(rejit_thread_t, entry)(%rcx)
                //    mov 1, %rsi
                //    jmp rejit_thread_wait
                return storage;

            case re2::kInstCapture:
                //    mov offsetof(rejit_threadset_t, groups)(%rdi), %rcx
                //    cmp cap, %rcx
                //    jge code+vtable[out]
                //    mov offsetof(rejit_threadset_t, running)(%rdi), %rcx
                //    mov offsetof(rejit_threadset_t, offset)(%rdi), %rax
                //    mov %rax, offsetof(rejit_thread_t, groups)+sizeof(int)*cap(%rcx)
                //    jmp code+vtable[out]
                return storage;

            case re2::kInstEmptyWidth:
                //    mov offsetof(rejit_threadset_t, empty)(%rdi), %rax
                //    inv %rax
                //    test empty, %rax
                //    jnz rejit_thread_fail
                //    jmp code+vtable[out]
                return storage;

            case re2::kInstNop:
                //    jmp code+vtable[out]
                return storage;

            case re2::kInstMatch:
                //    jmp rejit_thread_match
                return storage;

            case re2::kInstFail:
                //    jmp rejit_thread_fail
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
            // TODO write (target+vtable[i]) at (target+ref)
            (void) ref;
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

    while (rejit_thread_dispatch(nfa, 256)) { }

    return 1;
}
