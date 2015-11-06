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
     */
    static constexpr const uint32_t PSEUDOCODE = 0xF0000ull;

    /* Single step of a rewriting algorithm: given a string, a pointer to the first
     * character of an escape sequence, and a pointer to the last character of that same
     * sequence, replace it with an UTF-8 character, and return a pointer to the last
     * byte of that character. */
    static inline std::string::size_type _rewrite_step(std::string& s,
                  std::string::size_type pos,
                  std::string::size_type end, uint8_t op, uint16_t arg)
    {
        // 1111 0000 xxxx xxxx xxxx  ----------> 11110 011  10 110000  10 xxxxxx  10 xxxxxx
        // F    0    x    x    x        UTF-8    F   3      B    0
        uint8_t buf[4] = { 0xF3u, 0xB0u,
                (uint8_t) (0x80u | (op << 2) | (arg >> 6)),
                (uint8_t) (0x80u | (arg & 0x3Fu)) };
        s.replace(pos, end - pos, (const char *) buf, 4);
        return pos + 3;
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
        auto i = std::string::size_type();
        bool is_re2 = true;

        for (; i < regexp.size() - 1; i++)
            // backslash cannot be the last character
            if (regexp[i] == '\\') {
                if (regexp[i + 1] == 'p') {
                    // '\p{kind}' -- match a whole Unicode character class
                    auto lp = regexp.find('{', i);
                    auto rp = regexp.find('}', i);

                    if (lp == std::string::npos || rp == std::string::npos)
                        return false;  // invalid syntax: unicode class with no name

                    const uint8_t *id = rejit_unicode_category_id(&regexp[lp + 1], rp - lp - 1);

                    if (id)
                        i = _rewrite_step(regexp, i, rp + 1, inst::kUnicodeType, *id);
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
};

#endif
