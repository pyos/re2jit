#define PERF_TEST(n, regex, anchor, input) \
  test_case("re2    " _FORMAT(regex, anchor, input)) { _RE2 r(regex); MEASURE(n, _RE2_RUN(r, input, anchor, NULL, 0)); } \
  test_case("re2jit " _FORMAT(regex, anchor, input)) { _R2J r(regex); MEASURE(n, _R2J_RUN(r, input, anchor, NULL, 0)); }


PERF_TEST(1000000, "Hello, World!", ANCHOR_START, "Hello, World!")
PERF_TEST(1000000, "[hH]ello,? +[Ww]orld(\\?|!|\\.|)", ANCHOR_START, "Hello, World!")
PERF_TEST(1000000, "[hH]ello,? +[Ww]orld(\\?|!|\\.|)", ANCHOR_START, "Hello, Worldd")
