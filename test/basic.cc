test_case("re2 linkage")
{
    return RE2::FullMatch("Hello, World!", "Hello, World!");
}


test_case("re2 regex match")
{
    return RE2::FullMatch("Hello, World!", "[hH]ello,? +[Ww]orld(\\?|!|\\.|)");
}


test_case("re2 regex non-match")
{
    return !RE2::FullMatch("Hello, re2!", "[hH]ello,? +[Ww]orld(\\?|!|\\.|)");
}


test_case("re2jit linkage")
{
    return RE2jit("Hello, World!").Match("Hello, World!");
}


test_case("re2jit regex match")
{
    return RE2jit("[hH]ello,? +[Ww]orld(\\?|!|\\.|)").Match("Hello, World!");
}


test_case("re2jit regex non-match")
{
    return !RE2jit("[hH]ello,? +[Ww]orld(\\?|!|\\.|)").Match("Hello, re2jit!");
}
