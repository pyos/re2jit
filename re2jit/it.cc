#include <re2/prog.h>
#include <re2/regexp.h>

#include "it.h"
#include "debug.h"


namespace re2jit
{
    it::it(const re2::StringPiece& pattern) : it(pattern, RE2::Quiet) {}
    it::it(const re2::StringPiece& pattern, const RE2::Options& options) : RE2(pattern, options)
    {
        if (RE2::ok()
            && (_regex = RE2::Regexp())
            && (_prog  = _regex->CompileToProg(RE2::options().max_mem())))
               _compile();
    }


    it::~it()
    {
        _destroy();
        delete _prog;
    }


    status it::match(const re2::StringPiece& text, RE2::Anchor anchor,
                           re2::StringPiece *match, int nmatch) const
    {
        status r = run_nfa(text, anchor, match, nmatch);

        if (r == FAILED) {
            debug::write("re2jit::it: falling back to re2\n");

            if (RE2::Match(text, 0, text.size(), anchor, match, nmatch)) {
                return ACCEPT;
            }

            return REJECT;
        }

        return r;
    }
};


#if RE2JIT_INTERPRET
    #include "re2jit/it.vm.cc"
#elif __x86_64__
    #include "re2jit/it.x64.cc"
#else
    #include "re2jit/it.fallback.cc"
#endif
