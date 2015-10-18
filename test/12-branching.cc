GROUP_TEST("[xy]", ANCHOR_BOTH, "x", 1);
GROUP_TEST("[xy]", ANCHOR_BOTH, "y", 1);
GROUP_TEST("[xy]", ANCHOR_BOTH, "z", 1);

GROUP_TEST("(x|y|)", ANCHOR_BOTH, "", 1);
GROUP_TEST("(x|y|)", ANCHOR_BOTH, "x", 1);
GROUP_TEST("(x|y|)", ANCHOR_BOTH, "y", 1);
GROUP_TEST("(x|y|)", ANCHOR_BOTH, "z", 1);

GROUP_TEST("x*", ANCHOR_BOTH, "", 1);
GROUP_TEST("x*", ANCHOR_BOTH, "x", 1);
GROUP_TEST("x*", ANCHOR_BOTH, "xxxxxxxxxxxxxxxxx", 1);

GROUP_TEST(".*x", ANCHOR_START, "xzzzzzxzzzxxxxz", 1);
GROUP_TEST(".+x", ANCHOR_START, "xzzzzzxzzzxxxxz", 1);
GROUP_TEST(".*?x", ANCHOR_START, "xzzzzzxzzzxxxxz", 1);
GROUP_TEST(".+?x", ANCHOR_START, "xzzzzzxzzzxxxxz", 1);

GROUP_TEST("x+", ANCHOR_BOTH, "", 1);
GROUP_TEST("x+", ANCHOR_BOTH, "x", 1);
GROUP_TEST("x+", ANCHOR_BOTH, "xxxxxxxxxxxxxxxxx", 1);

GROUP_TEST("(?:[ab]{3,}|c*d{5})+", ANCHOR_BOTH, "ccccccddddd", 1);
GROUP_TEST("(?:[ab]{3,}|c*d{5})+", ANCHOR_BOTH, "aaabbaabcdddddaabbababbbb", 1);
GROUP_TEST("(?:[ab]{3,}|c*d{5})+", ANCHOR_BOTH, "aaabbaabcdddaabbababbbb", 1);
