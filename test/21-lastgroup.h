#include "00-definitions.h"


#define LASTGROUP_TEST(regex, anchor, input, ngroups, expect)                          \
    MATCH_TEST_NAMED(FORMAT_NAME(regex, anchor, input), regex, anchor, input, ngroups, \
                                                                                       \
      [&](bool am, bool bm, re2::StringPiece *a, re2::StringPiece *b, ssize_t n) {     \
          Result r = compare(am, bm, a, b, n);                                         \
          if (r.state != Result::PASS)                                                 \
              return r;                                                                \
                                                                                       \
          std::string last = _r.lastgroup(rgroups, ngroups);                           \
          return last == expect                                                        \
               ? Result::Pass("= '%s'", expect)                                        \
               : Result::Fail("= '%s' != '%s'", last.c_str(), expect);                 \
      })
