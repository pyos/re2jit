//! library: re2jit
//! library: re2
#include <re2/re2.h>
#include <re2jit/re2jit.h>


#define _RE2(name, regex) RE2 name(regex)
#define _R2J(name, regex) RE2jit name(regex)

#define _RE2_FULL(regex, text, ...) RE2::FullMatch(text, regex, ##__VA_ARGS__)
#define _R2J_FULL(regex, text, ...) regex.Match(text, RE2::ANCHOR_BOTH, ##__VA_ARGS__)

#define _RE2_PARTIAL(regex, text, ...) RE2::PartialMatch(text, regex, ##__VA_ARGS__)
#define _R2J_PARTIAL(regex, text, ...) regex.Match(text, RE2::UNANCHORED, ##__VA_ARGS__)

// XXX no idea how to do start-anchored matches in re2.
#define _R2J_STARTSWITH(regex, text, ...) regex.Match(text, RE2::ANCHOR_START, ##__VA_ARGS__)
