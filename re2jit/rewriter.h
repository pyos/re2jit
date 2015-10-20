#ifndef RE2JIT_REWRITER_H
#define RE2JIT_REWRITER_H

#include <vector>

#include <re2/re2.h>
#include <re2jit/unicode.h>


namespace re2jit
{
    #if RE2JIT_NO_EXTCODES
        static inline const re2::StringPiece& rewrite(const re2::StringPiece &r) { return r; }
    #else
        /* Rewrite a regular expression, replacing some syntax not supported by re2
         * with placeholders which our NFA implementations can then handle their own way.
         *
         * The returned value is a temporarily allocated string. It's placed in thread-local
         * storage and deallocated on the next call to `rewrite`. As long as it is only
         * passed to re2's constructor and then discarded, everything will be fine.
         *
         */
        const re2::StringPiece rewrite(const re2::StringPiece&);


        #define _opcode(n, v) static constexpr const rejit_bmp_char_t n = SURROGATE_L + v;
        _opcode(MATCH_UNICODE_CLASS_START, 0);
        _opcode(MATCH_UNICODE_LETTER,      0);
        _opcode(MATCH_UNICODE_NUMBER,      1);
        _opcode(MATCH_UNICODE_CLASS_END,   1);
        #undef _opcode


        struct fake_inst
        {
            // One of the above constants. Note that fake instructions carry no arguments;
            // these should be encoded in the opcode itself.
            rejit_bmp_char_t op;
            // ID of the next instruction in `re2::Prog` to evaluate.
            ssize_t out;

            operator bool () const { return op != 0; }
            bool operator == (rejit_bmp_char_t c) const { return op == c; }
            bool operator != (rejit_bmp_char_t c) const { return op != c; }
            bool operator >= (rejit_bmp_char_t c) const { return op >= c; }
            bool operator <= (rejit_bmp_char_t c) const { return op <= c; }
        };


        /* Check whether the i-th opcode of a re2 program is actually a fake instruction
         * inserted by `rewrite`, meaning it is a start of a sequence of instruction
         * that match a Unicode private use character. If yes, returns the list of
         * instructions to apply; if at least one matches, then this opcode matched.
         * If not, the list is empty.
         */
        std::vector<fake_inst> is_extcode(re2::Prog *p, ssize_t i);
    #endif
};

#endif
