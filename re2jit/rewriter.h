#ifndef RE2JIT_REWRITER_H
#define RE2JIT_REWRITER_H

#include <vector>
#include <cstring>

#include <re2/prog.h>
#include <re2/stringpiece.h>

#include <re2jit/unicode.h>


namespace re2jit
{
    struct inst {
        #define _opcode(n, v) static constexpr const rejit_bmp_char_t n = SURROGATE_L + v;
        _opcode(kUnicodeLetter, 0);
        _opcode(kUnicodeNumber, 1);
        #undef _opcode

        inst(ssize_t opcode, ssize_t out) : opcode_(opcode), out_(out) {}

        ssize_t opcode() const { return opcode_; }
        ssize_t out() const { return out_; }

        protected:
            ssize_t opcode_;
            ssize_t out_;
    };


    #if RE2JIT_NO_EXTCODES
        static inline const re2::StringPiece& rewrite(const re2::StringPiece &r) { return r; }

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
        #define RE2JIT_WITH_INST(var, prog, i, ext, normal) do { \
            auto var = (prog)->inst(i);                          \
            normal;                                              \
        } while (0)
    #else
        static thread_local struct
        {
            char  *buf;
            size_t len;
        } _storage = { NULL, 0 };


        static inline char *allocate_rewrite(const re2::StringPiece &regexp)
        {
            delete[] _storage.buf;
            _storage.len = regexp.size();
            _storage.buf = new char[_storage.len];
            memcpy(_storage.buf, regexp.data(), _storage.len);
            return _storage.buf;
        }


        /* Rewrite a regular expression, replacing some syntax not supported by re2
         * with placeholders which our NFA implementations can then handle their own way.
         *
         * The returned value is a temporarily allocated string. It's placed in thread-local
         * storage and deallocated on the next call to `rewrite`. As long as it is only
         * passed to re2's constructor and then discarded, everything will be fine.
         *
         */
        static inline const re2::StringPiece rewrite(const re2::StringPiece& regexp)
        {
            const char *src = regexp.data();
            const char *end = regexp.size() + src;

            char *dst = NULL;

            for (; src != end; src++) {
                if (*src == '\\') {
                    if (++src == end)
                        // unexpected EOF after escape character
                        return regexp;

                    if (dst) dst++;

                    #define REWRITE(ch, new_src)                                        \
                        do {                                                            \
                            if (dst == NULL)                                            \
                                dst = allocate_rewrite(regexp) + (src - regexp.data()); \
                                                                                        \
                            dst -= 1;                                                   \
                            int len = rejit_write_utf8((uint8_t *) dst, ch);            \
                            dst += len - 1;                                             \
                            _storage.len -= new_src - src - len + 2;                    \
                            memcpy(dst + 1, (src = new_src) + 1, end - new_src - 1);    \
                        } while (0)

                    switch (*src)
                    {
                        case 'p':
                            // '\p{kind}' -- match a whole Unicode character class
                            const char *lparen = src;

                            if (++lparen == end || *lparen != '{')
                                goto done;

                            const char *rparen = lparen;

                            while (*rparen != '}')
                                if (++rparen == end)
                                    goto done;

                            switch (rparen - (++lparen))  // i.e. length of class identifier
                            {
                                case 1: switch (*lparen)
                                {
                                    case 'L':
                                        REWRITE(inst::kUnicodeLetter, rparen);
                                        goto done;

                                    case 'N':
                                        REWRITE(inst::kUnicodeNumber, rparen);
                                        goto done;

                                    // TODO other classes?
                                }
                            }

                            break;
                    }
                }

                done: if (dst) dst++;
            }

            return dst ? re2::StringPiece{ _storage.buf, (int) _storage.len } : regexp;
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


        #define RE2JIT_WITH_INST(var, prog, i, ext, normal) do {               \
            auto var = (prog)->inst(i);                                        \
            auto __ins = re2jit::is_extcode(prog, var);                        \
            if (__ins.size()) {                                                \
                for (auto var = __ins.begin(); var != __ins.end(); var++) ext; \
            } else {                                                           \
                normal;                                                        \
            }                                                                  \
        } while (0)
    #endif
};

#endif
