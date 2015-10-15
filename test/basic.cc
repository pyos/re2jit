#define SIMPLE_TEST(regex, anchor, input) \
    test_case(_FORMAT(regex, anchor, input)) { \
        return _RE2_RUN(_RE2(regex), input, anchor, NULL, 0) \
            == _R2J_RUN(_R2J(regex), input, anchor, NULL, 0); \
    }


SIMPLE_TEST("Hello, World!", ANCHOR_BOTH, "Hello, World!");
SIMPLE_TEST("[hH]ello,? +[Ww]orld(\\?|!|\\.|)", UNANCHORED, "Hello, World");
SIMPLE_TEST("[hH]ello,? +[Ww]orld(\\?|!|\\.|)", UNANCHORED, "Hello, World! 2");
SIMPLE_TEST("[hH]ello,? +[Ww]orld(\\?|!|\\.|)", UNANCHORED, "1 Hello, World!");
SIMPLE_TEST("[hH]ello,? +[Ww]orld(\\?|!|\\.|)", UNANCHORED, "1 Hello, World! 2");
SIMPLE_TEST("[hH]ello,? +[Ww]orld(\\?|!|\\.|)", ANCHOR_START, "Hello, World");
SIMPLE_TEST("[hH]ello,? +[Ww]orld(\\?|!|\\.|)", ANCHOR_START, "Hello, World! 2");
SIMPLE_TEST("[hH]ello,? +[Ww]orld(\\?|!|\\.|)", ANCHOR_START, "1 Hello, World!");
SIMPLE_TEST("[hH]ello,? +[Ww]orld(\\?|!|\\.|)", ANCHOR_BOTH, "Hello, World");
SIMPLE_TEST("[hH]ello,? +[Ww]orld(\\?|!|\\.|)", ANCHOR_BOTH, "Hello, World! 2");
SIMPLE_TEST("[hH]ello,? +[Ww]orld(\\?|!|\\.|)", ANCHOR_BOTH, "1 Hello, World!");
