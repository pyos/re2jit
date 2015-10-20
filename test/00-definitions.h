#include <algorithm>
#include <ctime>

#include <re2/re2.h>
#include <re2jit/it.h>


#define FORMAT_NAME(regex, anchor, input) \
    FG GREEN #regex FG RESET " on " FG CYAN #input FG RESET " (" #anchor ")"


// Time how long it takes to do `__xs` exactly `__n` times,
// save the result (as seconds) into `__out`.
#define MEASURE_LOOP(__out, __n, __xs) do { \
    struct timespec __a, __b;               \
    clock_gettime(CLOCK_MONOTONIC, &__a);   \
                                            \
    for (int __i = 0; __i < __n; __i++) {   \
        __xs;                               \
    }                                       \
                                            \
    clock_gettime(CLOCK_MONOTONIC, &__b);   \
    time_t __s = __b.tv_sec  - __a.tv_sec;  \
    long   __u = __b.tv_nsec - __a.tv_nsec; \
    __out = __u * 1.0e-9 + __s;             \
} while (0)


#define GENERIC_TEST(name, regex, anchor, _input, ngroups, __xs)                              \
    test_case(name) {                                                                         \
        RE2 r2(regex);                                                                        \
        re2jit::it rj(regex);                                                                 \
        const char input[] = _input;                                                          \
                                                                                              \
        re2::StringPiece r2groups[ngroups], rjgroups[ngroups];                                \
        int r2match = r2.Match(input, 0, sizeof(input) - 1, RE2::anchor, r2groups, ngroups);  \
        int rjmatch = rj.match(input, RE2::anchor, rjgroups, ngroups);                        \
        if (r2match != rjmatch) return Result::Fail("expected %d, got %d", r2match, rjmatch); \
                                                                                              \
        __xs;                                                                                 \
        return Result::Pass("= %d", rjmatch);                                                 \
    }


#define GENERIC_GROUP_TEST(name, regex, anchor, _input, ngroups, __xs)           \
    GENERIC_TEST(name, regex, anchor, _input, ngroups, {                         \
        if (rjmatch) for (size_t i = 0; i < ngroups; i++) {                      \
            const auto &g2 = r2groups[i];                                        \
            const auto &gj = rjgroups[i];                                        \
            if (g2 != gj) {                                                      \
                return Result::Fail(                                             \
                    "group %zu incorrect\n"                                      \
                    "    expected [%d @ %p] '%.*s'\n"                            \
                    "    matched  [%d @ %p] '%.*s'", i,                          \
                    g2.size(), g2.data(), std::min(g2.size(), 50), g2.data(),    \
                    gj.size(), gj.data(), std::min(gj.size(), 50), gj.data());   \
            }                                                                    \
        }                                                                        \
                                                                                 \
        __xs;                                                                    \
    })


#if RE2JIT_DO_PERF_TESTS

#define GENERIC_PERF_TEST(name, __n, setup, body, teardown) \
    test_case(name) {                                       \
        setup                                               \
        double __t;                                         \
        MEASURE_LOOP(__t, __n, body);                       \
        teardown                                            \
        return Result::Pass("=> %f s", __t);                \
    }

#else

#define GENERIC_PERF_TEST(name, __n, setup, body, teardown) \
    test_case(name) {                                       \
        return Result::Skip("=> skipped");                  \
    }

#endif


#define MATCH_TEST(regex, anchor, input) \
    GENERIC_TEST(FORMAT_NAME(regex, anchor, input), regex, anchor, input, 0, {})


#define GROUP_TEST(regex, anchor, input, n) \
    GENERIC_GROUP_TEST(FORMAT_NAME(regex, anchor, input), regex, anchor, input, n, {})


#define PERF_TEST(name, n, regex, anchor, _input, ngroups) \
    GENERIC_PERF_TEST(name " [re2]", n                     \
      , RE2 r(regex);                                      \
        re2::StringPiece m[ngroups];                       \
        const char input[] = _input;                       \
        const size_t size = sizeof(input) - 1;             \
      , r.Match(input, 0, size, RE2::anchor, m, ngroups)   \
      , {});                                               \
                                                           \
    GENERIC_PERF_TEST(name " [jit]", n                     \
      , re2jit::it r(regex);                               \
        re2::StringPiece m[ngroups];                       \
      , r.match(_input, RE2::anchor, m, ngroups)           \
      , {})


#ifdef RE2JIT_DO_PERF_TESTS

#define GROUP_PERF_TEST(name, n, regex, anchor, input, ngroups) \
    GENERIC_GROUP_TEST(name, regex, anchor, input, ngroups, {}); \
    PERF_TEST(name, n, regex, anchor, input, ngroups)

#else

#define GROUP_PERF_TEST(name, n, regex, anchor, input, ngroups) \
    GENERIC_GROUP_TEST(name, regex, anchor, input, ngroups, {})

#endif

#define GROUP_PERF_TEST_EX(n, regex, anchor, input, ngroups) \
    GROUP_PERF_TEST(FORMAT_NAME(regex, anchor, input), n, regex, anchor, input, ngroups)
