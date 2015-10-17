#include <re2/prog.h>
#include <re2/regexp.h>


#if RE2JIT_VM
    #pragma message "target = NFA interpreter"
    #include "it.vm.cc"
#elif __x86_64__
    #pragma message "target = x86_64 System V ABI"
    #include "it.x64.cc"
#else
    #error "unsupported architecture, compile with FORCE_VM"
#endif


#include "it.h"
#include "debug.h"
#include "threads.h"


namespace re2jit
{
    it::it(const re2::StringPiece& pattern) : it(pattern, RE2::Quiet) {}
    it::it(const re2::StringPiece& pattern, const RE2::Options& options) : RE2(pattern, options)
    {
        if (RE2::ok()) {
            // May fail, but highly unlikely -- `RE2::Init` already compiled it.
            _bytecode = RE2::Regexp()->CompileToProg(RE2::options().max_mem());
            // Not like anything can be done if it fails anyway -- `error_` is private.
            _native = new native{_bytecode};
        }
    }


    it::~it()
    {
        delete _native;
        delete _bytecode;
    }


    bool it::match(const re2::StringPiece& text, RE2::Anchor anchor,
                         re2::StringPiece* match, int nmatch) const
    {
        if (_bytecode->anchor_start() && _bytecode->anchor_end())
            anchor = ANCHOR_BOTH;
        else if (_bytecode->anchor_start() && anchor != ANCHOR_BOTH)
            anchor = ANCHOR_START;

        struct rejit_threadset_t nfa;
        // A-a-a-and C++ is worse than C99.
        nfa.input  = text.data();
        nfa.length = text.size();
        nfa.groups = nmatch ? 2 * nmatch : 2;
        nfa.states = _bytecode->size();
        nfa.entry  = _native->entry();
        nfa.flags  = anchor == RE2::ANCHOR_START  ? RE2JIT_ANCHOR_START :
                     anchor == RE2::ANCHOR_BOTH   ? RE2JIT_ANCHOR_START | RE2JIT_ANCHOR_END :
                     0;

        if (!rejit_thread_init(&nfa)) {
            debug::write("re2jit::it: failed to initialize NFA\n");
            goto fallback;
        }

        if (!_native->run(&nfa)) {
            rejit_thread_free(&nfa);
            goto fallback;
        }

        {
            int *gs = NULL, r = rejit_thread_result(&nfa, &gs);

            if (gs != NULL) for (int i = 0; i < nmatch; i++, gs += 2) {
                if (gs[0] == -1 || gs[1] == -1)
                    match[i].set((const char *) NULL, 0);
                else
                    match[i].set(text.data() + gs[0], gs[1] - gs[0]);
            }

            debug::write("re2jit::it: finished with result %d\n", r);
            rejit_thread_free(&nfa);
            return r;
        }

    fallback:
        debug::write("re2jit::it: falling back to re2\n");
        return RE2::Match(text, 0, text.size(), anchor, match, nmatch);
    }
};
