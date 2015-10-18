MATCH_TEST("[xy]", ANCHOR_BOTH, "x");
MATCH_TEST("[xy]", ANCHOR_BOTH, "y");
MATCH_TEST("[xy]", ANCHOR_BOTH, "z");

MATCH_TEST("(x|y|)", ANCHOR_BOTH, "");
MATCH_TEST("(x|y|)", ANCHOR_BOTH, "x");
MATCH_TEST("(x|y|)", ANCHOR_BOTH, "y");
MATCH_TEST("(x|y|)", ANCHOR_BOTH, "z");

MATCH_TEST("x*", ANCHOR_BOTH, "");
MATCH_TEST("x*", ANCHOR_BOTH, "x");
MATCH_TEST("x*", ANCHOR_BOTH, "xxxxxxxxxxxxxxxxx");

MATCH_TEST("x+", ANCHOR_BOTH, "");
MATCH_TEST("x+", ANCHOR_BOTH, "x");
MATCH_TEST("x+", ANCHOR_BOTH, "xxxxxxxxxxxxxxxxx");

MATCH_TEST("(?:[ab]{3,}|c*d{5})+", ANCHOR_BOTH, "ccccccddddd");
MATCH_TEST("(?:[ab]{3,}|c*d{5})+", ANCHOR_BOTH, "aaabbaabcdddddaabbababbbb");
MATCH_TEST("(?:[ab]{3,}|c*d{5})+", ANCHOR_BOTH, "aaabbaabcdddaabbababbbb");
