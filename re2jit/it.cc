#include <stdexcept>

#include <re2/prog.h>
#include <re2/regexp.h>

#include "it.h"


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
    it::it(const re2::StringPiece& pattern, int max_mem)
    {
        re2::RegexpStatus status;

        _regexp.reset(re2::Regexp::Parse(pattern, re2::Regexp::LikePerl, &status));

        if (_regexp == NULL)
            throw std::runtime_error(status.Text());

        _forward.reset(_regexp->CompileToProg(max_mem * 2 / 3));
        _reverse.reset(_regexp->CompileToReverseProg(max_mem / 3));

        if (_forward == NULL || _reverse == NULL)
            throw std::runtime_error("could not compile regexp");

        _native.reset(new native{_forward.get()});
    }


    bool it::match(const re2::StringPiece& text, RE2::Anchor anchor,
                         re2::StringPiece* groups, int ngroups) const
    {
        unsigned int flags = 0;

        if (_forward->anchor_start() || anchor != RE2::UNANCHORED)
            flags |= RE2JIT_ANCHOR_START;

        if (_forward->anchor_end() || anchor == RE2::ANCHOR_BOTH)
            flags |= RE2JIT_ANCHOR_END;

        if (!(flags & RE2JIT_ANCHOR_START)) {
            re2::StringPiece found;
            bool failed  = false;
            bool matched = _forward->SearchDFA(text, text, re2::Prog::kUnanchored,
                                               re2::Prog::kFirstMatch, &found, &failed, NULL);

            if (!failed) {
                if (!matched)
                    return 0;

                matched = _reverse->SearchDFA(found, text, re2::Prog::kAnchored,
                                              re2::Prog::kLongestMatch, &found, &failed, NULL);

                if (!failed && matched) {
                    if (ngroups > 1)
                        return it::match(found, RE2::ANCHOR_BOTH, groups, ngroups);

                    if (ngroups)
                        *groups = found;
                    return 1;
                }
            }
        }

        return _native->match(text, flags, groups, ngroups);
    }


    template <typename T>
    void it::_simple_deleter<T>::operator()(T *p) { delete p; }
    void it::_regexp_deleter::operator()(re2::Regexp *regex)
    {
        if (regex)
            regex->Decref();
    }


    template struct it::_simple_deleter<native>;
    template struct it::_simple_deleter<re2::Prog>;
};
