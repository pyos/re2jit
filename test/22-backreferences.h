#include "00-definitions.h"
#include "framework.h"
#include <stdio.h>


// http://perl5.git.perl.org/perl.git/blob/d5e45555fc406945bd8e4694adaab59e4f99120e:/t/japh/abigail.t#l74
#define REGEX_PRIMALITY_TEST(input, expect)                                                   \
    test_case("primality test of " FG CYAN #input FG RESET) {                                 \
        re2::StringPiece buf[2];                                                              \
        re2jit::it("1?|(11+?)\\1+").match(std::string(input, '1'), RE2::ANCHOR_BOTH, buf, 2); \
                                                                                              \
        int div = buf[1].size();                                                              \
        if (div == expect)                                                                    \
            return Result::Pass(div ? "= k * %d" : "-- prime", div);                          \
        else                                                                                  \
            return Result::Fail(div ? "= k * %d" : "-- prime", div);                          \
    }


static inline std::string operator * (const std::string& x, int n)
{
    std::string r; while (n--) r += x; return r;
}


// http://perl.plover.com/NPC/NPC-3SAT.html
static inline bool regex_3sat(int inputs, int clauses, int (*cl)[3], char *out)
{
    char intbuf[256];
    std::string i = std::string(inputs, 'x') + ";" + std::string("x,") * clauses;
    std::string r = std::string("(x?)") * inputs + ".*;";

    for (int i = 0; i < clauses; i++) {
        snprintf(intbuf, sizeof(intbuf) - 1,
            "(?:\\%d%s|\\%d%s|\\%d%s),",
            abs(cl[i][0]), cl[i][0] < 0 ? "x" : "",
            abs(cl[i][1]), cl[i][1] < 0 ? "x" : "",
            abs(cl[i][2]), cl[i][2] < 0 ? "x" : "");
        r += intbuf;
    }

    std::vector<re2::StringPiece> vars(inputs + 1);
    bool answer = re2jit::it(r).match(i, RE2::ANCHOR_START, &vars[0], inputs + 1);

    for (int k = 0; k < inputs; k++)
        out[k] = vars[k + 1].size() > 0 ? '1' : '0';

    return answer;
}


#define REGEX_3SAT_TEST(inputs, expect, ...)                                      \
    test_case("3sat on " #inputs " inputs: " FG BLUE #__VA_ARGS__ FG RESET) {     \
        int __cl[][3] = __VA_ARGS__;                                              \
        char buf[inputs + 1] = { 0 };                                             \
        bool ans = regex_3sat(inputs, sizeof(__cl) / sizeof(int) / 3, __cl, buf); \
                                                                                  \
        if (ans == expect)                                                        \
            return Result::Pass(ans ? "= true @ %s" : "= false", buf);            \
        else                                                                      \
            return Result::Fail(ans ? "= true @ %s" : "= false", buf);            \
    }


// http://perl.plover.com/NPC/NPC-3COL.html
static inline bool regex_rgb(int vertices, int edges, int (*es)[2], char *out)
{
    char intbuf[256];
    std::string i = std::string("rgb\n") * vertices + ":" + std::string("\nrgbrbgr") * edges;
    std::string r = std::string(".*(.).*\\n") * vertices + ":";

    for (int i = 0; i < edges; i++) {
        snprintf(intbuf, sizeof(intbuf) - 1, "\\n.*\\%d\\%d.*", es[i][0], es[i][1]);
        r += intbuf;
    }

    std::vector<re2::StringPiece> vars(vertices + 1);
    bool answer = re2jit::it(r).match(i, RE2::ANCHOR_BOTH, &vars[0], vertices + 1);

    for (int k = 0; k < vertices; k++)
        out += sprintf(out, FG "%s" "x" FG RESET,
            vars[k].size()    == 0   ? YELLOW :
            vars[k].data()[0] == 'r' ? RED    :
            vars[k].data()[0] == 'g' ? GREEN  :
            vars[k].data()[0] == 'b' ? BLUE   : PURPLE);

    return answer;
}


#define REGEX_RGB_TEST(vertices, expect, ...)                                      \
    test_case("3col on " #vertices "-graph: " FG BLUE #__VA_ARGS__ FG RESET) {     \
        int __cl[][2] = __VA_ARGS__;                                               \
        char buf[vertices * 14 + 1] = { 0 };                                       \
        bool ans = regex_rgb(vertices, sizeof(__cl) / sizeof(int) / 2, __cl, buf); \
                                                                                   \
        if (ans == expect)                                                         \
            return Result::Pass(ans ? "= true @ %s" : "= false", buf);             \
        else                                                                       \
            return Result::Fail(ans ? "= true @ %s" : "= false", buf);             \
    }
