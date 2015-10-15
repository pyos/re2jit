#include <re2/prog.h>
#include <re2/regexp.h>

#include "it.h"
#include "debug.h"
#include "threads.h"


#if RE2JIT_VM
    #include "re2jit/it.vm.cc"
#elif __x86_64__
    #include "re2jit/it.x64.cc"
#else
    static inline void *        _compile (re2::Prog *) { return NULL; }
    static inline void          _destroy (void *) {}
    static inline rejit_entry_t _entry   (void *) { return NULL; }
    static inline bool          _run     (void *, struct rejit_threadset_t *) { return 0; }
#endif


namespace re2jit
{
    it::it(const re2::StringPiece& pattern) : it(pattern, RE2::Quiet) {}
    it::it(const re2::StringPiece& pattern, const RE2::Options& options) : RE2(pattern, options)
    {
        if (RE2::ok()
            && (_regex = RE2::Regexp())
            && (_prog  = _regex->CompileToProg(RE2::options().max_mem())))
               _platform = _compile(_prog);
    }


    it::~it()
    {
        _destroy(_platform);
        delete _prog;
    }


    status it::match(const re2::StringPiece& text, RE2::Anchor anchor,
                           re2::StringPiece* match, int nmatch) const
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

    status it::run_nfa(const re2::StringPiece& text, RE2::Anchor anchor,
                             re2::StringPiece* match, int nmatch) const
    {
        struct rejit_threadset_t nfa;
        // A-a-a-and C++ is worse than C99.
        nfa.input  = text.data();
        nfa.length = (size_t) text.size();
        nfa.groups = (size_t) (nmatch * 2 + 2);
        nfa.entry  = _entry(_platform);
        nfa.flags  = anchor == RE2::ANCHOR_START  ? RE2JIT_ANCHOR_START :
                     anchor == RE2::ANCHOR_BOTH   ? RE2JIT_ANCHOR_START | RE2JIT_ANCHOR_END :
                     0;

        rejit_thread_init(&nfa);

        if (!_run(_platform, &nfa)) {
            rejit_thread_free(&nfa);
            return FAILED;
        }

        if (nfa.flags & RE2JIT_THREAD_FAILED) {
            // already free'd.
            return FAILED;
        }

        int *gs = NULL, r = rejit_thread_result(&nfa, &gs);

        for (int i = 0; i < nmatch; i++) {
            if (gs == NULL || gs[2 * i + 2] == -1)
                match[i].set((const char *) NULL, 0);
            else
                match[i].set(text.data() + gs[2 * i + 2], gs[2 * i + 3] - gs[2 * i + 2]);
        }

        rejit_thread_free(&nfa);

        debug::write("re2jit::it: finished with result %d\n", r);
        return r ? ACCEPT : REJECT;
    }
};
