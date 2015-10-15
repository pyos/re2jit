#ifndef RE2JIT_IT_H
#define RE2JIT_IT_H

#include <re2/re2.h>
#include <re2jit/debug.h>


namespace re2jit
{
    enum status
    {
        FAILED = -1,
        REJECT = 0,
        ACCEPT = 1,
    };


    struct it : public RE2
    {
        it(const re2::StringPiece&);
        it(const re2::StringPiece&, const RE2::Options&);
       ~it();

        status match(const re2::StringPiece& text, RE2::Anchor anchor = RE2::ANCHOR_START,
                           re2::StringPiece *match = NULL, int nmatch = 0) const;

        status run_nfa(const re2::StringPiece& text, RE2::Anchor anchor = RE2::ANCHOR_START,
                             re2::StringPiece *match = NULL, int nmatch = 0) const;

        protected:
            re2::Regexp *_regex;
            re2::Prog   *_prog;
            void *_platform;
            void _compile();
            void _destroy();
    };
};


#endif
