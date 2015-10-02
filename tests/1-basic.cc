#include "0-template.cc"


bool RE2LiteralMatch()
{
    return RE2::FullMatch("Hello, World!", "Hello, World!");
}


bool RE2AdvancedMatch1()
{
    return RE2::FullMatch("Hello, World!", "[hH]ello,? +[Ww]orld(\\?|!|\\.|)");
}


bool RE2AdvancedMatch2()
{
    return !RE2::FullMatch("Hello, re2!", "[hH]ello,? +[Ww]orld(\\?|!|\\.|)");
}


bool JITLiteralMatch()
{
    return RE2jit("Hello, World!").Match("Hello, World!");
}


Tests = {
    { "RE2 linkage check", RE2LiteralMatch },
    { "RE2 sanity check 1", RE2AdvancedMatch1 },
    { "RE2 sanity check 2", RE2AdvancedMatch2 },
    { "JIT linkage check", JITLiteralMatch },
};


#include "0-template-footer.cc"
