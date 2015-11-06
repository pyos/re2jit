#include "00-definitions.h"
#include <stdio.h>


static int iabs(int i)
{
    return i < 0 ? -i : i;
}


// http://perl.plover.com/NPC/NPC-3SAT.html
static std::string regex_3sat_input(int inputs, int clauses)
{
    std::string x(inputs, 'x');
    x += ";";
    for (int i = 0; i < clauses; i++) x += "x,";
    return x;
}


static std::string regex_3sat_regexp(int inputs, int clauses, int (*cl)[3])
{
    // '^' => RE2::ANCHOR_START
    char intbuf[100];
    std::string r;

    for (int i = 0; i < inputs; i++) r += "(x?)";

    r += ".*;";

    for (int i = 0; i < clauses; i++) {
        snprintf(intbuf, sizeof(intbuf) - 1,
            "(?:\\%d%s|\\%d%s|\\%d%s),",
            iabs(cl[i][0]), cl[i][0] < 0 ? "x" : "",
            iabs(cl[i][1]), cl[i][1] < 0 ? "x" : "",
            iabs(cl[i][2]), cl[i][2] < 0 ? "x" : "");
        r += intbuf;
    }

    return r;
}


static inline bool regex_3sat_run(int inputs, int clauses, int (*cl)[3], bool *out)
{
    std::string r(regex_3sat_regexp(inputs, clauses, cl));
    std::string i(regex_3sat_input(inputs, clauses));

    re2::StringPiece *vars = new re2::StringPiece[inputs + 1];
    bool answer = re2jit::it(r).match(i, RE2::ANCHOR_START, vars, inputs + 1);
    if (!answer) {
        delete[] vars;
        return false;
    }

    for (int k = 0; k < inputs; k++) {
        out[k] = vars[k + 1].size() > 0;
    }

    delete[] vars;
    return true;
}


#define REGEX_3SAT_TEST(inputs, expect, ...)                                            \
    test_case("3sat on " #inputs " inputs: " FG BLUE #__VA_ARGS__ FG RESET) {           \
        int __cl[][3] = __VA_ARGS__;                                                    \
        int outs   = sizeof(__cl) / (sizeof(int) * 3);                                  \
        char buf[inputs + 1] = { 0 };                                                   \
        bool var[inputs];                                                               \
        bool ans = regex_3sat_run(inputs, outs, __cl, var);                             \
                                                                                        \
        for (int __k = 0; __k < inputs; __k++) buf[__k] = var[__k] ? '1' : '0';         \
        if (ans == expect)                                                              \
            return Result::Pass(ans ? "%s" : "none", buf);                              \
        else                                                                            \
            return Result::Fail(ans ? "%s" : "none", buf);                              \
    }
