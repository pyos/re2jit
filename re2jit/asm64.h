#ifndef AS_ASM64_H
#define AS_ASM64_H

#include <deque>
#include <vector>
#include <string.h>
#include <stddef.h>
#include <stdint.h>


namespace as
{
    typedef uint8_t  i8;
    typedef uint32_t i32;
    typedef uint64_t i64;
    typedef  int32_t s32;

    enum condition : i8
    {
        overflow = 0, not_overflow = 1, negative = 8, not_negative = 9, even = 10, odd  = 11,
        less_u = 2, more_equal_u = 3, equal = 4, not_equal = 5, less_equal_u = 6, more_u = 7,
        less  = 12, more_equal  = 13, zero  = 4, not_zero  = 5, less_equal  = 14, more  = 15
    };

    struct reg
    {
        const /* constexpr const constexpr static const constexpr */ i8 id;
        constexpr i8 L() const { return (id & 7); }
        constexpr i8 H() const { return (id & 8) >> 3; }
        constexpr bool operator == (const reg& y) const { return y.id == id; }
        constexpr bool operator != (const reg& y) const { return y.id != id; }
    };

    struct rb  : reg { constexpr explicit rb  (i8 id) : reg{id} {} };
    struct r32 : reg { constexpr explicit r32 (i8 id) : reg{id} {} };
    struct r64 : reg { constexpr explicit r64 (i8 id) : reg{id} {} };

    static constexpr const rb   al{0},  cl{1},  dl{2},   bl{3};
    static constexpr const r32 eax{0}, ecx{1}, edx{2},  ebx{3},  esp{4},  ebp{5},  esi{6},  edi{7};
    static constexpr const r64 rax{0}, rcx{1}, rdx{2},  rbx{3},  rsp{4},  rbp{5},  rsi{6},  rdi{7},
                               r8 {8}, r9 {9}, r10{10}, r11{11}, r12{12}, r13{13}, r14{14}, r15{15},
                                                                 r0{132}, rip{133};

    struct ptr
    {
        struct base  { const r64 reg; const s32 add; constexpr base (r64 r, s32 a = 0) : reg(r), add(a) {} };
        struct index { const r64 reg; const i8  mul; constexpr index(r64 r, i8  m = 1) : reg(r), mul(m) {} };

        const base  a;
        const index b;
        constexpr ptr(r64  a, index b = r0) : a{a},  b{b} {}
        constexpr ptr(base a, index b = r0) : a{a},  b{b} {}
        constexpr ptr(        index b = r0) : a{r0}, b{b} {}
    };

    // `ptr` encodes `disp(base, index, scale)`. `mem` encodes the actual value at a `ptr`.
    // Thus `ptr` can only be an argument to `lea`, but `mem` may be passed to any m8/32/64
    // instruction. That's the whole difference.
    struct mem : ptr { explicit mem(ptr x) : ptr(x) {} };

    // pointers are 64-bit, but it's possible to force them into 32 bits if necessary.
    static inline s32 p32(const void *p) { return s32(i64(p)); }

    // pointer arithmetic magic
    #define _operator(op, a, b, x) \
        static inline auto operator op(a, b) -> decltype(x) { return x; }
    #define _commutative(op, a, b, x) _operator(op, a, b, x) \
                                      _operator(op, b, a, x)
    _operator   (+, r64 a, r64   b, ptr(a, b))
    _commutative(+, r64 a, s32   b, ptr::base(a, b))
    _commutative(*, r64 a, i8    b, ptr::index(a, b))
    _operator   (-, r64 a, s32   b, ptr::base(a, -b))
    _commutative(+, r64 a, const void *b, ptr::base(a, p32(b)))
    _commutative(+, ptr::base  a, ptr::index b, ptr(a, b))
    _commutative(+, ptr::base  a, s32        b, a.reg + (a.add + b))
    _commutative(+, ptr        a, s32        b, (a.a + b) + a.b)
    _operator   (+, ptr::base  a, ptr::base  b, (a.reg + b.reg) + (a.add + b.add))
    _operator   (-, ptr::base  a, s32        b, a.reg + (a.add - b))
    _operator   (-, ptr        a, s32        b, (a.a - b) + a.b)
    _commutative(*, ptr::index a, i8         b, a.reg * i8(a.mul * b))
    _commutative(*, ptr::base  a, i8         b, a.reg * b + a.add * b)
    #undef _commutative
    #undef _operator
    // pointer arithmetic magic--

