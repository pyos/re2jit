// Infinite empty arrow loop, should halt anyway:
MATCH_TEST("(x*)*y", ANCHOR_START, "x", 0);
MATCH_TEST("(x*)*y", ANCHOR_START, "xxxy", 0);

// Exponential blowup in backtracking engines:
MATCH_TEST("(x+x+)+y", ANCHOR_START, "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx", 0);
MATCH_TEST("(x+x+)+y", ANCHOR_START, "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxy", 0);
