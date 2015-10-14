#ifndef RE2JIT_JITPROG_H
#define RE2JIT_JITPROG_H


#include <re2/re2.h>


namespace re2jit {
    class JITProg {

    protected:
        re2::Prog *_prog;

    public:
        JITProg(re2::Prog * = NULL);
       ~JITProg();

        enum Status { NOT_JITTED, NOT_MATCHED, MATCHED };

        Status operator()(const re2::StringPiece& text, RE2::Anchor anchor = RE2::ANCHOR_START,
                                re2::StringPiece *match = NULL, int nmatch = 0) const;
    };
};


#endif
