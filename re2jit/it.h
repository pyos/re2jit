#ifndef RE2JIT_IT_H
#define RE2JIT_IT_H

#include <re2/re2.h>


namespace re2jit
{
    struct native;

    struct it
    {
        it(const re2::StringPiece&, int max_mem = 8 << 21);
       ~it();

        it(const it&)  = delete;
        it(const it&&) = delete;
        it& operator=(const it&) = delete;

        /* Find out whether the constructor finished successfully.
         * If not, `error()` should return a short description. */
        bool ok() const { return _error.size() == 0; }

        const std::string& error() const { return _error; }

        /* Attempt to match a string.
         *
         * @param anchor: whether the match should be aligned to string boundaries.
         *    re2::UNANCHORED -- no; the match may appear anywhere within the string.
         *    re2::ANCHOR_START -- the match should be located at the start of the string.
         *    re2::ANCHOR_BOTH -- the match should span the whole string.
         *
         * @param groups: a preallocated array of `re2::StringPiece`-s to store
         *               the contents of capturing groups in. The first "group"
         *               is the whole regexp. Unmatched groups are set to NULL.
         *
         * @param ngroups: the length of `groups`.
         *
         * @return: whether there was a match. If there wasn't, the array is not modified.
         *
         * This method is equivalent to `RE2::Match` with bounds set to whole string.
         *
         */
        bool match(const re2::StringPiece& text, RE2::Anchor anchor = RE2::ANCHOR_START,
                         re2::StringPiece *groups = NULL, int ngroups = 0) const;

        /* Fetch the name of the last outermost group that matched as a null-terminated string.
         *
         * @param match: an array of capturing subgroups populated by `it::match`.
         *
         * @param nmatch: length of `match`.
         *
         * @return: the name of the group, "" if no group matched or the outermost
         *          matching group has no name (i.e. is not a `(?P<name>...)` group).
         *
         */
        std::string lastgroup(const re2::StringPiece *match, int nmatch) const;

        protected:
            native        *_native;
            re2::Prog     *_bytecode;  // rewritten with new opcodes
            re2::Prog     *_x_forward; // untouched
            re2::Prog     *_x_reverse; // untouched with all concats reversed
            re2::Regexp   *_regexp;
            std::string    _pattern;
            std::string    _pattern2;
            std::string    _error;
            bool           _pure_re2;
            mutable const std::map<int, std::string> *_capturing_groups = NULL;
    };
};


#endif
