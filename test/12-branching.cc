MATCH_TEST("[xy]", ANCHOR_BOTH, "x", 1);
MATCH_TEST("[xy]", ANCHOR_BOTH, "y", 1);
MATCH_TEST("[xy]", ANCHOR_BOTH, "z", 1);

MATCH_TEST("(x|z|)", ANCHOR_BOTH, "", 1);
MATCH_TEST("(x|z|)", ANCHOR_BOTH, "x", 1);
MATCH_TEST("(x|z|)", ANCHOR_BOTH, "y", 1);
MATCH_TEST("(x|z|)", ANCHOR_BOTH, "z", 1);

MATCH_TEST("x*", ANCHOR_BOTH, "", 1);
MATCH_TEST("x*", ANCHOR_BOTH, "x", 1);
MATCH_TEST("x*", ANCHOR_BOTH, "xxxxxxxxxxxxxxxxx", 1);

MATCH_TEST(".*x", ANCHOR_START, "xzzzzzxzzzxxxxz", 1);
MATCH_TEST(".+x", ANCHOR_START, "xzzzzzxzzzxxxxz", 1);
MATCH_TEST(".*?x", ANCHOR_START, "xzzzzzxzzzxxxxz", 1);
MATCH_TEST(".+?x", ANCHOR_START, "xzzzzzxzzzxxxxz", 1);

MATCH_TEST("x+", ANCHOR_BOTH, "", 1);
MATCH_TEST("x+", ANCHOR_BOTH, "x", 1);
MATCH_TEST("x+", ANCHOR_BOTH, "xxxxxxxxxxxxxxxxx", 1);

MATCH_TEST("(?:[ab]{3,}|c*d{5})+", ANCHOR_BOTH, "ccccccddddd", 1);
MATCH_TEST("(?:[ab]{3,}|c*d{5})+", ANCHOR_BOTH, "aaabbaabcdddddaabbababbbb", 1);
MATCH_TEST("(?:[ab]{3,}|c*d{5})+", ANCHOR_BOTH, "aaabbaabcdddaabbababbbb", 1);
