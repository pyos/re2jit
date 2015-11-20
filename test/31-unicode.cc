// NFA operates on bytes; it could only fail this if it tried to do signed
// arithmetic on them, as UTF-8 continuations have most significant bit set.

// NOTE: this file should be saved in UTF-8.

// Those are perf. tests because `\p{x}` is horribly slow.
MATCH_PERF_TEST(5000, "(\\p{L}*)", ANCHOR_BOTH, "latin", 2);
MATCH_PERF_TEST(5000, "(\\p{L}*)", ANCHOR_BOTH, "кириллица", 2);
MATCH_PERF_TEST(5000, "(\\p{L}*)", ANCHOR_BOTH, "ελληνικά", 2);
// Single-character categories need not be wrapped in brackets.
MATCH_PERF_TEST(5000, "(\\pL*)", ANCHOR_BOTH, "繁體字", 2);
MATCH_PERF_TEST(5000, "(\\pL*)", ANCHOR_BOTH, "カタカナ", 2);
MATCH_PERF_TEST(5000, "(\\p{L}*)", ANCHOR_BOTH, "asdfg 1234567890 fail", 2);
MATCH_PERF_TEST(9000, "(\\p{N}*)", ANCHOR_BOTH, "1234567890", 2);
MATCH_PERF_TEST(9000, "(\\p{N}*)", ANCHOR_BOTH, "ⅠⅡⅢⅣⅤⅥⅦⅧⅨⅩⅪⅫⅬⅭⅮⅯ", 2);
// These two don't match even though I copied these characters from a Wikipedia page
// on numerals in Unicode.
MATCH_PERF_TEST(9000, "(\\p{N}*)", ANCHOR_BOTH, "ΠΔΗΧΜ", 2);
MATCH_PERF_TEST(9000, "(\\p{N}*)", ANCHOR_BOTH, ")𐅀𐅁𐅂𐅃𐅄𐅅𐅆𐅇𐅈𐅉𐅊𐅋𐅌𐅍𐅎𐅏", 2);
MATCH_PERF_TEST(9000, "(\\p{N}*)", ANCHOR_BOTH, "12345 fail 67890", 2);
// Was initially [\p{Cc}\p{Cn}], but Cn is so large re2 doesn't even support it.
MATCH_PERF_TEST(9000, "([\\p{Cc}|\\p{Co}]*)", ANCHOR_BOTH, "\033\001\007\003", 2);
MATCH_PERF_TEST(9000, "([\\p{Cc}|\\p{Co}]*)", ANCHOR_BOTH, "\033[31m", 2);
// These fake opcodes shouldn't be accidentally treated as literal strings.
MATCH_TEST("literally \\p{L}", ANCHOR_BOTH, "literally L", 1);
MATCH_TEST("literally (?:\\p{L})+", ANCHOR_BOTH, "literally LLLLLL", 1);
// Lu != Ll
MATCH_TEST("UPPERCASE \\p{Lu}", ANCHOR_BOTH, "UPPERCASE L", 1);
MATCH_TEST("UPPERCASE \\p{Lu}", ANCHOR_BOTH, "UPPERCASE u", 1);
MATCH_TEST("lowercase \\p{Ll}", ANCHOR_BOTH, "lowercase L", 1);
MATCH_TEST("lowercase \\p{Ll}", ANCHOR_BOTH, "lowercase u", 1);
