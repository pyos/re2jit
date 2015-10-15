test_case("re2 linkage")
{
    _RE2(r, "Hello, World!");
    return _RE2_FULL(r, "Hello, World!");
}


test_case("re2jit linkage")
{
    _R2J(r, "Hello, World!");
    return _R2J_FULL(r, "Hello, World!");
}


test_case("unanchored")
{
    _R2J(r, "[hH]ello,? +[Ww]orld(\\?|!|\\.|)");
    if (!_R2J_PARTIAL(r, "Hello, World"))     return Result::Fail("didn't match");
    if (!_R2J_PARTIAL(r, "Hello, World! 2"))  return Result::Fail("didn't ignore junk after end");
    if (!_R2J_PARTIAL(r, "1 Hello, World!"))  return Result::Fail("didn't skip over junk");
    if (!_R2J_PARTIAL(r, "1 Hello, World 2")) return Result::Fail("didn't locate text in the middle");
}


test_case("anchored at start")
{
    _R2J(r, "[hH]ello,? +[Ww]orld(\\?|!|\\.|)");
    if (!_R2J_STARTSWITH(r, "Hello, World"))    return Result::Fail("didn't match");
    if (!_R2J_STARTSWITH(r, "Hello, World! 2")) return Result::Fail("didn't ignore junk after end");
    if ( _R2J_STARTSWITH(r, "1 Hello, World!")) return Result::Fail("matched junk before start");
}


test_case("anchored at both start and end")
{
    _R2J(r, "[hH]ello,? +[Ww]orld(\\?|!|\\.|)");
    if (!_R2J_FULL(r, "Hello, World"))    return Result::Fail("didn't match");
    if ( _R2J_FULL(r, "Hello, World! 2")) return Result::Fail("matched junk after end");
    if ( _R2J_FULL(r, "1 Hello, World!")) return Result::Fail("matched junk before start");
}
