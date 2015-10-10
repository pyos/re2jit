test_case("mul_add multiplies and adds")
{
    return mul_add(1, 2, 3) == 5;
}


test_case("can create a JIT context")
{
    context = new jit_context();
}


test_case("can create a JIT version of mul_add")
{
    return build_mul_add(context);
}


test_case("mul_add_jit multiplies and adds")
{
    if (mul_add_jit == NULL) {
        return Result::Skip("JIT compilation failed");
    }

    jit_int a = 1, b = 2, c = 3, r;
    void *args[3] = { &a, &b, &c };
    mul_add_jit->apply(args, &r);
    return r == 5;
}


test_case("mul_add_jit can be invoked directly")
{
    if (mul_add_jit == NULL) {
        return Result::Skip("JIT compilation failed");
    }

    mul_add_type mul_add_jit_native = (mul_add_type) mul_add_jit->closure();

    return mul_add_jit_native != NULL
        && mul_add_jit_native(1, 2, 3) == 5;
}


test_case("cleanup")
{
    delete mul_add_jit;
    delete context;
}
