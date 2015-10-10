#ifndef RE2JIT_H
#define RE2JIT_H

#include <re2/re2.h>

#include "recompiler.h"


class RE2jit : public RE2 {

protected:
    re2jit::Recompiler _cre;

public:
    RE2jit(const re2::StringPiece&);
    RE2jit(const re2::StringPiece&, const RE2::Options&);
   ~RE2jit();

    bool Match(const re2::StringPiece& text, RE2::Anchor anchor = RE2::ANCHOR_START,
                     re2::StringPiece *match = NULL, int nmatch = 0) const;
};


#endif