    struct target { size_t offset = -1;
                    std::vector<size_t> offsets; };

    // linker needs to keep track of all existing targets, so outside code can only
    // operate with `target *`s (the actual targets are stored in a vector below).
    // unlike `typedef target* label`, this thin wrapper ensures that uninitialized
    // labels are set to NULL.
    struct label { target* tg = NULL;
             const void  * operator()(const void *b) const { return tg ? (i8 *) b + tg->offset : NULL; }
             const target* operator->() const { return tg; } };

    // A simple x86-64 assembler and linker.
    //   http://wiki.osdev.org/X86-64_Instruction_Encoding
    //   http://ref.x86asm.net/coder64-abc.html
    struct code
    {
        size_t size() const
        {
            return _code.size();
        }

        bool write(void *base) const
        {
            i8 *out = (i8 *) memcpy(base, &_code[0], size());

            for (const auto& tg : _targets) {
                if (tg.offset == (size_t) -1)
                    return false;

                s32 rel;
                for (size_t ref : tg.offsets)
                    memcpy(&out[ref], &(rel = tg.offset - ref - 4), 4);
            }

            return true;
        }

        typedef label&    lab;
        typedef condition cnd;
        code& imm8  (i8  i) { append(&i, 1); return *this; }
        code& imm32 (i32 i) { append(&i, 4); return *this; }
        code& imm64 (i64 i) { append(&i, 8); return *this; }
        code& rel32 (lab i) { init_label(i)->offsets.push_back(size()); return imm32(0); }
        code& mark  (lab i) { init_label(i)->offset = size(); return *this; }

