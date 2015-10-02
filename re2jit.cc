#include "re2jit.h"


RE2jit::RE2jit(const re2::StringPiece& pattern) : RE2jit(pattern, RE2::Quiet)
{
}


RE2jit::RE2jit(const re2::StringPiece& pattern, const RE2::Options& options) : RE2(pattern, options)
{
}


RE2jit::~RE2jit()
{
}


bool
RE2jit::Match(const re2::StringPiece& text, int startpos, int endpos, RE2::Anchor anchor,
                    re2::StringPiece *match, int nmatch) const
{
    return RE2::Match(text, startpos, endpos, anchor, match, nmatch);
}


bool
RE2jit::Match(const re2::StringPiece& text, RE2::Anchor anchor,
                    re2::StringPiece *match, int nmatch) const
{
    return RE2jit::Match(text, 0, text.size(), anchor, match, nmatch);
}
