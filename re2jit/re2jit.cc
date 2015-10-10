#include <re2/prog.h>
#include <re2/regexp.h>
#include <re2jit/re2jit.h>


using namespace re2jit;


RE2jit::RE2jit(const re2::StringPiece& pattern) : RE2jit(pattern, RE2::Quiet)
{
}


RE2jit::RE2jit(const re2::StringPiece& pattern, const RE2::Options& options) : RE2(pattern, options)
{
    if (RE2::ok()) {
        re2::Regexp *r = RE2::Regexp();

        if (r != NULL) {
            re2::Prog *prog = r->CompileToProg(RE2::options().max_mem());

            if (prog != NULL) {
                _cre = *prog;
                delete prog;
            }
        }
    }
}


RE2jit::~RE2jit()
{
}


bool
RE2jit::Match(const re2::StringPiece& text, RE2::Anchor anchor,
                    re2::StringPiece *match, int nmatch) const
{
    Recompiler::Status r = _cre.Run(text, anchor, match, nmatch);

    if (r <= re2jit::Recompiler::ERR_GENERIC) {
        return RE2::Match(text, 0, text.size(), anchor, match, nmatch);
    }

    return r == re2jit::Recompiler::MATCHED;
}


// TODO FullMatch, PartialMatch, and other re2 static convenience methods.
