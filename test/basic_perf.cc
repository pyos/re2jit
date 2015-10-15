test_case("re2 linkage")
{
    _RE2(regex, "Hello, World!");
    MEASURE(1000000, _RE2_FULL(regex, "Hello, World!"));
}


test_case("re2 regex match")
{
    re2::StringPiece xs;
    _RE2(regex, "[hH]ello,? +[Ww]orld(\\?|!|\\.|)");
    MEASURE(1000000, _RE2_FULL(regex, "Hello, World!", &xs));
}


test_case("re2 regex non-match")
{
    re2::StringPiece xs;
    _RE2(regex, "[hH]ello,? +[Ww]orld(\\?|!|\\.|)");
    MEASURE(1000000, _RE2_FULL(regex, "Hello, re2!", &xs));
}


test_case("re2jit linkage")
{
    _R2J(regex, "Hello, World!");
    MEASURE(1000000, _R2J_FULL(regex, "Hello, World!"));
}


test_case("re2jit regex match")
{
    re2::StringPiece xs;
    _R2J(regex, "[hH]ello,? +[Ww]orld(\\?|!|\\.|)");
    MEASURE(1000000, _R2J_FULL(regex, "Hello, World!", &xs, 1));
}


test_case("re2jit regex non-match")
{
    re2::StringPiece xs;
    _R2J(regex, "[hH]ello,? +[Ww]orld(\\?|!|\\.|)");
    MEASURE(1000000, _R2J_FULL(regex, "Hello, re2!", &xs, 1));
}