        // NOTE: if a 2-register instruction is R/M -> R, order of registers is swapped:
        //       reg1 contains the source while reg2 specifies the destination.
        //           src    dst             REX prefix   opcode     ModR/M      immediate
        //                  /cond           [64-bit mode]           [+ disp]
        code& add   ( i8 a,  rb b) { return rex(0,    b).imm8(0x80).modrm(0, b).imm8 (a) ; }
        code& add   ( i8 a, r32 b) { return rex(0,    b).imm8(0x83).modrm(0, b).imm8 (a) ; }
        code& add   (i32 a, r32 b) { return rex(0,    b).imm8(0x81).modrm(0, b).imm32(a) ; }
        code& add   ( i8 a, r64 b) { return rex(1,    b).imm8(0x83).modrm(0, b).imm8 (a) ; }
        code& add   (i32 a, r64 b) { return rex(1,    b).imm8(0x81).modrm(0, b).imm32(a) ; }
        code& add   (r32 a, r32 b) { return rex(0, a, b).imm8(0x01).modrm(a, b)          ; }
        code& add   (r64 a, r64 b) { return rex(1, a, b).imm8(0x01).modrm(a, b)          ; }
        code& add   ( i8 a, mem b) { return rex(0,    b).imm8(0x80).modrm(0, b).imm8 (a) ; }
        code& add   (i32 a, mem b) { return rex(0,    b).imm8(0x81).modrm(0, b).imm32(a) ; }
        code& add   (r32 a, mem b) { return rex(0, a, b).imm8(0x01).modrm(a, b)          ; }
        code& add   (r64 a, mem b) { return rex(1, a, b).imm8(0x01).modrm(a, b)          ; }
        code& add   (mem a, r32 b) { return rex(0, a, b).imm8(0x03).modrm(a, b)          ; } // r/m -> r
        code& add   (mem a, r64 b) { return rex(1, a, b).imm8(0x03).modrm(a, b)          ; } // r/m -> r
        code& and_  ( i8 a,  rb b) { return rex(0,    b).imm8(0x80).modrm(4, b).imm8 (a) ; }
        code& and_  ( i8 a, r32 b) { return rex(0,    b).imm8(0x83).modrm(4, b).imm8 (a) ; }
        code& and_  ( i8 a, r64 b) { return rex(1,    b).imm8(0x83).modrm(4, b).imm8 (a) ; }
        code& and_  (i32 a, r32 b) { return rex(0,    b).imm8(0x81).modrm(4, b).imm32(a) ; }
        code& and_  (i32 a, r64 b) { return rex(1,    b).imm8(0x81).modrm(4, b).imm32(a) ; }
        code& and_  (i32 a, mem b) { return rex(0,    b).imm8(0x81).modrm(4, b).imm32(a) ; }
        code& and_  ( i8 a, mem b) { return rex(0,    b).imm8(0x80).modrm(4, b).imm8 (a) ; }
        code& call  (i32 a       ) { return              imm8(0xe8).            imm32(a) ; }
        code& call  (lab a       ) { return              imm8(0xe8).            rel32(a) ; }
        code& call  (       r64 b) { return rex(0,    b).imm8(0xff).modrm(2, b)          ; }
        code& cmp   ( i8 a,  rb b) { return rex(0,    b).imm8(0x80).modrm(7, b).imm8 (a) ; }
        code& cmp   ( i8 a, r32 b) { return rex(0,    b).imm8(0x83).modrm(7, b).imm8 (a) ; }
        code& cmp   (i32 a, r32 b) { return rex(0,    b).imm8(0x81).modrm(7, b).imm32(a) ; }
        code& cmp   (r32 a, r32 b) { return rex(0, a, b).imm8(0x39).modrm(a, b)          ; }
        code& cmp   (r64 a, r64 b) { return rex(1, a, b).imm8(0x39).modrm(a, b)          ; }
        code& cmp   ( i8 a, mem b) { return rex(0,    b).imm8(0x80).modrm(7, b).imm8 (a) ; }
        code& cmp   (i32 a, mem b) { return rex(0,    b).imm8(0x81).modrm(7, b).imm32(a) ; }
        code& cmp   (r32 a, mem b) { return rex(0, a, b).imm8(0x39).modrm(a, b)          ; }
        code& cmp   (r64 a, mem b) { return rex(1, a, b).imm8(0x39).modrm(a, b)          ; }
        code& cmpsb (            ) { return              imm8(0xa6)                      ; }
        code& dec   (       r32 b) { return rex(0,    b).imm8(0xff).modrm(1, b)          ; }
        code& dec   (       r64 b) { return rex(1,    b).imm8(0xff).modrm(1, b)          ; }
        code& decl  (       mem b) { return rex(0,    b).imm8(0xff).modrm(1, b)          ; }
        code& decq  (       mem b) { return rex(1,    b).imm8(0xff).modrm(1, b)          ; }
        code& inc   (       r32 b) { return rex(0,    b).imm8(0xff).modrm(0, b)          ; }
        code& inc   (       r64 b) { return rex(1,    b).imm8(0xff).modrm(0, b)          ; }
        code& incl  (       mem b) { return rex(0,    b).imm8(0xff).modrm(0, b)          ; }
        code& incq  (       mem b) { return rex(1,    b).imm8(0xff).modrm(0, b)          ; }
        code& jmp   (i32 a       ) { return              imm8(0xe9).            imm32(a) ; }
        code& jmp   (lab a       ) { return              imm8(0xe9).            rel32(a) ; }
        code& jmp   (i32 a, cnd b) { return   imm8(0x0f).imm8(0x80 | b).        imm32(a) ; }
        code& jmp   (lab a, cnd b) { return   imm8(0x0f).imm8(0x80 | b).        rel32(a) ; }
        code& jmp   (       r64 b) { return rex(0,    b).imm8(0xff).modrm(4, b)          ; }
        code& mov   (i32 a, r32 b) { return rex(0,    b).imm8(0xb8 | b.L()).    imm32(a) ; }
        code& mov   (i32 a, r64 b) { return rex(1,    b).imm8(0xc7).modrm(0, b).imm32(a) ; }
        code& mov   (i64 a, r64 b) { return // if upper dword is 0, no need to waste space.
                                  a >> 32 ? rex(1,    b).imm8(0xb8 | b.L()).    imm64(a)
                                          : rex(0,    b).imm8(0xb8 | b.L()).    imm32(a) ; }
        code& movzb ( rb a, r32 b) { return rex(0, b, a).imm8(0x0f)
                                                        .imm8(0xb6).modrm(b, a)          ; }  // r/m -> r
        code& mov   (r32 a, r32 b) { return rex(0, a, b).imm8(0x89).modrm(a, b)          ; }
        code& mov   (r64 a, r64 b) { return rex(1, a, b).imm8(0x89).modrm(a, b)          ; }
        code& movsl (r32 a, r64 b) { return rex(1, b, a).imm8(0x63).modrm(b, a)          ; }  // r/m -> r
        code& mov   (i32 a, mem b) { return rex(1,    b).imm8(0xc7).modrm(0, b).imm32(a) ; }
        code& mov   (r32 a, mem b) { return rex(0, a, b).imm8(0x89).modrm(a, b)          ; }
        code& mov   (r64 a, mem b) { return rex(1, a, b).imm8(0x89).modrm(a, b)          ; }
        code& mov   (mem a,  rb b) { return rex(0, a, b).imm8(0x8a).modrm(a, b)          ; }  // r/m -> r
        code& movzb (mem a, r32 b) { return rex(0, b, a).imm8(0x0f)
                                                        .imm8(0xb6).modrm(b, a)          ; }  // r/m -> r
        code& mov   (mem a, r32 b) { return rex(0, a, b).imm8(0x8b).modrm(a, b)          ; }  // r/m -> r
        code& mov   (mem a, r64 b) { return rex(1, a, b).imm8(0x8b).modrm(a, b)          ; }  // r/m -> r
        code& movsl (mem a, r64 b) { return rex(1, a, b).imm8(0x63).modrm(a, b)          ; }  // r/m -> r
        code& mov   (ptr a, r32 b) { return rex(0, a, b).imm8(0x8d).modrm(a, b) /* lea */; }
        code& mov   (ptr a, r64 b) { return rex(1, a, b).imm8(0x8d).modrm(a, b) /* lea */; }
        code& mov   (lab a, r64 b) { return rex(1,    b).imm8(0x8d).imm8(b.L() << 3 | rip.L())
                                                          /* lea a(%rip), b */ .rel32(a) ; }
        code& neg   (       r32 b) { return rex(0,    b).imm8(0xf7).modrm(3, b)          ; }
        code& neg   (       r64 b) { return rex(1,    b).imm8(0xf7).modrm(3, b)          ; }
        code& negl  (       mem b) { return rex(0,    b).imm8(0xf7).modrm(3, b)          ; }
        code& negq  (       mem b) { return rex(1,    b).imm8(0xf7).modrm(3, b)          ; }
        code& not_  (       r32 b) { return rex(0,    b).imm8(0xf7).modrm(2, b)          ; }
        code& not_  (       r64 b) { return rex(1,    b).imm8(0xf7).modrm(2, b)          ; }
        code& notl  (       mem b) { return rex(0,    b).imm8(0xf7).modrm(2, b)          ; }
        code& notq  (       mem b) { return rex(1,    b).imm8(0xf7).modrm(2, b)          ; }
        code& or_   ( i8 a,  rb b) { return rex(0,    b).imm8(0x80).modrm(1, b).imm8 (a) ; }
        code& or_   ( i8 a, r32 b) { return rex(0,    b).imm8(0x83).modrm(1, b).imm8 (a) ; }
        code& or_   ( i8 a, r64 b) { return rex(1,    b).imm8(0x83).modrm(1, b).imm8 (a) ; }
        code& or_   (i32 a, r32 b) { return rex(0,    b).imm8(0x81).modrm(1, b).imm32(a) ; }
        code& or_   (i32 a, r64 b) { return rex(1,    b).imm8(0x81).modrm(1, b).imm32(a) ; }
        code& or_   (i32 a, mem b) { return rex(0,    b).imm8(0x81).modrm(1, b).imm32(a) ; }
        code& or_   ( i8 a, mem b) { return rex(0,    b).imm8(0x80).modrm(1, b).imm8 (a) ; }
        code& pop   (       r64 b) { return rex(0,    b).imm8(0x58 | b.L())              ; }
        code& pop   (       mem b) { return rex(0,    b).imm8(0x8f).modrm(0, b)          ; }
        code& push  (       r64 b) { return rex(0,    b).imm8(0x50 | b.L())              ; }
        code& push  (       mem b) { return rex(0,    b).imm8(0xff).modrm(6, b)          ; }
        code& repz  (            ) { return              imm8(0xf3)                      ; }
        code& ret   (            ) { return              imm8(0xc3)                      ; }
        code& sar   ( i8 a, r32 b) { return rex(0,    b).imm8(0xc1).modrm(7, b).imm8 (a) ; }
        code& sar   ( i8 a, r64 b) { return rex(1,    b).imm8(0xc1).modrm(7, b).imm8 (a) ; }
        code& shl   ( i8 a, r32 b) { return rex(0,    b).imm8(0xc1).modrm(4, b).imm8 (a) ; }
        code& shl   ( i8 a, r64 b) { return rex(1,    b).imm8(0xc1).modrm(4, b).imm8 (a) ; }
        code& shr   ( i8 a, r32 b) { return rex(0,    b).imm8(0xc1).modrm(5, b).imm8 (a) ; }
        code& shr   ( i8 a, r64 b) { return rex(1,    b).imm8(0xc1).modrm(5, b).imm8 (a) ; }
        code& sub   ( i8 a,  rb b) { return rex(0,    b).imm8(0x80).modrm(5, b).imm8 (a) ; }
        code& sub   ( i8 a, r32 b) { return rex(0,    b).imm8(0x83).modrm(5, b).imm8 (a) ; }
        code& sub   (i32 a, r32 b) { return rex(0,    b).imm8(0x81).modrm(5, b).imm32(a) ; }
        code& sub   ( i8 a, r64 b) { return rex(1,    b).imm8(0x83).modrm(5, b).imm8 (a) ; }
        code& sub   (i32 a, r64 b) { return rex(1,    b).imm8(0x81).modrm(5, b).imm32(a) ; }
        code& sub   (r32 a, r32 b) { return rex(0, a, b).imm8(0x29).modrm(a, b)          ; }
        code& sub   (r64 a, r64 b) { return rex(1, a, b).imm8(0x29).modrm(a, b)          ; }
        code& sub   ( i8 a, mem b) { return rex(0,    b).imm8(0x80).modrm(5, b).imm8 (a) ; }
        code& sub   (i32 a, mem b) { return rex(0,    b).imm8(0x81).modrm(5, b).imm32(a) ; }
        code& sub   (r32 a, mem b) { return rex(0, a, b).imm8(0x29).modrm(a, b)          ; }
        code& sub   (r64 a, mem b) { return rex(1, a, b).imm8(0x29).modrm(a, b)          ; }
        code& sub   (mem a, r32 b) { return rex(0, a, b).imm8(0x2b).modrm(a, b)          ; }  // r/m -> r
        code& sub   (mem a, r64 b) { return rex(1, a, b).imm8(0x2b).modrm(a, b)          ; }  // r/m -> r
        code& test  (i32 a, r32 b) { return rex(0,    b).imm8(0xf7).modrm(0, b).imm32(a) ; }
        code& test  (i32 a, r64 b) { return rex(1,    b).imm8(0xf7).modrm(0, b).imm32(a) ; }
        code& test  (r32 a, r32 b) { return rex(0, a, b).imm8(0x85).modrm(a, b)          ; }
        code& test  (r64 a, r64 b) { return rex(1, a, b).imm8(0x85).modrm(a, b)          ; }
        code& test  ( i8 a, mem b) { return rex(0,    b).imm8(0xf6).modrm(0, b).imm8 (a) ; }
        code& test  (i32 a, mem b) { return rex(0,    b).imm8(0xf7).modrm(0, b).imm32(a) ; }
        code& ud2   (            ) { return   imm8(0x0f).imm8(0x0b)                      ; }
        code& xor_  ( i8 a,  rb b) { return rex(0,    b).imm8(0x80).modrm(6, b).imm8 (a) ; }
        code& xor_  ( i8 a, r32 b) { return rex(0,    b).imm8(0x83).modrm(6, b).imm8 (a) ; }
        code& xor_  ( i8 a, r64 b) { return rex(1,    b).imm8(0x83).modrm(6, b).imm8 (a) ; }
        code& xor_  (i32 a, r32 b) { return rex(0,    b).imm8(0x81).modrm(6, b).imm32(a) ; }
        code& xor_  (i32 a, r64 b) { return rex(1,    b).imm8(0x81).modrm(6, b).imm32(a) ; }
        code& xor_  (i32 a, mem b) { return rex(0,    b).imm8(0x81).modrm(6, b).imm32(a) ; }
        code& xor_  ( i8 a, mem b) { return rex(0,    b).imm8(0x80).modrm(6, b).imm8 (a) ; }
        code& xor_  (r32 a, r32 b) { return rex(0, a, b).imm8(0x31).modrm(a, b)          ; }
        code& xor_  (r64 a, r64 b) { return rex(1, a, b).imm8(0x31).modrm(a, b)          ; }

