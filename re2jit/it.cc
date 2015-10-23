#include <re2/prog.h>
#include <re2/regexp.h>

#include "it.h"
#include "threads.h"
#include "rewriter.h"


#if RE2JIT_VM
    #pragma message "target = NFA interpreter"
    #include "it.vm.cc"
#elif __x86_64__
    #pragma message "target = x86_64 System V ABI"
    #include "it.x64.cc"
#else
    #pragma message "target = ??? [try ENABLE_VM=1]"
#endif


#ifndef RE2JIT_DFA_CUTOFF
/* When doing unanchored searches on inputs at least this long, use re2's fast DFA
 * to locate the match first, then run our NFA anchored to the result to capture groups.
 * Set to 0 to disable. */
#define RE2JIT_DFA_CUTOFF 8196
#endif


namespace re2jit
{
    it::it(const re2::StringPiece& pattern) : _native(NULL), _original(NULL), _bytecode(NULL)
    {
        re2::RegexpStatus status;

        _pattern  = pattern.as_string();
        _pattern2 = pattern.as_string();
        _pure_re2 = rewrite(_pattern2);
        // Some other parsing options:
        //   re2::Regexp::DotNL -- use `(?s)` instead
        //   re2::Regexp::FoldCase -- use `(?i)` instead
        //   re2::Regexp::NeverCapture -- write `(?:...)`, don't be lazy
        _regexp = re2::Regexp::Parse(_pattern2, re2::Regexp::LikePerl, &status);

        if (_regexp == NULL) {
            _error = status.Text();
            return;
        }

        _bytecode = _regexp->CompileToProg(8 << 20);

        if (_bytecode == NULL) {
            _error = "out of memory";  // 8 << 20 was not enough.
            return;
        }

        _native = new native{_bytecode};

        #if !RE2JIT_VM
            if (_native->entry() == NULL) {
                _error = "JIT compilation error";
                return;
            }
        #endif

        if (_pure_re2 && RE2JIT_DFA_CUTOFF) {
            _original = new RE2(_pattern, RE2::Quiet);

            if (!_original->ok()) {
                delete _original;  // Don't care, won't use DFA.
                _original = NULL;
            }
        }
    }


    it::~it()
    {
        delete _original;
        delete _native;
        delete _bytecode;
        if (_regexp)
            _regexp->Decref();
    }


    bool it::match(const re2::StringPiece& text, RE2::Anchor anchor,
                         re2::StringPiece* match, int nmatch) const
    {
        if (!ok()) {
            return 0;
        }

        unsigned int flags = 0;

        if (_bytecode->anchor_start() || anchor != RE2::UNANCHORED)
            flags |= RE2JIT_ANCHOR_START;

        if (_bytecode->anchor_end() || anchor == RE2::ANCHOR_BOTH)
            flags |= RE2JIT_ANCHOR_END;

        if (!(flags & RE2JIT_ANCHOR_START) && _original && text.size() >= RE2JIT_DFA_CUTOFF) {
            re2::StringPiece found;

            if (!_original->Match(text, 0, text.size(), RE2::UNANCHORED, &found, 1))
                return 0;

            return it::match(found, RE2::ANCHOR_BOTH, match, nmatch);
        }

        struct rejit_threadset_t nfa;
        nfa.input  = text.data();
        nfa.length = text.size();
        nfa.groups = 2 * nmatch + 2;
        nfa.states = _bytecode->size();
        nfa.entry  = _native->entry();
        nfa.flags  = flags;

        if (!rejit_thread_init(&nfa)) {
            return 0;
        }

        _native->run(&nfa);

        int *gs, r;

        if ((r = rejit_thread_result(&nfa, &gs))) {
            for (int i = 0; i < nmatch; i++, gs += 2) {
                if (gs[0] == -1 || gs[1] == -1)
                    match[i].set((const char *) NULL, 0);
                else
                    match[i].set(text.data() + gs[0], gs[1] - gs[0]);
            }
        }

        rejit_thread_free(&nfa);
        return r;
    }


    std::string it::lastgroup(const re2::StringPiece *match, int nmatch) const
    {
        if (!ok()) {
            return "";
        }

        const std::map<int, std::string> *names = _regexp->CaptureNames();
        const char *end = NULL;

        if (names == NULL) {
            return "";
        }

        int grp = 0;

        for (int i = 1; i < nmatch; i++) {  // 0 = whole match
            if (match[i].data() == NULL || match[i].data() < end)
                continue;

            end = match[i].data() + match[i].size();
            grp = i;
        }

        const auto it = names->find(grp);

        if (it == names->cend()) {
            delete names;
            return "";
        }

        std::string n = it->second;
        delete names;
        return n;
    }
};
