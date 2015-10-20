#ifndef RE2JIT_REWRITER_H
#define RE2JIT_REWRITER_H

#include <vector>
#include <string>

#include <re2/prog.h>
#include <re2/stringpiece.h>

#include "unicode.h"


namespace re2jit
{
    struct inst {
        enum OPCODE
        {
            kUnicodeLetter = SURROGATE_L,
            kUnicodeNumber = SURROGATE_L + 1,
        };

        inst(ssize_t opcode, ssize_t out) : opcode_(opcode), out_(out) {}

        ssize_t opcode() const { return opcode_; }
        ssize_t out()    const { return out_; }

        protected:
            ssize_t opcode_;
            ssize_t out_;
    };


    /* Single step of a rewriting algorithm: given a string, a pointer to the first
     * character of an escape sequence, and a pointer to the last character of that same
     * sequence, replace it with an UTF-8 character, and return a pointer to the last
     * byte of that character. */
    static inline std::string::iterator _rewrite_step(std::string& s,
                  std::string::iterator pos,
                  std::string::iterator end, ssize_t op)
    {
        char u8buf[3];
        int  u8len = rejit_write_utf8((uint8_t *) u8buf, op);

        ssize_t off = pos - s.begin() - 1;
        return s.replace(pos - 1, end + 1, u8buf, u8len).begin() + off + u8len - 1;
    }


    /* Rewrite a regular expression, replacing some syntax not supported by re2
     * with placeholders that our NFA implementations can then handle their own way.
     *
     * Returns `false` if any of the rewrites are backwards-incompatible with re2,
     * i.e. add new features instead of, say, replacing some things with faster
     * implementations.
     *
     */
    static inline bool rewrite(std::string& regexp)
    {
        auto src = regexp.begin();
        bool is_re2 = true;

        for (; src != regexp.end(); src++) if (*src == '\\') {
            if (++src == regexp.end())
                return false;  // invalid syntax: escape character before EOF

            switch (*src)
            {
                case 'p': {
                    // '\p{kind}' -- match a whole Unicode character class
                    auto lp = src;

                    if (++lp == regexp.end() || *lp != '{')
                        return false;  // invalid syntax: unicode class with no name

                    auto rp = lp;

                    while (*rp != '}')
                        if (++rp == regexp.end())
                            return false;  // invalid syntax: mismatched parenthesis

                    ++lp;

                    if (rp - lp == 1) switch (*lp)
                    {
                        case 'L':
                            src = _rewrite_step(regexp, src, rp, inst::kUnicodeLetter);
                            break;

                        case 'N':
                            src = _rewrite_step(regexp, src, rp, inst::kUnicodeNumber);
                            break;
                    }

                    break;
                }
            }
        }

        return is_re2;
    }


    /* Check whether the i-th opcode of a re2 program is actually a fake instruction
     * inserted by `rewrite`, meaning it is a start of a sequence of instruction
     * that match a Unicode private use character. If yes, returns the list of
     * instructions to apply; if at least one matches, then this opcode matched.
     * If not, the list is empty.
     */
    static inline std::vector<inst> is_extcode(re2::Prog *p, re2::Prog::Inst *in)
    {
        // (SURROGATE_L .. SURROGATE_H) in UTF-8 = 0xED 0xB3 0xXX where 0xXX & 0xC0 == 0x80.
        if (in->opcode() != re2::kInstByteRange || in->hi() != 0xED || in->lo() != 0xED)
            return std::vector<inst>{};

        in = p->inst(in->out());

        if (in->opcode() != re2::kInstByteRange || in->hi() != 0xB3 || in->lo() != 0xB3)
            return std::vector<inst>{};

        in = p->inst(in->out());

        // The third one is tricky. It can be a choice between any `kInstByteRange`s
        // where both limits are UTF-8 continuation bytes.
        ssize_t stack[65];
        ssize_t stptr = 0;
        std::vector<inst> rs;

        while (true) switch (in->opcode()) {
            case re2::kInstAlt:
            case re2::kInstAltMatch:
                stack[stptr++] = in->out1();
                in = p->inst(in->out());
                break;

            case re2::kInstByteRange: {
                if ((in->lo() & 0xC0) == 0x80 && (in->hi() & 0xC0) == 0x80) {
                    for (ssize_t a = in->lo(); a <= in->hi(); a++) {
                        rs.emplace_back(SURROGATE_L + (a & 0x3F), in->out());
                    }

                    if (!stptr)
                        return rs;

                    in = p->inst(stack[--stptr]);
                }
            }

            default:
                // regexp can match junk in the middle of a character. not good.
                return std::vector<inst>{};
        }
    }


    /* Execute a statement in the context of handling an instruction.
     *
     * @param var: name of the variable to store the instruction in.
     * @param prog: `re2::Prog *` to read instructions out of.
     * @param i: index of the current instruction.
     * @param ext: stmt to execute if the instruction is a re2jit extension
     *             (type of `var` is `re2jit::inst *`).
     * @param normal: stmt to execute if the instruction is a re2 opcode
     *                (type of `var` is `re2::Prog::Inst *`).
     *
     */
    #define RE2JIT_WITH_INST(var, prog, i, ext, normal) do {               \
        auto var = (prog)->inst(i);                                        \
        auto __ins = re2jit::is_extcode(prog, var);                        \
        if (__ins.size()) {                                                \
            for (auto var = __ins.begin(); var != __ins.end(); var++) ext; \
        } else {                                                           \
            normal;                                                        \
        }                                                                  \
    } while (0)
};

#endif
