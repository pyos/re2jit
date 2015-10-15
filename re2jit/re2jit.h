#ifndef RE2JIT_H
#define RE2JIT_H

#include <re2/re2.h>
#include <re2jit/debug.h>
#include <re2jit/jitprog.h>


class RE2jit : public RE2 {

protected:
    re2jit::JITProg *_jitprog;

public:
    RE2jit(const re2::StringPiece&);
    RE2jit(const re2::StringPiece&, const RE2::Options&);
   ~RE2jit();

    re2jit::JITProg::Status
    ForceJITMatch(const re2::StringPiece& text, RE2::Anchor anchor = RE2::ANCHOR_START,
                        re2::StringPiece *match = NULL, int nmatch = 0) const;

    bool Match(const re2::StringPiece& text, RE2::Anchor anchor = RE2::ANCHOR_START,
                     re2::StringPiece *match = NULL, int nmatch = 0) const;
};


#endif