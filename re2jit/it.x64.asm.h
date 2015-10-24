// this is complete bullshit.
struct as
{
    typedef uint8_t  i8;
    typedef uint32_t i32;
    typedef uint64_t i64;

    // http://wiki.osdev.org/X86-64_Instruction_Encoding
    // http://ref.x86asm.net/coder64-abc.html
    enum rb  : i8 {  al,  cl,  dl,  bl, };
 // enum r16 is slow useless shit. use 32-bit registers.
    enum r32 : i8 { eax, ecx, edx, ebx, esp, ebp, esi, edi, };
    enum r64 : i8 { rax, rcx, rdx, rbx, rsp, rbp, rsi, rdi, r8, r9, r10, r11, r12, r13, r14, r15, rip = 0x80 | rbp };

    enum cond : i8 {
        less_u       = 0x2,  // note that `x` is opposite of `x ^ 1`
        more_equal_u = 0x3,  // pass as second argument to `jmp` for a conditional jump
        equal        = 0x4,
        equal_u      = equal,
        zero         = equal,
        not_equal    = 0x5,
        not_equal_u  = not_equal,
        not_zero     = not_equal,
        less_equal_u = 0x6,
        more_u       = 0x7,
        less         = 0xc,
        more_equal   = 0xd,
        less_equal   = 0xe,
        more         = 0xf,
    };

    struct mem { int32_t disp; r64 base;  // `+ index * offset` not supported.
           // as::mem{as::rcx}      --  (%rcx)
           // as::mem{as::rcx} + 5  -- 5(%rcx)
           explicit mem(r64 r)                : disp(0), base(r) {}
                    mem(r64 r, int32_t d)     : disp(d), base(r) {}
           mem operator + (const void *off) { return mem { base, disp + (int32_t) (uint64_t) off }; }
           mem operator + (int32_t off) { return mem { base, disp + off }; }
           mem operator - (int32_t off) { return mem { base, disp - off }; } };

    struct label
    {
        size_t offset;
        std::vector<size_t> abs64;  // where to link references to this label
        std::vector<size_t> rel32;  // (this one is relative to position + 4)
        explicit label(size_t off) : offset(off) {}
    };

    as() : _start(NULL), _last(NULL), _end(NULL) {}
   ~as() { free(_start); }

    size_t size() const { return _last - _start; }

    label& mark()
    {
        _labels.emplace_back(size());
        return _labels.back();
    }

    as& mark(label& lb) {
        lb.offset = size();
        return *this;
    }

    void write(void *base) const
    {
        uint8_t *tg = (uint8_t *) memcpy(base, _start, size());

        for (const label& lb : _labels) {
            uint8_t *t = &tg[lb.offset];

            for (size_t ref : lb.abs64)
                memcpy(&tg[ref], &t, sizeof(t));

            for (size_t ref : lb.rel32) {
                int32_t r = t - &tg[ref + 4];
                memcpy(&tg[ref], &r, sizeof(r));
            }
        }
    }

