#ifndef RE2JIT_UNICODE_H
#define RE2JIT_UNICODE_H

#include <stdint.h>


extern "C" {
    typedef uint16_t rejit_bmp_char_t;
    typedef uint32_t rejit_uni_char_t;


    // There's a range of unused characters at U+D800 through U+DFFF.
    // These are UTF-16 surrogate characters, intended to be used to address
    // code points higher than U+FFFF through pairs of 16-bit numbers. Technically,
    // in UTF-8 a sequence of bytes that decodes to a surrogate is invalid; re2 doesn't
    // care, though, so whatever. We'll use 64 of them as makeshift opcodes.
    static constexpr const rejit_bmp_char_t SURROGATE_L = 0xDCC0u;
    static constexpr const rejit_bmp_char_t SURROGATE_H = 0xDCFFu;
    // In UTF-8, all characters that fall in this range are encoded as 0xED 0xB3 0xXX
    // where 0xXX & 0xC0 == 0x80.


    /* Attempt to read a single UTF-8 character from a buffer. On success,
     * write that character to an out-param as a 32-bit number and return
     * its length in bytes. On failure, return -1. */
    static inline int rejit_read_utf8(const uint8_t *buf, size_t size, rejit_uni_char_t *out)
    {
        #define _UTF8_CONT(i, off) ((buf[i] & 0x3F) << (6 * off))
        #define _UTF8_MCHR(s, msk, rest)                            \
            if (size >= s && (*buf & msk) == ((msk << 1) & 0xFF)) { \
                *out = ((buf[0] & ~msk) << (6 * (s - 1))) | rest;   \
                return s;                                           \
            }

        _UTF8_MCHR(1, 0x80, 0);
        _UTF8_MCHR(2, 0xE0, _UTF8_CONT(1, 0));
        _UTF8_MCHR(3, 0xF0, _UTF8_CONT(1, 1) | _UTF8_CONT(2, 0));
        _UTF8_MCHR(4, 0xF8, _UTF8_CONT(1, 2) | _UTF8_CONT(2, 1) | _UTF8_CONT(3, 0));
        _UTF8_MCHR(5, 0xFB, _UTF8_CONT(1, 3) | _UTF8_CONT(2, 2) | _UTF8_CONT(3, 1) | _UTF8_CONT(4, 0));
        _UTF8_MCHR(6, 0xFE, _UTF8_CONT(1, 4) | _UTF8_CONT(2, 3) | _UTF8_CONT(3, 2) | _UTF8_CONT(4, 1) | _UTF8_CONT(5, 0));
        return -1;

        #undef _UTF8_MCHR
        #undef _UTF8_CONT
    }


    /* Write a BMP Unicode character in UTF-8 to a buffer. The buffer must
     * be sufficiently large (>= 3 bytes). */
    static inline int rejit_write_utf8(uint8_t *dst, rejit_bmp_char_t c)
    {
        if (c < 0x80u) {
            *dst++ = c;
            return 1;
        }

        if (c < 0x800u) {
            *dst++ = 0xC0u | (c >> 6);
            *dst++ = 0x80u | (c & 0x3Fu);
            return 2;
        }

        *dst++ = 0xE0u | (c >> 12);
        *dst++ = 0x80u | ((c >> 6) & 0x3Fu);
        *dst++ = 0x80u | (c & 0x3Fu);
        return 3;
    }
};

#endif
