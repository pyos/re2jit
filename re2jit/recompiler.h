#ifndef RE2JIT_RECOMPILER_H
#define RE2JIT_RECOMPILER_H

#include <re2/re2.h>


namespace re2jit {
    class Recompiler {

    protected:
        void *_prog;
        void *_entry;

    public:
        enum Status {
            ERR_NO_JIT  = -2,  // unsupported opcode in regex / no regex assigned
            ERR_GENERIC = -1,  // other errors
            NOT_MATCHED =  0,
            MATCHED     =  1,
        };

        Recompiler();
        Recompiler(re2::Prog&);
        Recompiler& operator = (re2::Prog&);
        Recompiler& operator = (re2::Prog*);
       ~Recompiler();

        Status Run(const re2::StringPiece& text, RE2::Anchor anchor = RE2::ANCHOR_START,
                         re2::StringPiece *match = NULL, int nmatch = 0) const;
    };
};


#endif
