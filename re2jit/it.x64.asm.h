#include <deque>
#include <vector>
#include <string.h>
#include <stddef.h>
#include <stdint.h>

// A simple x86-64 assembler and linker.
//   http://wiki.osdev.org/X86-64_Instruction_Encoding
//   http://ref.x86asm.net/coder64-abc.html
struct as
{
    typedef uint8_t  i8;
    typedef uint32_t i32;
    typedef uint64_t i64;
    typedef  int32_t s32;

    enum rb  : i8 {  al,  cl,  dl,  bl, };
    enum r32 : i8 { eax, ecx, edx, ebx, esp, ebp, esi, edi, };
    enum r64 : i8 { rax, rcx, rdx, rbx, rsp, rbp, rsi, rdi, r8, r9, r10, r11, r12, r13, r14, r15, r0 = 0x80, rip = r0 | rbp };

    enum cond : i8  // pass as second argument to jmp to create a conditional jump.
    {
        less_u       = 0x2,
        more_equal_u = 0x3,
        equal        = 0x4,
        zero         = equal,
        not_equal    = 0x5,
        not_zero     = not_equal,
        less_equal_u = 0x6,
        more_u       = 0x7,
        less         = 0xc,
        more_equal   = 0xd,
        less_equal   = 0xe,
        more         = 0xf,
    };

    struct ptr
    {
        struct base {
            r64 reg;
            s32 add;
            base(r64 _r = r0, s32 _a = 0) : reg(_r), add(_a) {}
        } a;

        struct index
        {
            r64 reg;
            i8  mul;
            index(r64 _r = r0, i8 _m = 1) : reg(_r), mul(_m) {}
        } b;

        ptr(r64   _a, index _b = r0) : a{_a},    b{_b} {}
        ptr(base  _a, index _b = r0) : a{_a},    b{_b} {}
        ptr(          index _b = r0) : a{r0, 0}, b{_b} {}
    };

    // `ptr` encodes `disp(base, index, scale)`. `mem` encodes the actual value at a `ptr`.
    // Thus `ptr` can only be an argument to `lea`, but `mem` may be passed to any m8/32/64
    // instruction. That's the whole difference.
    struct mem : ptr { explicit mem(ptr x) : ptr(x) {} };

    struct target { size_t offset = -1;
                    std::vector<size_t> abs64;
                    std::vector<size_t> rel32; };

    // linker needs to keep track of all existing targets, so outside code can only
    // operate with `target *`s (the actual targets are stored in a vector below).
    // unlike `typedef target* label`, this thin wrapper ensures that uninitialized
    // labels are set to NULL.
    struct label { target* tg = NULL;
                   target* operator->() { return tg; } };

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

            i64 abs = (i64) (out + tg.offset);
            s32 rel;

            for (size_t ref : tg.abs64)
                memcpy(&out[ref], &abs, 8);

