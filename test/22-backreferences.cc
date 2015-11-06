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
// Technically, all of the above languages are still regular because they are finite.
// Here's something non-regular for a change.
FIXED_TEST("(?i)<([A-Z][A-Z0-9]*)(?:[^A-Z0-9>][^>]*)?>.*?</\\1>", UNANCHORED, "some <b class='x'>bold</b> text", true, "<b class='x'>bold</b>", "b");
FIXED_TEST("(?i)<([A-Z][A-Z0-9]*)(?:[^A-Z0-9>][^>]*)?>.*?</\\1>", UNANCHORED, "no <boo class='x'>bold</b> text", false);
// That was easy. What if the match is not obvious?
FIXED_TEST("(?i)([A-Z0-9]+)([A-Z0-9]+)\\2\\1", UNANCHORED, "notregularregularnot", true, "notregularregularnot", "not", "regular");
// And we've gone NP-complete.
REGEX_3SAT_TEST(3, true, {{1, 2, -3}, {1, -2, 3}, {-1, -2, 3}, {-1, -2, -3}});
REGEX_3SAT_TEST(3, false, {{1, 2, 3}, {1, 2, -3}, {1, -2, 3}, {1, -2, -3}, {-1, 2, 3}, {-1, 2, -3}, {-1, -2, 3}, {-1, -2, -3}});
