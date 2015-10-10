#include <re2/prog.h>
#include <re2/regexp.h>
#include <re2jit/re2jit.h>


using namespace re2jit;


RE2jit::RE2jit(const re2::StringPiece& pattern) : RE2jit(pattern, RE2::Quiet) {}
RE2jit::RE2jit(const re2::StringPiece& pattern, const RE2::Options& options) : RE2(pattern, options)
{
    re2::Regexp *r;
    re2::Prog   *p;

    if (RE2::ok()
        && (r = RE2::Regexp())
        && (p = r->CompileToProg(RE2::options().max_mem()))
    )   // ownership of `prog` transferred to `_jitprog`
        _jitprog = new JITProg(p);
    else
        _jitprog = new JITProg(NULL);
}


RE2jit::~RE2jit()
{
    delete _jitprog;
}


JITProg::Status
RE2jit::ForceJITMatch(const re2::StringPiece& text, RE2::Anchor anchor,
                            re2::StringPiece *match, int nmatch) const
{
    return (*_jitprog)(text, anchor, match, nmatch);
}


bool
RE2jit::Match(const re2::StringPiece& text, RE2::Anchor anchor,
                    re2::StringPiece *match, int nmatch) const
{
    JITProg::Status r = (*_jitprog)(text, anchor, match, nmatch);

    return r == JITProg::NOT_JITTED ? RE2::Match(text, 0, text.size(), anchor, match, nmatch)
         : r == JITProg::MATCHED;
}


// TODO FullMatch, PartialMatch, and other re2 static convenience methods.
