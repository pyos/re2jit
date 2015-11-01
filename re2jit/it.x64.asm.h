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
    enum r64 : i8 { rax, rcx, rdx, rbx, rsp, rbp, rsi, rdi, r8, r9, r10, r11, r12, r13, r14, r15, rip = 0x80 | rbp };

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

    struct mem  // mem{rcx} + 5 -- 5(%rcx) in at&t syntax
    {
        s32 disp;  // `disp(base, index, scale)` not supported.
        r64 base;
        explicit mem(r64 _base, s32 _disp = 0) : disp(_disp), base(_base) {}

        template <typename T>
        mem operator + (T  *ptr) { return mem { base, disp + (s32) (i64) ptr }; }
        mem operator + (s32 off) { return mem { base, disp + off }; }
        mem operator - (s32 off) { return mem { base, disp - off }; }
    };

    struct target { size_t offset;
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

    void write(void *base) const
    {
        i8 *out = (i8 *) memcpy(base, &_code[0], size());

        for (const auto& tg : _targets) {
            i64 abs = (i64) (out + tg.offset);
            s32 rel;

            for (size_t ref : tg.abs64)
                memcpy(&out[ref], &abs, 8);

            for (size_t ref : tg.rel32)
                memcpy(&out[ref], &(rel = tg.offset - ref - 4), 4);
        }
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
    as& mov   (mem a,  rb b) { return rex(0, 0, a).imm8(0x8a).modrm(a, b)          ; }
    as& mov   (mem a, r32 b) { return rex(0, 0, a).imm8(0x8b).modrm(a, b)          ; }
    as& mov   (mem a, r64 b) { return rex(1, a, b).imm8(0x8b).modrm(a, b)          ; }
    as& mov   ( rb a, mem b) { return rex(0, 0, b).imm8(0x88).modrm(a, b)          ; }
    as& mov   (r32 a, mem b) { return rex(0, 0, b).imm8(0x89).modrm(a, b)          ; }
    as& mov   (r64 a, mem b) { return rex(1, a, b).imm8(0x89).modrm(a, b)          ; }
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
    template <typename T> as& jmp   (T *p) { return mov((i64) p, rax).jmp  (rax); }
    template <typename T> as& call  (T *p) { return mov((i64) p, rax).call (rax); }
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

        as& rex(i8 w, i8 r, mem b) { return rex(w, r, b.base); }
        as& rex(i8 w, mem b, i8 r) { return rex(w, r, b.base); }
        as& rex(i8 w, i8 r /*, i8 x -- index register, not supported */, i8 b)
        {
            //     /--- fixed value
            //     |      /--- opcode is 64-bit
            //     |      |         /--- additional significant bit for modr/m reg2 field
            //     |      |         |               /--- same for reg1 (see below)
            i8 f = 0x40 | w << 3 | (r & 8) >> 1 | (b & 8) >> 3;
            //          /--- REX with all zero flags is ignored
            return f == 0x40 ? *this : imm8(f);
        }

        as& modrm( i8 a,  i8 b) { return imm8((a & 7) << 3 | (b & 7) | 0xc0); }
        as& modrm( i8 a, mem b) { return imm8((a & 7) << 3 | (b.base & 7)).modrm(b); }
        as& modrm(mem a,  i8 b) { return imm8((b & 7) << 3 | (a.base & 7)).modrm(a); }
        as& modrm(mem m) {
            i8& ref = _code[_code.size() - 1];
            // ref is the ModR/M byte:
            //    0 1 2 3 4 5 6 7
            //    | | |   | \---/----- register 1
            //    | | \---/----- opcode extension (only for single-register opcodes) or register 2
            //    \-/----- mode (0, 1, or 2; 3 means "raw value from register" and is encoded above)
            if (m.base == rsp)
                // rsp's encoding (100) in reg1 means "use SIB byte", so that's how we'll encode it:
                //   SIB = 00 100 100 = (%rsp, %rsp, 1)
                imm8(0x24);

            if (m.base == rip)
                // mode = 0 with reg1 = rbp/rip/r13 (0b101) means `disp32(%rip)`
                return imm32((i32) m.disp);

            if (m.disp == 0 && m.base != rbp && m.base != r13)
                // mode = 0 otherwise means `(reg1)`, i.e. no displacement
                return *this;

            if (-128 <= m.disp && m.disp < 128) {
                ref |= 0x40;  // mode = 1 -- 8-bit displacement.
                return imm8((i8) m.disp);
            }

            ref |= 0x80;  // mode = 2 -- 32-bit displacement
            return imm32((i32) m.disp);
        }
};
