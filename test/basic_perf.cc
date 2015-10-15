#define PERF_TEST(n, regex, input) \
  test_case("re2    " FG GREEN #regex FG RESET " on " FG CYAN #input FG RESET) { _RE2(r, regex); MEASURE(n, _RE2_FULL(r, input)); } \
  test_case("re2jit " FG GREEN #regex FG RESET " on " FG CYAN #input FG RESET) { _R2J(r, regex); MEASURE(n, _R2J_FULL(r, input)); }


PERF_TEST(1000000, "Hello, World!", "Hello, World!")
PERF_TEST(1000000, "[hH]ello,? +[Ww]orld(\\?|!|\\.|)", "Hello, World!")
PERF_TEST(1000000, "[hH]ello,? +[Ww]orld(\\?|!|\\.|)", "Hello, Worldd")
