#ifndef RE2JIT_IT_H
#define RE2JIT_IT_H

#include <memory>

#include <re2/re2.h>


namespace re2jit
{
    struct native;

    enum RE2JIT_ANCHOR_FLAGS
    {
        RE2JIT_ANCHOR_START = 1,
        RE2JIT_ANCHOR_END   = 2,
    };

    struct it
    {
        it(const re2::StringPiece&, int max_mem = 8 << 21);
        it(const it&)  = delete;
        it(const it&&) = delete;
        it& operator=(const it&) = delete;

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

        protected:
            template <typename T>
            struct _simple_deleter { void operator()(T *p); };
            struct _regexp_deleter { void operator()(re2::Regexp *); };

            std::unique_ptr<native,      _simple_deleter<native>>    _native;
            std::unique_ptr<re2::Prog,   _simple_deleter<re2::Prog>> _forward;
            std::unique_ptr<re2::Prog,   _simple_deleter<re2::Prog>> _reverse;
            std::unique_ptr<re2::Regexp, _regexp_deleter>            _regexp;
    };
};


#endif
