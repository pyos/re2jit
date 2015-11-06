#ifdef test_case
// `make test/framework`
test_case("Result::Pass") return true;
test_case("Result::Fail") return false;
test_case("Result::Skip") return Result::Skip("with a %s", "message");
#else

#include <string>
#include <functional>

#define escape_lname(x) #x
#define escape_macro(x) escape_lname(x)


struct Result
{
    enum State { PASS, FAIL, SKIP } state;
    char info[4096] = { 0 };

    Result(bool r) : state(r ? PASS : FAIL) {}
    template <typename...T> static Result Pass(T... args) { return Result{PASS, args...}; }
    template <typename...T> static Result Fail(T... args) { return Result{FAIL, args...}; }
    template <typename...T> static Result Skip(T... args) { return Result{SKIP, args...}; }
    template <typename...T> Result(State s, T... args) : state(s) {
        snprintf(info, sizeof(info) - 1, args...);
    }
};


struct TestCase {
    std::string name;
    std::function<Result()> fn;
};


#include <stdio.h>

#define FG_HIDE "\033[8m"
#define FG_SHOW "\033[28m"
#define FG      "\033[3"
#define BG      "\033[4"
#define RED     "1m"
#define GREEN   "2m"
#define YELLOW  "3m"
#define BLUE    "4m"
#define PURPLE  "5m"
#define CYAN    "6m"
#define GRAY    "7m"
#define RESET   "9m"
// TEST = test definition file, TESTH = accompanying header file.
#include escape_macro(TESTH)


TestCase TESTS[] = {
    #define test_case_header(name) { name, []() -> Result {
    #define test_case_footer                              ; return true; } }
    #define test_case(name) test_case_footer, test_case_header(name)

    test_case_header("") return Result::Fail("should never run");
    #include escape_macro(TEST)
    test_case_footer
};


int main()
{
    for (size_t i = 1; i < sizeof(TESTS) / sizeof(*TESTS); i++) {
        fprintf(stdout, FG YELLOW "%zu" FG RESET ". %s ", i, TESTS[i].name.c_str());
        fflush(stdout);

        auto r = TESTS[i].fn();

        fprintf(stdout, "\r%s%zu" FG RESET ". %s %s\n",
            r.state == Result::PASS ? FG GREEN  :
            r.state == Result::FAIL ? FG RED    :
            r.state == Result::SKIP ? FG YELLOW : "", i, TESTS[i].name.c_str(), r.info);
    }

    return 0;
}

#endif
