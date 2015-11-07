MATCH_TEST("x", UNANCHORED, "x", 0);
MATCH_TEST("x", UNANCHORED, "xyz", 0);
MATCH_TEST("x", UNANCHORED, "uvwx", 0);
MATCH_TEST("x", UNANCHORED, "uvwxyz", 0);

MATCH_TEST("x", ANCHOR_START, "x", 0);
MATCH_TEST("x", ANCHOR_START, "xyz", 0);
MATCH_TEST("x", ANCHOR_START, "uvwx", 0);
MATCH_TEST("x", ANCHOR_START, "uvwxyz", 0);

MATCH_TEST("x", ANCHOR_BOTH, "x", 0);
MATCH_TEST("x", ANCHOR_BOTH, "xyz", 0);
MATCH_TEST("x", ANCHOR_BOTH, "uvwx", 0);
MATCH_TEST("x", ANCHOR_BOTH, "uvwxyz", 0);

MATCH_TEST("^x", UNANCHORED, "x", 0);
MATCH_TEST("^x", UNANCHORED, "xyz", 0);
MATCH_TEST("^x", UNANCHORED, "uvwx", 0);
MATCH_TEST("^x", UNANCHORED, "uvwxyz", 0);

MATCH_TEST("x$", UNANCHORED, "x", 0);
MATCH_TEST("x$", UNANCHORED, "xyz", 0);
MATCH_TEST("x$", UNANCHORED, "uvwx", 0);
MATCH_TEST("x$", UNANCHORED, "uvwxyz", 0);

MATCH_TEST("^x$", UNANCHORED, "x", 0);
MATCH_TEST("^x$", UNANCHORED, "xyz", 0);
MATCH_TEST("^x$", UNANCHORED, "uvwx", 0);
MATCH_TEST("^x$", UNANCHORED, "uvwxyz", 0);
