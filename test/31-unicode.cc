// NFA operates on bytes; it could only fail this if it tried to do signed
// arithmetic on them, as UTF-8 continuations have most significant bit set.

// NOTE: this file should be saved in UTF-8.

// Those are perf. tests because `\p{x}` is horribly slow.
GROUP_PERF_TEST_EX(5000, "(\\p{L}*)", ANCHOR_BOTH, "latin", 2);
GROUP_PERF_TEST_EX(5000, "(\\p{L}*)", ANCHOR_BOTH, "кириллица", 2);
GROUP_PERF_TEST_EX(5000, "(\\p{L}*)", ANCHOR_BOTH, "ελληνικά", 2);
GROUP_PERF_TEST_EX(5000, "(\\p{L}*)", ANCHOR_BOTH, "繁體字", 2);
GROUP_PERF_TEST_EX(5000, "(\\p{L}*)", ANCHOR_BOTH, "カタカナ", 2);
GROUP_PERF_TEST_EX(5000, "(\\p{L}*)", ANCHOR_BOTH, "asdfg 1234567890 fail", 2);

GROUP_PERF_TEST_EX(9000, "(\\p{N}*)", ANCHOR_BOTH, "1234567890", 2);
GROUP_PERF_TEST_EX(9000, "(\\p{N}*)", ANCHOR_BOTH, "ⅠⅡⅢⅣⅤⅥⅦⅧⅨⅩⅪⅫⅬⅭⅮⅯ", 2);
// These two don't match even though I copied these characters from a Wikipedia page
// on numerals in Unicode.
GROUP_PERF_TEST_EX(9000, "(\\p{N}*)", ANCHOR_BOTH, "ΠΔΗΧΜ", 2);
GROUP_PERF_TEST_EX(9000, "(\\p{N}*)", ANCHOR_BOTH, ")𐅀𐅁𐅂𐅃𐅄𐅅𐅆𐅇𐅈𐅉𐅊𐅋𐅌𐅍𐅎𐅏", 2);
GROUP_PERF_TEST_EX(9000, "(\\p{N}*)", ANCHOR_BOTH, "12345 fail 67890", 2);
