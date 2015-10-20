LASTGROUP_TEST("x", ANCHOR_START, "x", 1, "");
LASTGROUP_TEST("(x)", ANCHOR_START, "x", 2, "");
LASTGROUP_TEST("(?P<x>y)", ANCHOR_START, "y", 2, "x");

LASTGROUP_TEST("(?P<x>(y))", ANCHOR_START, "y", 3, "x");
LASTGROUP_TEST("(?P<x>(?P<y>z))", ANCHOR_START, "z", 3, "x");

LASTGROUP_TEST("(?P<x>y)z", ANCHOR_START, "yz", 2, "x");
LASTGROUP_TEST("(?P<x>y)(z)", ANCHOR_START, "yz", 3, "");
LASTGROUP_TEST("(?P<x>y)(?P<z>w)", ANCHOR_START, "yw", 3, "z");

LASTGROUP_TEST("(?P<x>y)|(?P<z>w)|u", ANCHOR_START, "y", 3, "x");
LASTGROUP_TEST("(?P<x>y)|(?P<z>w)|u", ANCHOR_START, "w", 3, "z");
LASTGROUP_TEST("(?P<x>y)|(?P<z>w)|u", ANCHOR_START, "u", 3, "");

LASTGROUP_TEST("(?P<x>y|(?P<z>w))", ANCHOR_START, "y", 3, "x");
LASTGROUP_TEST("(?P<x>y|(?P<z>w))", ANCHOR_START, "w", 3, "x");
