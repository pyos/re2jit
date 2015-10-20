#include <cstring>

#include <re2/prog.h>

#include "rewriter.h"


#if !RE2JIT_NO_EXTCODES
    static thread_local struct
    {
        char  *buf;
        size_t len;
    } _storage = { NULL, 0 };


    static char *allocate(const re2::StringPiece &regexp)
    {
        delete[] _storage.buf;
        _storage.len = regexp.size();
        _storage.buf = new char[_storage.len];
        memcpy(_storage.buf, regexp.data(), _storage.len);
        return _storage.buf;
    }


    namespace re2jit
    {
        const re2::StringPiece rewrite(const re2::StringPiece& regexp)
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

                    #define REWRITE(ch, new_src)                                     \
                        do {                                                         \
                            if (dst == NULL)                                         \
                                dst = allocate(regexp) + (src - regexp.data());      \
                                                                                     \
                            dst -= 1;                                                \
                            int len = rejit_write_utf8((uint8_t *) dst, ch);         \
                            dst += len - 1;                                          \
                            _storage.len -= new_src - src - len + 2;                 \
                            memcpy(dst + 1, (src = new_src) + 1, end - new_src - 1); \
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
                                        REWRITE(MATCH_UNICODE_LETTER, rparen);
                                        goto done;

                                    case 'N':
                                        REWRITE(MATCH_UNICODE_NUMBER, rparen);
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


        std::vector<fake_inst> is_extcode(re2::Prog *p, ssize_t i)
        {
            re2::Prog::Inst *in = p->inst(i);

            // (SURROGATE_L .. SURROGATE_H) in UTF-8 = 0xED 0xB3 0xXX where 0xXX & 0xC0 == 0x80.
            if (in->opcode() != re2::kInstByteRange || in->hi() != 0xED || in->lo() != 0xED)
                return std::vector<fake_inst>{};

            in = p->inst(in->out());

            if (in->opcode() != re2::kInstByteRange || in->hi() != 0xB3 || in->lo() != 0xB3)
                return std::vector<fake_inst>{};

            // The third one is tricky. It can be a choice between any `kInstByteRange`s
            // where both limits are UTF-8 continuation bytes.
            ssize_t stack[128];
            ssize_t stptr = 0;
            stack[stptr++] = in->out();
            std::vector<fake_inst> rs;

            while (stptr--) {
                in = p->inst(stack[stptr]);

                switch (in->opcode()) {
                    case re2::kInstAlt:
                    case re2::kInstAltMatch:
                        stack[stptr++] = in->out1();
                        stack[stptr++] = in->out();
                        break;

                    case re2::kInstByteRange: {
                        rejit_bmp_char_t a = in->lo();
                        rejit_bmp_char_t b = in->hi();

                        if ((a & 0xC0) == 0x80 && (b & 0xC0) == 0x80) {
                            for (; a <= b; a++) {
                                rs.push_back(fake_inst { (rejit_bmp_char_t) (SURROGATE_L + (a & 0x3F)), in->out() });
                            }

                            break;
                        }
                    }

                    default:
                        // regexp can match junk in the middle of characters. not good.
                        return std::vector<fake_inst>{};
                }
            }

            return rs;
        }
    };
#endif
