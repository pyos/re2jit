// Infinite empty arrow loop, should halt anyway:
MATCH_TEST("(x*)*y", ANCHOR_START, "x");
MATCH_TEST("(x*)*y", ANCHOR_START, "xxxy");

// Exponential blowup in backtracking engines:
MATCH_TEST("(x+x+)+y", ANCHOR_START, "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
MATCH_TEST("(x+x+)+y", ANCHOR_START, "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxy");
