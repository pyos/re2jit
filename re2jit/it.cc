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


namespace re2jit
{
    it::it(const re2::StringPiece& pattern, int max_mem) : _native(NULL), _bytecode(NULL), _x_forward(NULL), _x_reverse(NULL)
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

        _bytecode = _regexp->CompileToProg(max_mem / 2);

        if (_bytecode == NULL) {
            _error = "out of memory: could not compile regexp";
            return;
        }

        _native = new native{_bytecode};

        if (_native->entry == NULL || _native->state == NULL) {
            _error = "JIT compilation error";
            return;
        }

        if (_pure_re2) {
            re2::Regexp *r = re2::Regexp::Parse(pattern, re2::Regexp::LikePerl, &status);

            if (r != NULL) {
                // don't care if NULL, simply won't use DFA.
                _x_forward = r->CompileToProg(max_mem / 4);
                _x_reverse = r->CompileToReverseProg(max_mem / 4);
                             r->Decref();
            }
        }
    }


    it::~it()
    {
        delete _native;
        delete _bytecode;
        delete _x_forward;
        delete _x_reverse;
        delete _capturing_groups;
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

        if (!(flags & RE2JIT_ANCHOR_START) && _x_forward && _x_reverse) {
            re2::StringPiece found;
            bool failed  = false;
            bool matched = _x_forward->SearchDFA(text, text, re2::Prog::kUnanchored,
                                                 re2::Prog::kFirstMatch, &found, &failed, NULL);

            if (!failed) {
                if (!matched)
                    return 0;

                matched = _x_reverse->SearchDFA(found, text, re2::Prog::kAnchored,
                                                re2::Prog::kLongestMatch, &found, &failed, NULL);

                if (!failed && matched) {
                    if (nmatch > 1)  // no point in doing anything else otherwise.
                        return it::match(found, RE2::ANCHOR_BOTH, match, nmatch);

                    if (nmatch)
                        *match = found;
                    return 1;
                }
            }
        }

        struct rejit_threadset_t nfa;
        nfa.input   = text.data();
        nfa.length  = text.size();
        nfa.groups  = 2 * nmatch + 2;
        nfa.states  = _bytecode->size();
        nfa.initial = _native->state;
        nfa.entry   = (rejit_entry_t) _native->entry;
        nfa.flags   = flags;
        _native->init();

        int *gs, r;
        rejit_thread_init(&nfa);
        r = rejit_thread_dispatch(&nfa, &gs);

        if (r == 1)
            for (int i = 0; i < nmatch; i++, gs += 2) {
                if (gs[0] == -1 || gs[1] == -1)
                    match[i].set((const char *) NULL, 0);
                else
                    match[i].set(text.data() + gs[0], gs[1] - gs[0]);
            }

        rejit_thread_free(&nfa);
        return r;
    }


    std::string it::lastgroup(const re2::StringPiece *match, int nmatch) const
    {
        if (!ok())
            return "";

        if (_capturing_groups == NULL)
            _capturing_groups = _regexp->CaptureNames();

        if (_capturing_groups == NULL)
            return "";

        const char *end = NULL;
        int grp = 0;

        for (int i = 1; i < nmatch; i++) {  // 0 = whole match
            if (match[i].data() == NULL || match[i].data() < end)
                continue;

            end = match[i].data() + match[i].size();
            grp = i;
        }

        const auto it = _capturing_groups->find(grp);

        if (it == _capturing_groups->cend())
            return "";

        std::string n = it->second;
        return n;
    }
};
