MATCH_TEST("x", ANCHOR_START, "x", 0);
MATCH_TEST("x", ANCHOR_START, "y", 0);
MATCH_TEST("\\\\1\\\\p{N}\\pN", ANCHOR_START, "\\1\\p{N}3", 0);
