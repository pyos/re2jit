#include <algorithm>
#include <ctime>

#include <re2/re2.h>
#include <re2jit/it.h>


#define FORMAT_NAME(regex, anchor, input) \
    FG GREEN #regex FG RESET " on " FG CYAN #input FG RESET " (" #anchor ")"


template <typename T>
bool match(const T &regexp, const re2::StringPiece& text, RE2::Anchor anchor,
                                  re2::StringPiece* groups, int ngroups);


template <>
bool match<RE2>(const RE2 &regexp, const re2::StringPiece& text, RE2::Anchor anchor,
                                         re2::StringPiece* groups, int ngroups)
{
    return regexp.Match(text, 0, text.size(), anchor, groups, ngroups);
}


template <>
bool match<re2jit::it>(const re2jit::it &regexp, const re2::StringPiece& text, RE2::Anchor anchor,
                                                       re2::StringPiece* groups, int ngroups)
{
    return regexp.match(text, anchor, groups, ngroups);
}


Result compare(bool am, bool bm, re2::StringPiece *a, re2::StringPiece *b, ssize_t n)
{
    if (am != bm) return Result::Fail("invalid answer %d", am);
    if (am) for (ssize_t i = 0; i < n; i++, a++, b++)
        if (*a != *b)
            return Result::Fail(
                "group %zu incorrect\n"
                "    expected [%d @ %p] '%.*s'\n"
                "    matched  [%d @ %p] '%.*s'", i,
                b->size(), b->data(), std::min(b->size(), 50), b->data(),
                a->size(), a->data(), std::min(a->size(), 50), a->data());
    return Result::Pass("= %d", am);
}


#if __APPLE__

template <typename F> double measure(int, const F&&)
{
    return 0;
}

#else

template <typename F> double measure(int k, const F&& fn)
{
    struct timespec start;
    struct timespec end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    while (k--) fn();

    clock_gettime(CLOCK_MONOTONIC, &end);
    auto s = end.tv_sec  - start.tv_sec;
    auto n = end.tv_nsec - start.tv_nsec;
    return n * 1e-9 + s;
}

#endif


#define GENERIC_TEST(name, regex, anchor, _input, ngroups, __fn, answer, ...) \
    test_case(name) {                                                         \
        re2::StringPiece input = _input;                                      \
        re2::StringPiece rgroups[ngroups];                                    \
        re2::StringPiece egroups[ngroups] = { __VA_ARGS__ };                  \
        re2jit::it _r(regex);                                                 \
        auto fn = __fn;                                                       \
        return fn(match(_r, input, RE2::anchor, rgroups, ngroups),            \
                               answer, rgroups, egroups, ngroups);            \
    }


#define FIXED_TEST(regex, anchor, input, answer, ...)                     \
    GENERIC_TEST(FORMAT_NAME(regex, anchor, input), regex, anchor, input, \
        (sizeof((const char*[]){__VA_ARGS__})/sizeof(char*)), compare, answer, __VA_ARGS__)


#define MATCH_TEST_NAMED(name, regex, anchor, _input, ngroups, __fn) \
            GENERIC_TEST(name, regex, anchor, _input, ngroups, __fn, \
                         match(RE2(regex), input, RE2::anchor, egroups, ngroups))


#define MATCH_TEST(regex, anchor, input, n)           \
        MATCH_TEST_NAMED(                             \
                   FORMAT_NAME(regex, anchor, input), \
                   regex, anchor, input, n, compare)


#if RE2JIT_DO_PERF_TESTS

#define GENERIC_PERF_TEST(name, __n, setup, body, teardown) \
    test_case(name) {                                       \
        setup                                               \
        double __t = measure(__n, [&]() { body });          \
        teardown                                            \
        return Result::Pass("=> %f s", __t);                \
    }

#else

#define GENERIC_PERF_TEST(name, __n, setup, body, teardown)

#endif


#define PERF_TEST_NAMED(name, n, regex, anchor, _input, ngroups) \
    GENERIC_PERF_TEST(name " [re2]", n                           \
      , RE2 r(regex);                                            \
        re2::StringPiece m[ngroups];                             \
        re2::StringPiece i(_input, sizeof(_input) - 1);          \
      , match(r, i, RE2::anchor, m, ngroups);                    \
      , {});                                                     \
                                                                 \
    GENERIC_PERF_TEST(name " [jit]", n                           \
      , re2jit::it r(regex);                                     \
        re2::StringPiece m[ngroups];                             \
        re2::StringPiece i(_input, sizeof(_input) - 1);          \
      , match(r, i, RE2::anchor, m, ngroups);                    \
      , {})


#define MATCH_PERF_TEST_NAMED(name, n, regex, anchor, input, ngroups)   \
             MATCH_TEST_NAMED(name,    regex, anchor, input, ngroups, compare); \
              PERF_TEST_NAMED(name, n, regex, anchor, input, ngroups)


#define MATCH_PERF_TEST(n, regex, anchor, input, ngroups)  \
        MATCH_PERF_TEST_NAMED(                             \
                        FORMAT_NAME(regex, anchor, input), \
                        n, regex, anchor, input, ngroups)
