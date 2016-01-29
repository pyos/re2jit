#include "00-definitions.h"
#include <iostream>


bool getInput(std::istream &in, std::ostream &out, std::string prompt, std::string &result)
{
    std::string line;

    while (true) {
        out << prompt;
        out.flush();
        std::getline(in, line);

        if (!in)
            return false;

        if (line.size() != 0 && line[line.size() - 1] == '\\') {
            result += line.substr(0, line.size() - 1);
        } else {
            result += line;
            break;
        }
    }

    return true;
}
