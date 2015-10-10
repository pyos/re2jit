#ifndef RE2JIT_JITPROG_H
#define RE2JIT_JITPROG_H

#include <re2/re2.h>
#include <jit/jit-plus.h>


namespace re2jit {
    class JITProg : public jit_function {

    protected:
        jit_context *_context;
        re2::Prog *_prog;

        typedef int Closure(const char *text, int *groups, int ngroups);
        Closure *_ptr;

    public:
        JITProg(re2::Prog * = NULL);
       ~JITProg();

        enum Status { NOT_JITTED, NOT_MATCHED, MATCHED };

        Status operator()(const re2::StringPiece& text, RE2::Anchor anchor = RE2::ANCHOR_START,
                                re2::StringPiece *match = NULL, int nmatch = 0) const;
    };
};


#endif
