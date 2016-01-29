#ifndef RE2JIT_REWRITER_H
#define RE2JIT_REWRITER_H

#include <deque>
#include <string>
#include <re2/prog.h>

#include "unicode.h"


namespace re2jit
{
    enum ecode_t : uint8_t
    {
        kUnicodeTypeGeneral,
        kUnicodeTypeSpecific,
        kUnicodeTypeGeneralNegated,
        kUnicodeTypeSpecificNegated,
        kBackreference,
    };


    struct inst {
        ecode_t opcode;
        uint8_t arg;
        ssize_t out;
    };


    static inline std::string::size_type _rewrite_inst(std::string& s,
                  std::string::size_type pos,
                  std::string::size_type end, uint8_t op, uint8_t arg)
    {
        // Private Use Area on plane 15: 0xF0000 .. 0xFFFFD. We'll use 0xF0000 .. 0xF0FFF:
        //
        // 1111 0000 xxxx xxxx xxxx  ----------> 11110 011  10 110000  10 xxxxxx  10 xxxxxx
        // F    0    op   arg           UTF-8    F   3      B    0        op  arg
        //
        uint8_t buf[4] = { 0xF3, 0xB0, uint8_t(0x80 | (arg >> 6) | (op << 2)),
                                       uint8_t(0x80 | (arg & 0x3F)) };
        s.replace(pos, end - pos, (char *) buf, 4);
        return pos + 3;
    }


    /* Replace some escaped sequences with private use Unicode characters.
     * NFA may detect sequences of opcodes that match these private use code points
     * and do something implementation-defined instead of actually matching a character.
     * Returns `true` if the original regexp was compatible with re2, `false` otherwise. */
    static inline bool rewrite(std::string& regexp)
    {
        auto i = std::string::size_type();
        bool is_re2 = true;
        bool in_negated_chclass = false;

        for (; i + 1 < regexp.size(); i++)
            // backslash cannot be the last character
            if (regexp[i] == '\\') {
                if (in_negated_chclass)
                    // [^\U+F0xxx] would be interpreted as "anything except that character".
                    // this would completely screw us.
                    goto unrecognized;
                else if (regexp[i + 1] == 'p' || regexp[i + 1] == 'P') {
                    // '\p{kind}' or '\pK' -- match a whole Unicode character class
                    // '\P{kind}' or '\PK' -- match everything except a class
                    auto neg = regexp[i + 1] == 'P';
                    auto lp = i + 2;
                    auto rp = i + 3;

                    if (lp == regexp.size())
                        goto unrecognized;  // invalid syntax: unicode class with no name

                    if (regexp[lp] == '{' && (rp = regexp.find('}', ++lp)) == std::string::npos)
                        goto unrecognized;  // invalid syntax: mismatched `{`

                    const uint8_t *id = rejit_unicode_category_id(&regexp[lp], rp - lp);

                    if (id)
                        i = _rewrite_inst(regexp, i, rp + (lp != i + 2),
                            rp - lp == 1 && neg ? kUnicodeTypeGeneralNegated
                          : rp - lp == 1        ? kUnicodeTypeGeneral
                          : neg                 ? kUnicodeTypeSpecificNegated
                          :                       kUnicodeTypeSpecific, *id);
                }
                else if (isdigit(regexp[i + 1])) {
                    // \1234 -- backreference to group 1234.
                    char *e = NULL;
                    long  r = strtol(&regexp[i + 1], &e, 10);

                    i = _rewrite_inst(regexp, i, e - &regexp[0], kBackreference, r);
                    // re2 does not support backreferences.
                    is_re2 = false;
                } else unrecognized: i++;
            }
            else if (regexp[i] == '[' && regexp[i + 1] == '^')
                in_negated_chclass = true;
            else if (in_negated_chclass && regexp[i] == ']')
                in_negated_chclass = false;

        return is_re2;
    }


    static inline bool _inst_matches_byte(re2::Prog::Inst *i, int b)
    {
        return i->opcode() == re2::kInstByteRange && i->hi() == b && i->lo() == b;
    }


    /* Check whether an instruction is actually the first in a rewritten opcode sequence. *
     * A rewritten opcode sequence is a series of opcodes that matches a character
     * in the U+F0000 .. U+F0FFF range. */
    static inline bool is_extcode(re2::Prog *p, re2::Prog::Inst *i)
    {
        return _inst_matches_byte(i, 0xF3) && _inst_matches_byte(p->inst(i->out()), 0xB0);
    }


    /* If an instruction is a start of a rewritten opcode sequence, return a container
     * with actual insts to evaluate. The returned insts are joined by implicit kInstAlts.
     * If no insts are returned, `i` should have original re2 behavior. */
    static inline std::deque<inst> get_extcode(re2::Prog *p, re2::Prog::Inst *i)
    {
        if (!is_extcode(p, i))
            return std::deque<inst>{};

        std::deque<inst> fst { inst { ecode_t(-1), 0, p->inst(i->out())->out() } }, snd;
        // re2 may have "simplified" the automaton. instead of something like
        //          /--- F3 B0 xx yy ---\          extcode 1 OR
        //   ... ---+--- F3 B0 aa bb --- --- ...   extcode 2 OR
        //          \--- F3 B0 aa bc ---/          extcode 3
        // where decoding an extcode would be easy (it's a sequence of 4 single-byte
        // ranges) we may get something like
        //                /--- xx yy ---\          (`+` is a kInstAlt)
        //   ... F3 B0 ---+              --- ...
        //                \--- aa rr ---/
        // where `rr` is a range that matches either `bb` or `bc`, so we need
        // to walk the graph to unwrap alternations and ranges.
        while (!fst.empty()) {
            inst q = fst.back(); fst.pop_back();

            switch ((i = p->inst(q.out))->opcode()) {
                case re2::kInstAlt:
                case re2::kInstAltMatch:
                    fst.push_back(inst { q.opcode, q.arg, i->out1() });
                    fst.push_back(inst { q.opcode, q.arg, i->out()  });
                    continue;

                case re2::kInstByteRange:
                    if ((i->lo() & 0xC0) == 0x80 && (i->hi() & 0xC0) == 0x80)  // utf-8 continuation
                        for (int a = i->lo(); a <= i->hi(); a++) {
                            if (q.opcode == ecode_t(-1))
                                // first byte -- 4-bit opcode + high 2 bits of argument
                                fst.push_back(inst { ecode_t((a >> 2) & 0xF), uint8_t(a << 6), i->out() });
                            else
                                // second byte -- low 6 bits of argument, got whole extcode now.
                                snd.push_back(inst { q.opcode, uint8_t(q.arg | (a & 0x3F)), i->out() });
                        }
                    continue;

                default:
                    return std::deque<inst>{};
            }
        }

        return snd;
    }
}

#endif
