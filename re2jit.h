#ifndef RE2JIT_H
#define RE2JIT_H

#include <re2/re2.h>


class RE2jit : public RE2 {

public:
    RE2jit(const re2::StringPiece&);
    RE2jit(const re2::StringPiece&, const RE2::Options&);
    ~RE2jit();

    /* Attempt to match a part of text between given indices.
     * Additional anchoring to the beginning/end can be provided; the ones in
     * the regex itself are always respected either way. If a preallocated array
     * of `StringPiece`s is provided, the contents of the first N non-atomic groups
     * are stored in it; the 0-th string piece is the whole matched part of the text.
     */
    bool Match(const re2::StringPiece& text, int startpos, int endpos, RE2::Anchor anchor,
                     re2::StringPiece *match, int nmatch) const;
    bool Match(const re2::StringPiece& text, RE2::Anchor anchor = RE2::ANCHOR_START,
                     re2::StringPiece *match = NULL, int nmatch = 0) const;
};


#endif
