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

        bool match(const re2::StringPiece& text, RE2::Anchor anchor = RE2::ANCHOR_START,
                         re2::StringPiece *match = NULL, int nmatch = 0) const;

        protected:
            native    *_native;
            re2::Prog *_bytecode;
    };
};


#endif