        code& mov   (r32 a, r32 b, cnd c) { return rex(0, b, a).imm8(0x0f).imm8(0x40 | c).modrm(b, a); }  // r/m -> r
        code& mov   (r64 a, r64 b, cnd c) { return rex(1, b, a).imm8(0x0f).imm8(0x40 | c).modrm(b, a); }  // r/m -> r
        code& mov   (mem a, r32 b, cnd c) { return rex(0, b, a).imm8(0x0f).imm8(0x40 | c).modrm(b, a); }  // r/m -> r
        code& mov   (mem a, r64 b, cnd c) { return rex(1, b, a).imm8(0x0f).imm8(0x40 | c).modrm(b, a); }  // r/m -> r

        // shorthands for indirect jumps to 64-bit (ok, 48-bit) pointers.
        template <typename T> code& mov   (T* p, r64 b) { return mov(i64(p), b); }
        template <typename T> code& jmp   (T* p) { return mov(p, r10).jmp  (r10); }
        template <typename T> code& call  (T* p) { return mov(p, r10).call (r10); }

        protected:
            std::vector<i8> _code;
            std::deque<target> _targets;

            void append(void *p, size_t sz)
            {
                _code.insert(_code.end(), (i8 *) p, (i8 *) p + sz);
            }

            target* init_label(label& i)
            {
                if (i.tg != NULL) return i.tg;
                _targets.emplace_back();
                return i.tg = &_targets.back();
            }

