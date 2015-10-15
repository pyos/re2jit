#include "debug.h"
#include "threads.h"


namespace re2jit {
    void it::_compile() {}
    void it::_destroy() {}

    status it::run_nfa(const re2::StringPiece&, RE2::Anchor,
                             re2::StringPiece*, int) const
    {
        return FAILED;
    }
};
