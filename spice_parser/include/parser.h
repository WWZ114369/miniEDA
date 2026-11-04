#pragma once

#include "circuit.h"

#include <istream>

namespace spice
{

class parser
{
public:
    // Parses R, C, DC V, and 4-terminal M lines. Blank lines, '*' and '$'/';'
    // comments, directives, an optional first title line, and a UTF-8 BOM are
    // accepted. SPICE names are case-insensitive: node names are stored in
    // lower case and duplicate component names are rejected.
    // Throws std::invalid_argument for malformed or unsupported lines.
    circuit parse(std::istream& input) const;
};

} // namespace spice
