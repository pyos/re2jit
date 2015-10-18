#include "00-definitions.h"
#include <cstring>


#define LASTGROUP_TEST(regex, anchor, input, ngroups, expect)              \
    GENERIC_GROUP_TEST(FORMAT_NAME(regex, anchor, input),                  \
                                   regex, anchor, input, ngroups, {        \
        const char *last = rj.lastgroup(r2groups, ngroups);                \
        if (last == NULL)                                                  \
            return Result::Fail("got NULL instead of '%s'", expect);       \
        if (strcmp(last, expect))                                          \
            return Result::Fail("got '%s' instead of '%s'", last, expect); \
        return Result::Pass("= %s", expect);                               \
    })


#define LASTGROUP_NULL(regex, anchor, input, ngroups)               \
    GENERIC_GROUP_TEST(FORMAT_NAME(regex, anchor, input),           \
                                   regex, anchor, input, ngroups, { \
        const char *last = rj.lastgroup(r2groups, ngroups);         \
        if (last != NULL)                                           \
            return Result::Fail("got '%s' instead of NULL", last);  \
        return Result::Pass("= NULL");                              \
    })
