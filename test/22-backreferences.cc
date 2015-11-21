FIXED_TEST("(cat|dog|)\\1", ANCHOR_START, "catcat", true, "catcat", "cat");
FIXED_TEST("(cat|dog|)\\1", ANCHOR_START, "dogdog", true, "dogdog", "dog");
FIXED_TEST("(cat|dog|)\\1", ANCHOR_START, "snek",   true, "",       "");
FIXED_TEST("(cat|dog)\\1", ANCHOR_START, "catdog", false, "", "");
FIXED_TEST("(cat|dog)\\1", ANCHOR_START, "dogcat", false, "", "");
FIXED_TEST("(cat|dog)\\1", ANCHOR_START, "cat",    false, "", "");
FIXED_TEST("(cat|dog)\\1",  UNANCHORED,   "dogcatdogsnekcatdogdogcatcatsnek", true, "dogdog", "dog");
FIXED_TEST("(cat|dog)\\1",  UNANCHORED,   "dogcatdogsnekcatdogcatdogcatsnek", false, "", "");
FIXED_TEST("(cat|dog)(cat|dog)(?:\\1|\\2)", ANCHOR_START, "catcatcat", true, "catcatcat", "cat", "cat");
FIXED_TEST("(cat|dog)(cat|dog)(?:\\1|\\2)", ANCHOR_START, "catdogcat", true, "catdogcat", "cat", "dog");
FIXED_TEST("(cat|dog)(cat|dog)(?:\\1|\\2)", ANCHOR_START, "catdogdog", true, "catdogdog", "cat", "dog");
FIXED_TEST("(cat|dog)(cat|dog)(?:\\1|\\2)", ANCHOR_START, "catcatdog", false, "", "", "");
// Technically, all of the above languages are still regular because they are finite.
// Here's something non-regular for a change.
FIXED_TEST("(?i)<([A-Z][A-Z0-9]*)(?:[^A-Z0-9>][^>]*)?>.*?</\\1>", UNANCHORED, "some <b class='x'>bold</b> text", true, "<b class='x'>bold</b>", "b");
FIXED_TEST("(?i)<([A-Z][A-Z0-9]*)(?:[^A-Z0-9>][^>]*)?>.*?</\\1>", UNANCHORED, "no <boo class='x'>bold</b> text", false, "", "");
// That was easy. What if the match is not obvious?
FIXED_TEST("(?i)([A-Z0-9]+)([A-Z0-9]+)\\2\\1", UNANCHORED, "notregularregularnot", true, "notregularregularnot", "not", "regular");
// Infinite epsilon transition loops may try to cause a crash due to repeated `malloc`s.
// (With a backreference, these regexps *are* exponential.)
FIXED_TEST("(x*)*y\\1?", ANCHOR_START, "x", false, "", "");
FIXED_TEST("(x*)*y\\1?", ANCHOR_START, "xxxy", true, "xxxy", "" /* first it matches "xxx", then "" */);
// PCRE can optimize the above regex, but not this one:
FIXED_TEST("(x*){1,100}[yz]", UNANCHORED, "xx", false, "", "");
// +1 # grep -P '(x*){1,100}[yz]' <<< 'xx'
// grep: exceeded PCRE's backtracking limit
FIXED_TEST("(x*){1,100}[yz]\\1?", UNANCHORED, "xx", false, "", "");
// Pretty sure there's a bug in glibc's regexec...
FIXED_TEST("(x*){4,}\\1", UNANCHORED, "", true, "", "");
// Ah, Perl...
REGEX_PRIMALITY_TEST(2, 0);
REGEX_PRIMALITY_TEST(3, 0);
REGEX_PRIMALITY_TEST(7, 0);
REGEX_PRIMALITY_TEST(13, 0);
REGEX_PRIMALITY_TEST(53, 0);
REGEX_PRIMALITY_TEST(101, 0);
REGEX_PRIMALITY_TEST(557, 0);
REGEX_PRIMALITY_TEST(1429, 0);
REGEX_PRIMALITY_TEST(4, 2);
REGEX_PRIMALITY_TEST(10, 2);
REGEX_PRIMALITY_TEST(25, 5);
REGEX_PRIMALITY_TEST(32, 2);
REGEX_PRIMALITY_TEST(75, 3);
REGEX_PRIMALITY_TEST(143, 11);
REGEX_PRIMALITY_TEST(1333, 31);
REGEX_PRIMALITY_TEST(1728, 2);
// And we've gone NP-complete.
REGEX_3SAT_TEST(3, true, {{1,2,-3},{1,-2,3},{-1,-2,3},{-1,-2,-3}});
REGEX_3SAT_TEST(3, false, {{1,2,3},{1,2,-3},{1,-2,3},{1,-2,-3},{-1,2,3},{-1,2,-3},{-1,-2,3},{-1,-2,-3}});
REGEX_RGB_TEST(4, true, {{1,2},{1,3},{2,3},{2,4},{3,4}});
REGEX_RGB_TEST(4, false, {{1,2},{1,3},{2,3},{2,4},{3,4},{1,4}});
