FIXED_TEST("(cat|dog|)\\1", ANCHOR_START, "catcat", true, "catcat", "cat");
FIXED_TEST("(cat|dog|)\\1", ANCHOR_START, "dogdog", true, "dogdog", "dog");
FIXED_TEST("(cat|dog|)\\1", ANCHOR_START, "snek",   true, "",       "");
FIXED_TEST("(cat|dog|)\\1", ANCHOR_START, "catdog", false);
FIXED_TEST("(cat|dog|)\\1", ANCHOR_START, "dogcat", false);
FIXED_TEST("(cat|dog|)\\1", ANCHOR_START, "cat",    false);
FIXED_TEST("(cat|dog)\\1",  UNANCHORED,   "dogcatdogsnekcatdogdogcatcatsnek", true, "dogdog", "dog");
FIXED_TEST("(cat|dog)\\1",  UNANCHORED,   "dogcatdogsnekcatdogcatdogcatsnek", false);
FIXED_TEST("(cat|dog)(cat|dog)(?:\\1|\\2)", ANCHOR_START, "catcatcat", true, "catcatcat", "cat", "cat");
FIXED_TEST("(cat|dog)(cat|dog)(?:\\1|\\2)", ANCHOR_START, "catdogcat", true, "catdogcat", "cat", "dog");
FIXED_TEST("(cat|dog)(cat|dog)(?:\\1|\\2)", ANCHOR_START, "catdogdog", true, "catdogdog", "cat", "dog");
FIXED_TEST("(cat|dog)(cat|dog)(?:\\1|\\2)", ANCHOR_START, "catcatdog", false);

REGEX_3SAT_TEST(3, true, {{1, 2, -3}, {1, -2, 3}, {-1, -2, 3}, {-1, -2, -3}});
