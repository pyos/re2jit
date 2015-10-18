#ifndef RE2JIT_IT_H
#define RE2JIT_IT_H

#include <re2/re2.h>
#include <re2jit/debug.h>


namespace re2jit
{
    struct native;

    struct it : public RE2
    {
        it(const re2::StringPiece&);
        it(const re2::StringPiece&, const RE2::Options&);
       ~it();

        /* Attempt to match a string.
         *
         * @param anchor: whether the match should be aligned to string boundaries.
         *    re2::UNANCHORED -- no; the match may appear anywhere within the string.
         *    re2::ANCHOR_START -- the match should be located at the start of the string.
         *    re2::ANCHOR_BOTH -- the match should span the whole string.
         *
         * @param match: a preallocated array of `re2::StringPiece`-s to store
         *               the contents of capturing groups in. The first "group"
         *               is the whole regexp. Unmatched groups are set to NULL.
         *
         * @param nmatch: the length of `match`.
         *
         * @return: whether there was a match. If there wasn't, the array is not modified.
         *
         * This method is equivalent to `RE2::Match` with bounds set to whole string.
         *
         */
        bool match(const re2::StringPiece& text, RE2::Anchor anchor = RE2::ANCHOR_START,
                         re2::StringPiece *match = NULL, int nmatch = 0) const;

        /* Fetch the name of the last outermost group that matched as a null-terminated string.
         *
         * @param match: an array of capturing subgroups populated by `it::match`.
         *
         * @param nmatch: length of `match`.
         *
         * @return: the name of the group, NULL if no group matched or the outermost
         *          matching group has no name (i.e. is not a `(?P<name>...)` group).
         *
         */
        const char *lastgroup(const re2::StringPiece *match, int nmatch) const;

        protected:
            native    *_native;
            re2::Prog *_bytecode;
    };
};


#endif
