test_case("empty regexp, empty input")
{
    re2jit::JITProg::Status s = RE2jit("").ForceJITMatch("");

    if (s == re2jit::JITProg::NOT_JITTED) {
        return Result::Fail("not jitted");
    }

    return s == re2jit::JITProg::MATCHED;
}


test_case("empty regexp, non-empty input")
{
    re2jit::JITProg::Status s = RE2jit("").ForceJITMatch("Hello, World!");

    if (s == re2jit::JITProg::NOT_JITTED) {
        return Result::Fail("not jitted");
    }

    return s == re2jit::JITProg::MATCHED;
}