    #define LAB label&
    as& imm8  (i8  i) { memcpy(allocate(), &i, 1); _last += 1; return *this; }
    as& imm32 (i32 i) { memcpy(allocate(), &i, 4); _last += 4; return *this; }
    as& imm64 (i64 i) { memcpy(allocate(), &i, 8); _last += 8; return *this; }
    as& imm32 (LAB i) { i.rel32.push_back(size()); return imm32(0); }
    as& imm64 (LAB i) { i.abs64.push_back(size()); return imm64(0); }
    //         src    dst             REX prefix   opcode     ModR/M      immediate
    //                /cond           [64-bit mode]           [+ disp]
    as& add   ( i8 a,  rb b) { return              imm8(0x80).modrm(0, b).imm8 (a) ; }
    as& add   (r32 a, r32 b) { return              imm8(0x01).modrm(a, b)          ; }
    as& add   (r64 a, r64 b) { return rex(1, a, b).imm8(0x01).modrm(a, b)          ; }
    as& and_  ( i8 a,  rb b) { return              imm8(0x80).modrm(4, b).imm8 (a) ; }
    as& call  (i32 a       ) { return              imm8(0xe8).            imm32(a) ; }
    as& call  (LAB a       ) { return              imm8(0xe8).            imm32(a) ; }
    as& call  (       r64 b) { return rex(0, 0, b).imm8(0xff).modrm(2, b)          ; }
    as& cmp   ( i8 a,  rb b) { return              imm8(0x80).modrm(7, b).imm8 (a) ; }
    as& cmp   ( i8 a, r32 b) { return              imm8(0x83).modrm(7, b).imm8 (a) ; }
    as& cmp   ( i8 a, mem b) { return rex(0, 0, b).imm8(0x83).modrm(7, b).imm8 (a) ; }
    as& cmp   (i32 a, mem b) { return rex(0, 0, b).imm8(0x81).modrm(7, b).imm32(a) ; }
    as& cmp   (r64 a, mem b) { return rex(1, a, b).imm8(0x39).modrm(a, b)          ; }
    as& cmpsb (            ) { return              imm8(0xa6)                      ; }
    as& inc   (       r64 b) { return rex(1, 0, b).imm8(0xff).modrm(0, b)          ; }
    as& jmp   (i32 a       ) { return              imm8(0xe9).            imm32(a) ; }
    as& jmp   (LAB a       ) { return              imm8(0xe9).            imm32(a) ; }
    as& jmp   (i32 a,  i8 b) { return   imm8(0x0f).imm8(0x80 | b).        imm32(a) ; }
    as& jmp   (LAB a,  i8 b) { return   imm8(0x0f).imm8(0x80 | b).        imm32(a) ; }
    as& jmp   (       r64 b) { return rex(0, 0, b).imm8(0xff).modrm(4, b)          ; }
    as& mov   (i32 a, r32 b) { return              imm8(0xb8 | b).        imm32(a) ; }
    as& mov   (i32 a, r64 b) { return rex(1, 0, b).imm8(0xc7).modrm(0, b).imm32(a) ; }
    as& mov   (i64 a, r64 b) { return rex(1, 0, b).imm8(0xb8 | b).        imm64(a) ; }
    as& mov   (LAB a, r64 b) { return rex(1, 0, b).imm8(0xb8 | b).        imm64(a) ; }
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
    // c++ still complains if that pointer is a function pointer, though.
    as& jmp   (void *p) { return mov((uint64_t) p, rax).jmp  (rax); }
    as& call  (void *p) { return mov((uint64_t) p, rax).call (rax); }
    #undef LAB

    protected:
        as& rex(i8 w, i8 r, mem b) { return rex(w, r, b.base); }
        as& rex(i8 w, mem b, i8 r) { return rex(w, r, b.base); }
        as& rex(i8 w, i8 r /*, i8 x -- index register, not supported */, i8 b)
        {
            //     /--- fixed value
            //     |      /--- opcode is 64-bit
            //     |      |         /--- additional bit for modr/m reg1
            //     |      |         |               /--- same for reg2
            i8 f = 0x40 | w << 3 | (r & 8) >> 1 | (b & 8) >> 3;
            //          /--- REX with all zero flags is ignored
            return f == 0x40 ? *this : imm8(f);
        }

        as& modrm( i8 a,  i8 b) { return imm8((a & 7) << 3 | (b & 7) | 0xc0); }
        as& modrm( i8 a, mem b) { return imm8((a & 7) << 3 | (b.base & 7)).modrm(b); }
        as& modrm(mem a,  i8 b) { return imm8((b & 7) << 3 | (a.base & 7)).modrm(a); }
        as& modrm(mem m) {
            uint8_t *ref = _last - 1;
            // *ref is the ModR/M byte:
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

            if ((int8_t) m.disp == m.disp) {
                *ref |= 0x40;  // mode = 1 -- 8-bit displacement.
                return imm8((i8) m.disp);
            }

            *ref |= 0x80;  // mode = 2 -- 32-bit displacement
            return imm32((i32) m.disp);
        }

        uint8_t *allocate()
        {
            if (_end - _last < 8) {
                uint8_t *r = (uint8_t *) realloc(_start, (_end - _start + 8) * 2);

                if (r == NULL)
                    throw std::runtime_error("out of memory");

                _end   = r + (_end  - _start + 8) * 2;
                _last  = r + (_last - _start);
                _start = r;
            }

            return _last;
        }

        uint8_t *_start;
        uint8_t *_last;
        uint8_t *_end;
        std::deque<label> _labels;
};
