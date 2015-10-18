GROUP_TEST("Hello, (.*)!", ANCHOR_START, "Hello, World!", 2);
GROUP_TEST("([^ ]*?), (.*)", UNANCHORED, "Hi there, world!", 3);

// Something more complex: correctly formatted integers and floating-point numbers.
GROUP_TEST("([+-]?(?:(0b)[01]+|(0o)[0-7]+|(0x)[0-9a-f]+|[0-9]+))", ANCHOR_BOTH, "0b00010", 5);
GROUP_TEST("([+-]?(?:(0b)[01]+|(0o)[0-7]+|(0x)[0-9a-f]+|[0-9]+((\\.[0-9]+)?(e[+-]?[0-9]+)?)(j)?))", ANCHOR_BOTH, "0b00010", 9);
GROUP_TEST("([+-]?(?:(0b)[01]+|(0o)[0-7]+|(0x)[0-9a-f]+|[0-9]+((\\.[0-9]+)?(e[+-]?[0-9]+)?)(j)?))", ANCHOR_BOTH, "30.5e+1j", 9);

// When a group A matches multiple times, the entry for each subgroup it contains
// should record the first location that subgroup matched at, even if these locations
// are within different matches of group A. For example, in the following regexps
// group `(\s+)` matches ' ' and group `(\w+)` matches 'submatch', even though
// their common parent group matches `submatch`.
GROUP_TEST("((\\s+)|(\\w+))+",   ANCHOR_START, "submatch test", 4);
GROUP_TEST("(?:(\\s+)|(\\w+))+", ANCHOR_START, "submatch test", 4);
