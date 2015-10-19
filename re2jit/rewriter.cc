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


        fake_inst::fake_inst(re2::Prog *p, ssize_t i) : op(0), out(0)
        {
            re2::Prog::Inst *in = p->inst(i);

            // 3 bytes are enough as we only use the BMP Private Use Area.
            uint8_t buf[3], *bptr = buf;
            ssize_t out[3], *optr = out;

            rejit_uni_char_t chr;

            // Only interested in opcodes that match a single byte.
            while (in->opcode() == re2::kInstByteRange && in->hi() == in->lo() && bptr != buf + 3) {
                *bptr++ = in->lo();
                *optr++ = in->out();
                in = p->inst(in->out());
            }

            int len = rejit_read_utf8(buf, bptr - buf, &chr);

            if (len != -1 && PRIVATE_USE_L <= chr && chr <= PRIVATE_USE_H) {
                this->op  = (rejit_bmp_char_t) chr;
                this->out = out[len - 1];
            }
        }
    };
#endif
