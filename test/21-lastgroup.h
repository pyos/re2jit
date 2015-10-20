#include "00-definitions.h"


#define LASTGROUP_TEST(regex, anchor, input, ngroups, expect)                      \
    GENERIC_GROUP_TEST(FORMAT_NAME(regex, anchor, input),                          \
                                   regex, anchor, input, ngroups, {                \
        std::string last = rj.lastgroup(rjgroups, ngroups);                        \
        if (last != expect)                                                        \
            return Result::Fail("got '%s' instead of '%s'", last.c_str(), expect); \
        return Result::Pass("= '%s'", expect);                                     \
    })
