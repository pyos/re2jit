#include <new>
#include <re2/prog.h>
#include <re2/regexp.h>

#include "it.h"
#include "threads.h"
#include "rewriter.h"


#if RE2JIT_VM
    #include "it.vm.cc"
#elif __x86_64__
    #include "it.x64.cc"
#else
    #error "unsupported target platform, try ENABLE_VM=1"
#endif


namespace re2jit
{
    it::it(const re2::StringPiece& pattern, int max_mem) : _capturing_groups(NULL)
    {
        auto pattern2 = pattern.as_string();
        auto pure_re2 = rewrite(pattern2);

        re2::RegexpStatus status;
        // Some other parsing options:
        //   re2::Regexp::DotNL -- use `(?s)` instead
        //   re2::Regexp::FoldCase -- use `(?i)` instead
        //   re2::Regexp::NeverCapture -- write `(?:...)`, don't be lazy
        _regexp = re2::Regexp::Parse(pattern2, re2::Regexp::LikePerl, &status);

        if (_regexp == NULL) {
            _error = status.Text();
            return;
        }

        _bytecode = _regexp->CompileToProg(max_mem / 2);

        if (_bytecode == NULL) {
            _error = "out of memory: could not compile regexp";
            return;
        }

        _native = new (std::nothrow) native{_bytecode};

        if (_native == NULL || _native->state == NULL) {
            _error = "JIT compilation error";
            return;
        }

        if (pure_re2) {
            re2::Regexp *r = re2::Regexp::Parse(pattern, re2::Regexp::LikePerl, &status);

            if (r != NULL) {
                // don't care if NULL, simply won't use DFA.
                _forward = r->CompileToProg(max_mem / 4);
                _reverse = r->CompileToReverseProg(max_mem / 4);
                r->Decref();
            }
        }
    }


    it::~it()
    {
        delete _native;
        delete _bytecode;
        delete _forward;
        delete _reverse;
        delete _capturing_groups.load();
        if (_regexp)
            _regexp->Decref();
    }


    bool it::match(re2::StringPiece text, RE2::Anchor anchor,
                   re2::StringPiece* groups, int ngroups) const
    {
        if (!ok())
            return 0;

        unsigned int flags = 0;

        if (anchor == RE2::ANCHOR_BOTH || _bytecode->anchor_end())
            flags |= RE2JIT_ANCHOR_END;

        if (anchor != RE2::UNANCHORED || _bytecode->anchor_start())
            flags |= RE2JIT_ANCHOR_START;

        else if (_forward && _reverse) {
            re2::StringPiece found;
            bool failed  = false;
            bool matched = _forward->SearchDFA(text, text, re2::Prog::kUnanchored,
                                               re2::Prog::kFirstMatch, &found, &failed, NULL);

            if (!failed) {
                if (!matched) return 0;
                if (!ngroups) return 1;

                matched = !_reverse->SearchDFA(found, text, re2::Prog::kAnchored,
                                               re2::Prog::kLongestMatch, &found, &failed, NULL);

                if (!failed && matched) {
                    text  = groups[0] = found;
                    flags = RE2JIT_ANCHOR_START | RE2JIT_ANCHOR_END;

                    if (ngroups < 2) return 1;
                }
            }
        }

        struct rejit_threadset_t nfa;
        nfa.input   = text.data();
        nfa.length  = text.size();
        nfa.groups  = 2 * ngroups + 2;
        nfa.data    = _native;
        nfa.space   = _native->space;
        nfa.entry   = _native->entry;
        nfa.initial = _native->state;
        nfa.flags   = flags;

        const int *gs = rejit_thread_dispatch(&nfa);

        if (gs)
            for (int i = 0; i < ngroups; i++, gs += 2) {
                if (gs[1] < 0)
                    groups[i].set((const char *) NULL, 0);
                else
                    groups[i].set(text.data() + gs[0], gs[1] - gs[0]);
            }

        rejit_thread_free(&nfa);
        return gs != NULL;
    }


    const std::map<int, std::string> &it::named_groups() const
    {
        auto p = _capturing_groups.load();

        if (p == NULL) {
            auto q = _regexp->CaptureNames();

            if (q == NULL)
                q = new std::map<int, std::string>;

            if (_capturing_groups.compare_exchange_strong(p, q))
                return *q;

            delete q;
        }

        return *p;
    }


    std::string it::lastgroup(const re2::StringPiece *groups, int ngroups) const
    {
        if (ngroups < 2 || groups->data() == NULL)
            return "";

        int last = 0;
        auto &map = named_groups();
        auto  end = groups++->data();

        for (int i = 1; i < ngroups; i++, groups++)
            if (groups->data() >= end) {
                last = i;
                end  = groups->data() + groups->size();
            }

        auto it = map.find(last);
        return it == map.end() ? "" : it->second;
    }
}
