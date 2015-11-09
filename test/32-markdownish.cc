MARKDOWNISH_TEST("simple [re2]", RE2,        "**test**\n## test", "<p><strong>test</strong></p><h2>test</h2>");
MARKDOWNISH_TEST("simple [jit]", re2jit::it, "**test**\n## test", "<p><strong>test</strong></p><h2>test</h2>");
MARKDOWNISH_PERF_TEST(100000, "simple", "**test**\n## test");
MARKDOWNISH_FILE_PERF_TEST(100, "dg-tutorial.md", "test/32-markdownish-dg-tutorial.md");