            for (size_t ref : tg.rel32)
                memcpy(&out[ref], &(rel = tg.offset - ref - 4), 4);
        }

        return true;
    }

    #define LAB label&
    as& imm8  (i8  i) { append(&i, 1); return *this; }
    as& imm32 (i32 i) { append(&i, 4); return *this; }
    as& imm64 (i64 i) { append(&i, 8); return *this; }

    as& mark  (LAB i) { init_label(i)->offset = size(); return *this; }
    as& rel32 (LAB i) { init_label(i)->rel32.push_back(size()); return imm32(0); }
    as& abs64 (LAB i) { init_label(i)->abs64.push_back(size()); return imm64(0); }

    //         src    dst             REX prefix   opcode     ModR/M      immediate
    //                /cond           [64-bit mode]           [+ disp]
    as& add   ( i8 a,  rb b) { return              imm8(0x80).modrm(0, b).imm8 (a) ; }
    as& add   (r32 a, r32 b) { return              imm8(0x01).modrm(a, b)          ; }
    as& add   ( i8 a, r64 b) { return rex(1, 0, b).imm8(0x83).modrm(0, b).imm8 (a) ; }
    as& add   (i32 a, r64 b) { return rex(1, 0, b).imm8(0x81).modrm(0, b).imm32(a) ; }
    as& add   (r64 a, r64 b) { return rex(1, a, b).imm8(0x01).modrm(a, b)          ; }
    as& and_  ( i8 a,  rb b) { return              imm8(0x80).modrm(4, b).imm8 (a) ; }
    as& call  (i32 a       ) { return              imm8(0xe8).            imm32(a) ; }
    as& call  (LAB a       ) { return              imm8(0xe8).            rel32(a) ; }
    as& call  (       r64 b) { return rex(0, 0, b).imm8(0xff).modrm(2, b)          ; }
    as& cmp   ( i8 a,  rb b) { return              imm8(0x80).modrm(7, b).imm8 (a) ; }
    as& cmp   ( i8 a, r32 b) { return              imm8(0x83).modrm(7, b).imm8 (a) ; }
    as& cmp   ( i8 a, mem b) { return rex(0, 0, b).imm8(0x83).modrm(7, b).imm8 (a) ; }
    as& cmp   (i32 a, mem b) { return rex(0, 0, b).imm8(0x81).modrm(7, b).imm32(a) ; }
    as& cmp   (r64 a, mem b) { return rex(1, a, b).imm8(0x39).modrm(a, b)          ; }
    as& cmpsb (            ) { return              imm8(0xa6)                      ; }
    as& inc   (       r64 b) { return rex(1, 0, b).imm8(0xff).modrm(0, b)          ; }
    as& jmp   (i32 a       ) { return              imm8(0xe9).            imm32(a) ; }
    as& jmp   (LAB a       ) { return              imm8(0xe9).            rel32(a) ; }
    as& jmp   (i32 a,  i8 b) { return   imm8(0x0f).imm8(0x80 | b).        imm32(a) ; }
    as& jmp   (LAB a,  i8 b) { return   imm8(0x0f).imm8(0x80 | b).        rel32(a) ; }
    as& jmp   (       r64 b) { return rex(0, 0, b).imm8(0xff).modrm(4, b)          ; }
    as& mov   (i32 a, r32 b) { return              imm8(0xb8 | b).        imm32(a) ; }
    as& mov   (i32 a, r64 b) { return rex(1, 0, b).imm8(0xc7).modrm(0, b).imm32(a) ; }
    as& mov   (i64 a, r64 b) { return rex(1, 0, b).imm8(0xb8 | b).        imm64(a) ; }
    as& mov   (LAB a, r64 b) { return rex(1, 0, b).imm8(0xb8 | b).        abs64(a) ; }
    as& mov   (r32 a, r32 b) { return              imm8(0x89).modrm(a, b)          ; }
    as& mov   (r64 a, r64 b) { return rex(1, a, b).imm8(0x89).modrm(a, b)          ; }
    as& mov   (r32 a, mem b) { return rex(0, 0, b).imm8(0x89).modrm(a, b)          ; }
    as& mov   (r64 a, mem b) { return rex(1, a, b).imm8(0x89).modrm(a, b)          ; }
    // NOTE: MOV ptr, reg is actually LEA mem, reg
    as& mov   (ptr a, r32 b) { return rex(0, a, b).imm8(0x8d).modrm(a, b)          ; }
    as& mov   (ptr a, r64 b) { return rex(1, a, b).imm8(0x8d).modrm(a, b)          ; }
    as& mov   (mem a,  rb b) { return rex(0, a, b).imm8(0x8a).modrm(a, b)          ; }
    as& mov   (mem a, r32 b) { return rex(0, a, b).imm8(0x8b).modrm(a, b)          ; }
    as& mov   (mem a, r64 b) { return rex(1, a, b).imm8(0x8b).modrm(a, b)          ; }
    // NOTE: CMOVcc: ModR/M reg1/reg2 fields are swapped compared to normal MOV.
    //       mov    %eax, %ecx  ->       0x89 [0xc1] (= 0b11 000 001)
    //       cmovbe %eax, %ecx  ->  0x0f 0x46 [0xc8] (= 0b11 001 000)
    // This is because mov is either r -> r/m or m -> r while cmovcc is r/m -> r.
    as& mov   (r32 a, r32 b,
                       i8 c) { return   imm8(0x0f).imm8(0x40 | c).modrm(b, a)      ; }
    as& not_  (       r32 b) { return              imm8(0xf7).modrm(2, b)          ; }
    as& or_   ( i8 a, mem b) { return rex(0, 0, b).imm8(0x80).modrm(1, b).imm8 (a) ; }
    as& pop   (       r64 b) { return rex(0, 0, b).imm8(0x58 | (b & 7))            ; }
    as& push  (       r64 b) { return rex(0, 0, b).imm8(0x50 | (b & 7))            ; }
    as& repz  (            ) { return              imm8(0xf3)                      ; }
    as& ret   (            ) { return              imm8(0xc3)                      ; }
    as& shr   ( i8 a, r64 b) { return rex(1, 0, b).imm8(0xc1).modrm(5, b).imm8 (a) ; }
    as& sub   ( i8 a,  rb b) { return              imm8(0x80).modrm(5, b).imm8 (a) ; }
    as& sub   (r32 a, r32 b) { return              imm8(0x29).modrm(a, b)          ; }
    as& sub   (r64 a, r64 b) { return rex(1, a, b).imm8(0x29).modrm(a, b)          ; }
    as& sub   (mem a, r64 b) { return rex(1, a, b).imm8(0x2b).modrm(a, b)          ; }
    as& sub   (r64 a, mem b) { return rex(1, a, b).imm8(0x29).modrm(a, b)          ; }
    as& test  (i32 a, r32 b) { return              imm8(0xf7).modrm(0, b).imm32(a) ; }
    as& test  (r32 a, r32 b) { return              imm8(0x85).modrm(a, b)          ; }
    as& test  ( i8 a, mem b) { return rex(0, 0, b).imm8(0xf6).modrm(0, b).imm8 (a) ; }
    as& test  (i32 a, mem b) { return rex(0, 0, b).imm8(0xf7).modrm(0, b).imm32(a) ; }
    as& xor_  (r32 a, r32 b) { return              imm8(0x31).modrm(a, b)          ; }

    // shorthands for indirect jumps to 64-bit (ok, 48-bit) pointers.
    template <typename T> as& jmp   (T *p) { return mov((i64) p, r10).jmp  (r10); }
    template <typename T> as& call  (T *p) { return mov((i64) p, r10).call (r10); }
    #undef LAB

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

        as& rex(i8 w, i8 r, mem b) { return rex(w, r, b.a.reg, b.b.reg); }
        as& rex(i8 w, ptr b, i8 r) { return rex(w, r, b.a.reg, b.b.reg); }
        as& rex(i8 w, mem b, i8 r) { return rex(w, r, b.a.reg, b.b.reg); }
        as& rex(i8 w, i8 r, i8 b, i8 x = 0)
        {
            //     /--- fixed value
            //     |      /--- opcode is 64-bit
            //     |      |         /--- additional significant bit for modr/m reg2 field
            //     |      |         |              /-- same for index register
            //     |      |         |              |              /--- same for reg1 (see below)
            i8 f = 0x40 | w << 3 | (r & 8) >> 1 | (x & 8) >> 2 | (b & 8) >> 3;
            //          /--- REX with all zero flags is ignored
            return f == 0x40 ? *this : imm8(f);
        }

        as& modrm( i8 a,  i8 b) { return imm8((a & 7) << 3 | (b & 7) | 0xc0); }
        as& modrm( i8 a, mem b) { return imm8((a & 7) << 3 | (b.a.reg & 7)).modrm(b); }
        as& modrm(mem a,  i8 b) { return modrm(b, a); }
        as& modrm(ptr a,  i8 b) { return modrm(b, as::mem(a)); }
        as& modrm(mem m) {
            i8& ref = _code[_code.size() - 1];
            // ref is the ModR/M byte:
            //    0 1 2 3 4 5 6 7
            //    | | |   | \---/----- register 1
            //    | | \---/----- opcode extension (only for single-register opcodes) or register 2
            //    \-/----- mode (0, 1, or 2; 3 means "raw value from register" and is encoded above)
            if (m.a.reg == rip)
                // mode = 0 with reg1 = rbp/rip/r13 (0b101) means `disp32(%rip)`
                // it's not possible to use rip-relative addressing in any other way.
                return imm32((i32) m.a.add);

            if (m.a.reg == r0 || m.b.reg != r0 || m.a.reg == rsp) {
                // rsp's encoding (100) in reg1 means "use SIB byte".
                ref = (ref & ~7) | rsp;
                // SIB byte:
                //    0 1 2 3 4 5 6 7
                //    | | |   | \---/--- base register
                //    | | \---/--- index register; %rsp if none
                //    \-/--- index scale: result = base + index * (2 ** scale) + disp
                i8 sib = rsp << 3;

                if (m.b.reg != r0) switch (m.b.mul) {
                    case 1: sib = (m.b.reg & 7) << 3;        break;
                    case 2: sib = (m.b.reg & 7) << 3 | 0x40; break;
                    case 4: sib = (m.b.reg & 7) << 3 | 0x80; break;
                    case 8: sib = (m.b.reg & 7) << 3 | 0xc0; break;
                }

                if (m.a.reg == r0)
                    // %rbp as base in mode 0 means no base at all, only 32-bit absolute address
                    return imm8(sib | rbp).imm32((i32) m.a.add);

                imm8(sib | (m.a.reg & 7));
            }

            if (m.a.add == 0 && m.a.reg != rbp && m.a.reg != r13)
                // mode = 0 means `(reg1)`, i.e. no displacement unless base is rbp/r13.
                return *this;

            if (-128 <= m.a.add && m.a.add < 128) {
                ref |= 0x40;  // mode = 1 -- 8-bit displacement.
                return imm8((i8) m.a.add);
            }

            ref |= 0x80;  // mode = 2 -- 32-bit displacement
            return imm32((i32) m.a.add);
        }
};

