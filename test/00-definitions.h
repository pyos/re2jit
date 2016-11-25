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


static char GARBAGE_GROUP[] = { 'x', 0 };


Result compare(bool am, bool bm, re2::StringPiece *a, re2::StringPiece *b, ssize_t n)
{
    if (am != bm) return Result::Fail("invalid answer %d", am);
    if (am) for (ssize_t i = 0; i < n; i++, a++, b++)
        if (b->data() != GARBAGE_GROUP && *a != *b)
            return Result::Fail(
                "group %zu incorrect\n"
                "    expected [%d @ %p] '%.*s'\n"
                "    matched  [%d @ %p] '%.*s'", i,
                b->size(), b->data(), std::min(b->size(), 50), b->data(),
                a->size(), a->data(), std::min(a->size(), 50), a->data());
    return Result::Pass("= %d", am);
}


#if __APPLE__ && __MACH__
#include <mach/clock.h>
#include <mach/mach.h>

static clock_serv_t mach_clock;

static void __attribute__((constructor)) mach_clock_init(void) {
    host_get_clock_service(mach_host_self(), SYSTEM_CLOCK, &mach_clock);
}

static void __attribute__((destructor)) mach_clock_fini(void) {
    mach_port_deallocate(mach_task_self(), mach_clock);
}
#endif


static inline struct timespec clock_monotonic(void) {
#if __APPLE__ && __MACH__
    mach_timespec_t val;
    clock_get_time(mach_clock, &val);
#else
    struct timespec val;
    clock_gettime(CLOCK_MONOTONIC, &val);
#endif
    return (struct timespec){val.tv_sec, val.tv_nsec};
}


template <typename F> double measure(int k, const F&& fn)
{
    struct timespec start = clock_monotonic();
    while (k--) fn();
    struct timespec end = clock_monotonic();
    auto s = end.tv_sec  - start.tv_sec;
    auto n = end.tv_nsec - start.tv_nsec;
    return n * 1e-9 + s;
}


#define GENERIC_TEST(name, regex, anchor, _input, ngroups, __fn, answer, ...) \
    test_case(name) {                                                         \
        re2::StringPiece input = _input;                                      \
        re2::StringPiece rgroups[ngroups];                                    \
        re2::StringPiece egroups[ngroups] = { __VA_ARGS__ };                  \
        re2jit::it _r(regex);                                                 \
        if (!_r.ok()) return Result::Fail("%s", _r.error().c_str());          \
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
