#include <string>
#include <iostream>


static const std::string FG_RED    = "\033[31m";
static const std::string FG_GREEN  = "\033[32m";
static const std::string FG_YELLOW = "\033[33m";
static const std::string FG_RESET  = "\033[39m";
static const std::string ANSI_UP   = "\033[A";
static const std::string ANSI_CLR  = "";


int main()
{
    size_t ok = 0;
    size_t total = sizeof(__Tests) / sizeof(TestDescription);

    for (size_t i = 0; i < total; i++) {
        std::cout << FG_YELLOW << "? " << FG_RESET << __Tests[i].name << std::endl;
        std::cout << ANSI_UP;

        if (__Tests[i].fn()) {
            ok++;
            std::cout << FG_GREEN << "+ ";
        } else {
            std::cout << FG_RED << "! ";
        }

        std::cout << std::endl;
    }

    std::cout << "  " << (ok == total ? FG_GREEN : FG_RED) << ok << "/" << total
              << FG_RESET << " tests passed" << std::endl;

    return ok != total;
}
