#include "parser.h"

#include <exception>
#include <fstream>
#include <iostream>

namespace
{
int parse_and_report(std::istream& input)
{
    const spice::parser netlist_parser;
    const spice::circuit parsed = netlist_parser.parse(input);

    std::cout << "Parsed " << parsed.component_count() << " components across "
              << parsed.node_count() << " nodes.\n"
              << "Ground node (0): "
              << (parsed.has_ground() ? "present" : "not found") << '\n';
    return 0;
}
} // namespace

int main(int argc, char* argv[])
{
    if (argc > 2)
    {
        std::cerr << "Usage: spice_parser [netlist-file]\n";
        return 2;
    }

    if (argc == 1)
    {
        try
        {
            return parse_and_report(std::cin);
        }
        catch (const std::exception& error)
        {
            std::cerr << "Parse error: " << error.what() << '\n';
            return 1;
        }
    }

    std::ifstream input(argv[1]);
    if (!input)
    {
        std::cerr << "Cannot open netlist file: " << argv[1] << '\n';
        return 1;
    }

    try
    {
        return parse_and_report(input);
    }
    catch (const std::exception& error)
    {
        std::cerr << "Parse error: " << error.what() << '\n';
        return 1;
    }
}
