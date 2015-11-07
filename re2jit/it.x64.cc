#include <sys/mman.h>

#include "asm64.h"


struct re2jit::native
{
    void * _code;
    size_t _size;

    native(re2::Prog *) : _code(NULL), _size(0)
    {
        as::code code;
        as::label loop, end, fail;

        // System V ABI:
        //   * первые 6 аргументов - rdi, rsi, rdx, rcx, r8, r9
        //   * остальные на стеке
        //   * возвращаемое значение - rax
        //   * регистры rax, rcx, rdx, rdi, rsi, r8, r9, r10 при вызовах не сохраняются
        //   * регистры rbx, rsp, rbp, r11-r15 -- сохраняются.
        //     сгенерированного кода это тоже касается. если испортить значение
        //     в регистре, поведение после ret будет непредсказуемым.

        // memset(&stack[-length], 'x', length);
        code.mov (as::rsi, as::rdx)
            .sub (as::rsi, as::rsp)
            .push(as::rsi)
            .push(as::rdi)
            .mov (as::rsp + 16, as::rdi)
            .push(as::rdi)
            .mov (as::i32('x'), as::esi)
            .call(&memset)
        // if (memcmp(rdi = input, rsi = &stack[-length], rcx = length) != 0) goto fail;
            .pop (as::rsi)
            .pop (as::rdi)
            .pop (as::rcx)
            .repz().cmpsb()
            .mov (as::rsi + as::rcx, as::rsp)
            .jmp (fail, as::not_equal)
        // return 1;
            .mov (as::i8(1), as::eax)
            .ret ()
        // fail: return 0;
            .mark(fail)
            .xor_(as::eax, as::eax)
            .ret ();

        size_t sz = code.size();
        void * tg = mmap(NULL, sz, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

        if (tg == (void *) -1)
            throw std::runtime_error("mmap failed");

        if (!code.write(tg)) {
            munmap(tg, sz);
            throw std::runtime_error("a label was used but not set");
        }

        FILE *f = fopen("asd.bin", "wb");
        fwrite(tg, 1, sz, f);
        fclose(f);

        if (mprotect(tg, sz, PROT_READ | PROT_EXEC) == -1) {
            munmap(tg, sz);
            throw std::runtime_error("can't change permissions");
        }

        _code = tg;
        _size = sz;
    }

   ~native()
    {
        munmap(_code, _size);
    }

    bool match(const re2::StringPiece &text, int /* flags */,
                     re2::StringPiece * /* groups */, int /* ngroups */)
    {
        typedef int f(const char*, size_t);
        // см. it.vm.cc -- там описание StringPiece, Prog, flags и groups.
        return ((f *) _code)(text.data(), text.size());
    }
};
