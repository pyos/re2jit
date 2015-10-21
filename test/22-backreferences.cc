FIXED_TEST("(cat|dog|)\\1", ANCHOR_START, "catcat", true, "catcat", "cat");
FIXED_TEST("(cat|dog|)\\1", ANCHOR_START, "dogdog", true, "dogdog", "dog");
FIXED_TEST("(cat|dog|)\\1", ANCHOR_START, "snek",   true, "",       "");
FIXED_TEST("(cat|dog|)\\1", ANCHOR_START, "catdog", false);
FIXED_TEST("(cat|dog|)\\1", ANCHOR_START, "dogcat", false);
