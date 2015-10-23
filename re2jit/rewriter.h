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
        enum code : uint8_t
        {
            kUnicodeType,
            kBackReference,
        };

        inst(code opcode, uint16_t arg, ssize_t out) : opcode_(opcode), arg_(arg), out_(out) {}

        code     opcode() const { return opcode_; }
        uint16_t arg()    const { return arg_; }
        ssize_t  out()    const { return out_; }

        protected:
            code     opcode_;
            uint16_t arg_;
            ssize_t  out_;
    };

    /* Private Use Area on plane 15: 0xF0000 .. 0xFFFFD. We'll use 0xF0000 .. 0xF0FFF:
     *
     *     1111 0000 xxxx xxxx xxxx
     *     F    0    code \- arg -/
     *
     * All UTF-8 points of this form look like 0xF3 0xB0 0xYY 0xYY
     * where (0xYY & 0xC0) == 0x80.
     *
     */
    static constexpr const rejit_uni_char_t PSEUDOCODE = 0xF0000ull;

    /* Single step of a rewriting algorithm: given a string, a pointer to the first
     * character of an escape sequence, and a pointer to the last character of that same
     * sequence, replace it with an UTF-8 character, and return a pointer to the last
     * byte of that character. */
    static inline std::string::iterator _rewrite_step(std::string& s,
                  std::string::iterator pos,
                  std::string::iterator end, uint8_t op, uint16_t arg)
    {
        uint8_t buf[4] = { 0xF3u, 0xB0u,
                (uint8_t) (0x80u | (op << 2) | (arg >> 6)),
                (uint8_t) (0x80u | (arg & 0x3Fu)) };
        ssize_t off = pos - s.begin() - 1;
        return s.replace(pos - 1, end + 1, (char *) buf, 4).begin() + off + 3;
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

            if (*src == 'p') {
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
                        src = _rewrite_step(regexp, src, rp, inst::kUnicodeType, UNICODE_TYPE_L);
                        break;

                    case 'N':
                        src = _rewrite_step(regexp, src, rp, inst::kUnicodeType, UNICODE_TYPE_N);
                        break;
                }
            }

            else if (isdigit(*src)) {
                // \1234 -- backreference to group 1234.
                auto e = src;
                int  r = 0;

                for (; e != regexp.end() && isdigit(*e); ++e)
                    r = r * 10 + (*e - '0');

                src = _rewrite_step(regexp, src, --e, inst::kBackReference, r);
                // re2 does not support backreferences.
                is_re2 = false;
            }
        }

        return is_re2;
    }


    static inline std::vector<inst> _follow_empty(re2::Prog *p, re2::Prog::Inst *in, uint8_t op, uint16_t arg)
    {
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
                if ((in->lo() & 0xC0u) == 0x80u && (in->hi() & 0xC0u) == 0x80u) {
                    for (uint8_t a = in->lo(); a <= in->hi(); a++) {
                        rs.emplace_back((inst::code) op, arg | (a & 0x3F), in->out());
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


    /* Check whether the i-th opcode of a re2 program is actually a fake instruction
     * inserted by `rewrite`, meaning it is a start of a sequence of instruction
     * that match a Unicode private use character. If yes, returns the list of
     * instructions to apply; if at least one matches, then this opcode matched.
     * If not, the list is empty.
     */
    static inline std::vector<inst> is_extcode(re2::Prog *p, re2::Prog::Inst *in)
    {
        if (in->opcode() != re2::kInstByteRange || in->hi() != 0xF3 || in->lo() != 0xF3)
            return std::vector<inst>{};

        in = p->inst(in->out());

        if (in->opcode() != re2::kInstByteRange || in->hi() != 0xB0 || in->lo() != 0xB0)
            return std::vector<inst>{};

        in = p->inst(in->out());

        // Third and fourth ones can be any continuation byte.
        std::vector<inst> rs;

        for (inst i : _follow_empty(p, in, 0, 0)) {
            in = p->inst(i.out());

            std::vector<inst> xs = _follow_empty(p, p->inst(i.out()), i.arg() >> 2, (i.arg() & 3) << 6);

            if (xs.empty())
                return std::vector<inst>{};

            rs.insert(rs.end(), xs.begin(), xs.end());
        }

        return rs;
    }


    #define RE2JIT_WITH_INST(prog, i, vec, ext, opv, normal) do {          \
        auto opv = (prog)->inst(i);                                        \
        auto vec = re2jit::is_extcode(prog, opv);                          \
        if (vec.size()) {                                                  \
            ext;                                                           \
        } else {                                                           \
            normal;                                                        \
        }                                                                  \
    } while (0)
};

#endif
