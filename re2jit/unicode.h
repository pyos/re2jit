#ifndef RE2JIT_UNICODE_H
#define RE2JIT_UNICODE_H

extern "C" {
    #include <stdint.h>
    #include "unicodedata.h"

    /* Return an ID of a category given its 1- or 2-letter name, NULL if unknown. */
    static inline const uint8_t *rejit_unicode_category_id(const char *s, int sz)
    {
        const _rejit_uni_cat_id_t *p = _rejit_uni_cat_id(s, sz);
        return p->name ? &p->id : NULL;
    }

    /* Return a character's specific category. `&` with UNICODE_CATEGORY_GENERAL
     * to get the general category. */
    static inline uint8_t rejit_unicode_category(uint32_t c)
    {
        return UNICODE_2STAGE_GET(UNICODE_CATEGORY, c);
    }

    /* Attempt to read a single UTF-8 character from a buffer. On success,
     * return that character as lower 32 bits and its length as upper 32 bits.
     * On failure, return 0. */
    static inline uint64_t rejit_read_utf8(const uint8_t *buf, size_t size)
    {
        #define _UTF8_CONT(i, off) ((buf[i] & 0x3F) << (6 * off))
        #define _UTF8_MCHR(s, msk, rest)                                      \
            if (size >= s && (*buf & msk) == ((msk << 1) & 0xFF)) {           \
                return (s << 32) | ((buf[0] & ~msk) << (6 * (s - 1))) | rest; \
            }

        _UTF8_MCHR(1LL, 0x80, 0);
        _UTF8_MCHR(2LL, 0xE0, _UTF8_CONT(1, 0));
        _UTF8_MCHR(3LL, 0xF0, _UTF8_CONT(1, 1) | _UTF8_CONT(2, 0));
        _UTF8_MCHR(4LL, 0xF8, _UTF8_CONT(1, 2) | _UTF8_CONT(2, 1) | _UTF8_CONT(3, 0));
     // Unicode ends at 0x10FFFF.
     // _UTF8_MCHR(5LL, 0xFB, _UTF8_CONT(1, 3) | _UTF8_CONT(2, 2) | _UTF8_CONT(3, 1) | _UTF8_CONT(4, 0));
     // _UTF8_MCHR(6LL, 0xFE, _UTF8_CONT(1, 4) | _UTF8_CONT(2, 3) | _UTF8_CONT(3, 2) | _UTF8_CONT(4, 1) | _UTF8_CONT(5, 0));
        return 0;

        #undef _UTF8_MCHR
        #undef _UTF8_CONT
    }
};

#endif
