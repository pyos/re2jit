test_case("should not crash on incorrect regexp")
{
    re2jit::it regexp("(asd");
}


test_case("should have ok() = false, too")
{
    re2jit::it regexp("(asd");
    return !regexp.ok();
}
