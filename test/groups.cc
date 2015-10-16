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
GROUP_TEST("([^ ]*?), (.*)", UNANCHORED, "Hi there, world!", 3);
GROUP_TEST("([+-]?(?:(0b)[01]+|(0o)[0-7]+|(0x)[0-9a-f]+))", ANCHOR_BOTH, "0b00010", 6);
GROUP_TEST("([+-]?(?:(0b)[01]+|(0o)[0-7]+|(0x)[0-9a-f]+|[0-9]+((?:\\.[0-9]+)?(?:e[+-]?[0-9]+)?)(j)?))", ANCHOR_BOTH, "0b00010", 6);
GROUP_TEST("([+-]?(?:(0b)[01]+|(0o)[0-7]+|(0x)[0-9a-f]+|[0-9]+((?:\\.[0-9]+)?(?:e[+-]?[0-9]+)?)(j)?))", ANCHOR_BOTH, "30.5e+1j", 6);
