#ifndef RE2JIT_UNICODE_H
#define RE2JIT_UNICODE_H

#ifdef __cplusplus
extern "C" {
#endif
    #include "unicodedata.h"

    /* Return an ID of a category given its 1- or 2-letter name, NULL if unknown. */
    static inline const uint8_t *rejit_unicode_category_id(const char *s, int sz)
    {
        const struct _rejit_uni_cat_id_t *p = _rejit_uni_cat_id(s, sz);
        return p && p->name ? &p->id : NULL;
    }

    /* Return a character's specific category. `&` with UNICODE_CATEGORY_GENERAL
     * to get the general category. */
    static inline uint8_t rejit_unicode_category(uint32_t c)
    {
        return UNICODE_2STAGE_GET(UNICODE_CATEGORY, c & 0x1FFFFF);
    }

    /* Attempt to read a single UTF-8 character from a buffer. On success,
     * return that character as lower 24 bits and no. of read bytes as upper 8 bits.
     * On failure, return 0. */
    static inline uint32_t rejit_read_utf8(const uint8_t *buf, size_t size)
    {
        uint32_t c;
        if (!  size) return 0; else c = *buf;
        if (*buf < 0x80 /* 0....... */) return 1LL << 24 | c;
        if (*buf < 0xC0 /* 10...... */) return 0;  // stray continuation byte
        if (!--size) return 0; else c = c << 6 | (buf[1] & 0x3F);
        if (*buf < 0xE0 /* 110..... */) return 2LL << 24 | (c & 0x7FF);
        if (!--size) return 0; else c = c << 6 | (buf[2] & 0x3F);
        if (*buf < 0xF0 /* 1110.... */) return 3LL << 24 | (c & 0xFFFF);
        if (!--size) return 0; else c = c << 6 | (buf[3] & 0x3F); if ((c & 0x1FFFFF) > 0x10FFFF) return 0;
        if (*buf < 0xF8 /* 11110... */) return 4LL << 24 | (c & 0x1FFFFF);
        return 0;
    }
#ifdef __cplusplus
}
#endif

#endif
