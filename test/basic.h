//! library: re2jit
//! library: re2
#include <re2/re2.h>
#include <re2jit/it.h>


#define _RE2 RE2
#define _R2J re2jit::it

#define _RE2_RUN(r, text, anchor, gs, ngs) (r).Match(text, 0, sizeof(text) - 1, RE2::anchor, gs, ngs)
#define _R2J_RUN(r, text, anchor, gs, ngs) (r).match(text, RE2::anchor, gs, ngs)


#define _FORMAT(regex, anchor, input) \
    FG GREEN #regex FG RESET " (" #anchor ")" " on " FG CYAN #input FG RESET
