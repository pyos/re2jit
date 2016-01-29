test_case("starting the shell; press Ctrl+D to quit")
{
    std::cout << std::endl;

    const size_t ngroups = 64;
    re2::StringPiece groups[ngroups];

    while (true) {
        std::string regex;
        std::string input;

        if (!getInput(std::cin, std::cout, "regex > ", regex)) break;

        re2jit::it r(regex);

        if (!r.ok()) {
            std::cout << "\033[31merror\033[39m: " << r.error() << std::endl;
            continue;
        }

        if (!getInput(std::cin, std::cout, "input > ", input)) break;

        bool m = r.match(input, RE2::UNANCHORED, groups, ngroups);

        if (!m) {
            std::cout << "\033[31mfalse\033[39m" << std::endl;
            continue;
        }

        std::cout << "\033[32mtrue\033[39m" << std::endl;

        for (size_t i = 0; i < ngroups; i++) if (groups[i].data() != NULL) {
            std::cout << "  \033[33m#" << i << "\033[39m: " << groups[i].as_string() << std::endl;
        }
    }

    std::cout << std::endl;
    return true;
}