            code& rex(i8 w,        ptr b) { return rex(w,     0, b.a.reg.H(), b.b.reg.H()); }
            code& rex(i8 w, reg r, ptr b) { return rex(w, r.H(), b.a.reg.H(), b.b.reg.H()); }
            code& rex(i8 w, ptr r, reg b) { return rex(w, b.H(), r.a.reg.H(), r.b.reg.H()); }
            code& rex(i8 w, reg r, reg b) { return rex(w, r.H(),       b.H(),           0); }
            code& rex(i8 w,        reg b) { return rex(w,     0,       b.H(),           0); }
            code& rex(i8 w,  i8 r,  i8 b, i8 x)
            {
                //     /--- opcode is 64-bit
                //     |   /--- additional significant bit for modr/m reg2 field
                //     |   |   /-- same for index register
                //     |   |   |   /--- same for reg1 (see below)
                return w + r + x + b ? imm8(0x40 | w << 3 | r << 2 | x << 1 | b) : *this;
            }

            code& modrm(reg a, reg b) { return modrm(a.L(), b); }
            code& modrm(reg a, ptr b) { return modrm(a.L(), b); }
            code& modrm(ptr a, reg b) { return modrm(b.L(), a); }
            code& modrm( i8 a, reg r) { return imm8(0xc0 | a << 3 | r.L()); }
            code& modrm( i8 a, ptr m) {
                size_t ref = _code.size();
                // _code[ref] is the ModR/M byte:
                //    0 1 2 3 4 5 6 7
                //    | | |   | \---/----- register 1
                //    | | \---/----- opcode extension (only for single-register opcodes) or register 2
                //    \-/----- mode (0, 1, or 2; 3 means "raw value from register" and is encoded above)
                imm8(a << 3 | m.a.reg.L());

                if (m.a.reg == rip)
                    // mode = 0 with reg1 = rbp/rip/r13 (0b101) means `disp32(%rip)`
                    // it's not possible to use rip-relative addressing in any other way.
                    return imm32((i32) m.a.add);

                if (m.a.reg == r0 || m.a.reg.L() == rsp.L() || m.b.reg != r0) {
                    // rsp's encoding (100) in reg1 means "use SIB byte".
                    _code[ref] = (_code[ref] & ~7) | r0.L();
                    // SIB byte:
                    //    0 1 2 3 4 5 6 7
                    //    | | |   | \---/--- base register
                    //    | | \---/--- index register; %rsp if none
                    //    \-/--- index scale: result = base + index * (2 ** scale) + disp
                    i8 sib = m.b.reg.L() << 3 | (m.b.mul == 2 ? 0x40
                                               : m.b.mul == 4 ? 0x80
                                               : m.b.mul == 8 ? 0xc0 : 0);

                    if (m.a.reg == r0)
                        // %rbp as base in mode 0 means no base at all, only 32-bit absolute address
                        return imm8(sib | rbp.L()).imm32((i32) m.a.add);

                    imm8(sib | m.a.reg.L());
                }

                if (m.a.add == 0 && m.a.reg != rbp && m.a.reg != r13)
                    // mode = 0 means `(reg1)`, i.e. no displacement unless base is rbp/r13.
                    return *this;

                if (-128 <= m.a.add && m.a.add < 128) {
                    _code[ref] |= 0x40;  // mode = 1 -- 8-bit displacement.
                    return imm8((i8) m.a.add);
                }

                _code[ref] |= 0x80;  // mode = 2 -- 32-bit displacement
                return imm32((i32) m.a.add);
            }
    };
}

#endif
