#ifndef RE2JIT_IT_H
#define RE2JIT_IT_H

#include <atomic>
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
        bool match(re2::StringPiece text, RE2::Anchor anchor = RE2::ANCHOR_START,
                   re2::StringPiece *groups = NULL, int ngroups = 0) const;

        /* Return a mapping of group indices to names.
         *
         * (Named groups are declared with `(?P<name>...)` syntax.)
         *
         */
        const std::map<int, std::string> &named_groups() const;

        /* Fetch the name of the last outermost group that matched as a null-terminated string.
         *
         * @param groups: an array of capturing subgroups populated by `it::match`.
         *
         * @param ngroups: length of `groups`.
         *
         * @return: the name of the group, or "" if no group matched or the outermost
         *          matching group has no name.
         *
         */
        std::string lastgroup(const re2::StringPiece *groups, int ngroups) const;

        protected:
            native      *_native   = NULL;
            re2::Prog   *_bytecode = NULL;  // rewritten with new opcodes
            re2::Prog   *_forward  = NULL;  // untouched
            re2::Prog   *_reverse  = NULL;  // untouched with all concats reversed
            re2::Regexp *_regexp   = NULL;
            std::string  _error;
            mutable std::atomic<const std::map<int, std::string> *> _capturing_groups;
    };
}


#endif
