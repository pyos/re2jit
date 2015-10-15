test_case("re2 linkage")
{
    RE2 regex("Hello, World!");

    MEASURE(1000000, RE2::FullMatch("Hello, World!", regex));
}


test_case("re2 regex match")
{
    re2::StringPiece xs;
    RE2 regex("[hH]ello,? +[Ww]orld(\\?|!|\\.|)");

    MEASURE(1000000, RE2::FullMatch("Hello, World!", regex, &xs));
    re2jit::Debug::Write("=> %s\n", xs.data());
}


test_case("re2 regex non-match")
{
    re2::StringPiece xs;
    RE2 regex("[hH]ello,? +[Ww]orld(\\?|!|\\.|)");

    MEASURE(1000000, RE2::FullMatch("Hello, re2!", regex, &xs));
    re2jit::Debug::Write("=> %s\n", xs.data());
}


test_case("re2jit linkage")
{
    RE2jit regex("Hello, World!");

    MEASURE(1000000, regex.Match("Hello, World!"));
}


test_case("re2jit regex match")
{
    re2::StringPiece xs;
    RE2jit regex("[hH]ello,? +[Ww]orld(\\?|!|\\.|)");

    MEASURE(1000000, regex.Match("Hello, World!", RE2::ANCHOR_START, &xs, 1));
    re2jit::Debug::Write("=> %s\n", xs.data());
}


test_case("re2jit regex non-match")
{
    re2::StringPiece xs;
    RE2jit regex("[hH]ello,? +[Ww]orld(\\?|!|\\.|)");

    MEASURE(1000000, regex.Match("Hello, re2!", RE2::ANCHOR_BOTH, &xs, 1));
    re2jit::Debug::Write("=> %s\n", xs.data());
}
