// note down that `$+offset` should be linked to a vtable index.
// see compiler in it.x64.cc for definition of `backrefs`, `vtable`, and `code`.
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
#define MOVQ_IMM_RSI(p) INSCODE(0x48, 0xBE, IMM64(p))
#define MOVQ_TBL_RSI(p) INSBACK(p, 2); MOVQ_IMM_RSI(0ULL)


// mov m64=8/32(r64), r8/32/64
#define MOV__MRAX__CL()  INSCODE(      0x8a, 0x08)
#define MOVB_MRDI_EAX(p) INSCODE(      0x8b, 0x47, IMM8(p, 0))
#define MOVB_MRDI_RAX(p) INSCODE(0x48, 0x8b, 0x47, IMM8(p, 0))
#define MOVB_MRDI_RCX(p) INSCODE(0x48, 0x8b, 0x4f, IMM8(p, 0))
#define MOVB_MRDI_RSI(p) INSCODE(0x48, 0x8b, 0x77, IMM8(p, 0))


// mov r32/64, m64=imm8/32(r64)
#define MOVB_EAX_MRCX(p) INSCODE(      0x89, 0x41, IMM8(p, 0))
#define MOVL_EAX_MRCX(p) INSCODE(      0x89, 0x81, IMM32(p))


// cmp imm8/32, r8/64
#define CMPB_IMM__CL(p) INSCODE(      0x80, 0xF9, IMM8(p, 0))


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
#define ORB_IMM_MRSI(imm, d) INSCODE(0x80, 0x8E, IMM32(d), IMM8(imm, 0))

// stack r64
#define PUSH_RDI() INSCODE(0x57)
#define POP_RDI()  INSCODE(0x5F)

// call imm32, relative near
#define CALL_REL(p) INSCODE(0xE8, IMM32(p))
#define CALL_TBL(k) INSBACK(k, 1); CALL_REL(0)

// call/jump imm32, absolute indirect through %rax
#define CALL_IMM(p) MOVL_IMM_RAX((uint64_t) p); INSCODE(0xFF, 0xD0)
#define JMPL_IMM(p) MOVL_IMM_RAX((uint64_t) p); INSCODE(0xFF, 0xE0)

// jump imm32, relative near
#define JMP_UNCOND_REL(p) INSCODE(0xE9, IMM32(p))  // rel. to next opcode
#define JMP_UNCOND_TBL(k) INSBACK(k, 1); JMP_UNCOND_REL(0L)  // abs. to vtable entry
#define JMP_UNCOND_ABS(p) JMP_UNCOND_REL(p - code.size() - 5)  // rel. to code start
#define RETQ() INSCODE(0xC3)

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