// pointer arithmetic magic
template <typename T> as::ptr::base  operator + (T       a, as::r64 b) { return as::ptr::base  {b,  as::s32(a)}; }
template <typename T> as::ptr::base  operator + (as::r64 a, T       b) { return as::ptr::base  {a,  as::s32(b)}; }
template <typename T> as::ptr::base  operator - (as::r64 a, T       b) { return as::ptr::base  {a, -as::s32(b)}; }
template <typename T> as::ptr::base  operator + (as::r64 a, T      *b) { return as::ptr::base  {a,  as::s32(as::i64(b))}; }
template <typename T> as::ptr::index operator * (T       a, as::r64 b) { return as::ptr::index {b,  as::i8(a)}; }
template <typename T> as::ptr::index operator * (as::r64 a, T       b) { return as::ptr::index {a,  as::i8(b)}; }

as::ptr        operator + (as::r64        a, as::r64        b) { return as::ptr {a,  b}; }
as::ptr        operator + (as::ptr::index a, as::r64        b) { return as::ptr {b,  a}; }
as::ptr        operator + (as::r64        a, as::ptr::index b) { return as::ptr {a,  b}; }
as::ptr        operator + (as::r64        a, as::ptr::base  b) { return as::ptr {b,  a}; }
as::ptr        operator + (as::ptr::base  a, as::r64        b) { return as::ptr {a,  b}; }
as::ptr        operator + (as::ptr::index a, as::ptr::base  b) { return as::ptr {b,  a}; }
as::ptr        operator + (as::ptr::base  a, as::ptr::index b) { return as::ptr {a,  b}; }

as::ptr::base  operator + (as::s32        a, as::ptr::base  b) { return b.reg + (b.add + a); }
as::ptr::base  operator + (as::ptr::base  a, as::s32        b) { return a.reg + (a.add + b); }
as::ptr::base  operator - (as::ptr::base  a, as::s32        b) { return a.reg + (a.add - b); }
as::ptr::index operator * (as::i8         a, as::ptr::index b) { return b.reg * as::i8(b.mul * a); }
as::ptr::index operator * (as::ptr::index a, as::i8         b) { return a.reg * as::i8(a.mul * b); }
as::ptr        operator + (as::s32        a, as::ptr        b) { return (b.a + a) + b.b; }
as::ptr        operator + (as::ptr        a, as::s32        b) { return (a.a + b) + a.b; }
as::ptr        operator - (as::ptr        a, as::s32        b) { return (a.a - b) + a.b; }
as::ptr        operator + (as::ptr::base  a, as::ptr::base  b) { return (a.reg + b.reg) + (a.add + b.add); }
as::ptr        operator * (as::i8         a, as::ptr::base  b) { return b.reg * a + b.add * a; }
as::ptr        operator * (as::ptr::base  a, as::i8         b) { return a.reg * b + a.add * b; }
// pointer arithmetic magic--
