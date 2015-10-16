#define GROUP_TEST(regex, anchor, input, n) \
    test_case(_FORMAT(regex, anchor, input)) { \
        re2::StringPiece g2[n]; \
        re2::StringPiece gj[n]; \
        if ( _RE2_RUN(_RE2(regex), input, anchor, g2, n) \
          != _R2J_RUN(_R2J(regex), input, anchor, gj, n)) return Result::Fail("invalid answer"); \
        for (size_t i = 0; i < n; i++) { \
            if (g2[i] != gj[i]) { \
                return Result::Fail( \
                    "group %zu incorrect\n" \
                    "    expected [%d] '%s'\n" \
                    "    matched  [%d] '%s'", i, g2[i].size(), g2[i].data(), gj[i].size(), gj[i].data()); \
            } \
        } \
    }


GROUP_TEST("Hello, (.*)!", ANCHOR_START, "Hello, World!", 2);
