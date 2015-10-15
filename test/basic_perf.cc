test_case("re2 'Hello, World!'")
{
    _RE2(regex, "Hello, World!");
    MEASURE(1000000, _RE2_FULL(regex, "Hello, World!"));
}


test_case("re2 '[hH]ello,? +[Ww]orld(\\?|!|\\.|)'")
{
    re2::StringPiece xs;
    _RE2(regex, "[hH]ello,? +[Ww]orld(\\?|!|\\.|)");

    MEASURE(1000000, {
        _RE2_FULL(regex, "Hello, World!", &xs);
        _RE2_FULL(regex, "Hello, Worldd", &xs);
    });
}


test_case("re2jit 'Hello, World!'")
{
    _R2J(regex, "Hello, World!");
    MEASURE(1000000, _R2J_FULL(regex, "Hello, World!"));
}


test_case("re2jit '[hH]ello,? +[Ww]orld(\\?|!|\\.|)'")
{
    re2::StringPiece xs;
    _R2J(regex, "[hH]ello,? +[Ww]orld(\\?|!|\\.|)");

    MEASURE(1000000, {
        _R2J_FULL(regex, "Hello, World!", &xs, 1);
        _R2J_FULL(regex, "Hello, Worldd", &xs, 1);
    });
}
