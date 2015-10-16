#define PERF_TEST(n, regex, anchor, input, ngroups) \
  test_case("re2    " _FORMAT(regex, anchor, input)) { _RE2 r(regex); re2::StringPiece s[ngroups]; \
      MEASURE(n, _RE2_RUN(r, input, anchor, s, ngroups)); } \
  test_case("re2jit " _FORMAT(regex, anchor, input)) { _R2J r(regex); re2::StringPiece s[ngroups]; \
      MEASURE(n, _R2J_RUN(r, input, anchor, s, ngroups)); }


PERF_TEST(1000000, "Hello, World!", ANCHOR_START, "Hello, World!", 1)
PERF_TEST(1000000, "[hH]ello,?( +| $)[Ww]orld(\\?|!|\\.|)", ANCHOR_START, "Hello, World!", 3)
PERF_TEST(1000000, "[hH]ello,?( +| $)[Ww]orld(\\?|!|\\.|)", ANCHOR_START, "Hello, Worldd", 3)
PERF_TEST(10000, "(x*)*y", ANCHOR_START, "x", 2)
PERF_TEST(10000, "(x+x+)+y", ANCHOR_START, "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx", 2)
