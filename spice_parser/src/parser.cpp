#include "parser.h"

#include <cctype>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_set>
#include <utility>
#include <vector>

namespace spice
{
namespace
{
bool is_space(char character)
{
    return std::isspace(static_cast<unsigned char>(character)) != 0;
}

char lower_char(char character)
{
    return static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
}

std::string to_lower(std::string_view text)
{
    std::string result(text);
    for (char& character : result)
    {
        character = lower_char(character);
    }
    return result;
}

bool iequals(std::string_view left, std::string_view right)
{
    if (left.size() != right.size())
    {
        return false;
    }
    for (std::size_t index = 0; index < left.size(); ++index)
    {
        if (lower_char(left[index]) != lower_char(right[index]))
        {
            return false;
        }
    }
    return true;
}

std::string quote(std::string_view text)
{
    return "'" + std::string(text) + "'";
}

std::string line_error(std::size_t line_number, const std::string& message)
{
    return "Line " + std::to_string(line_number) + ": " + message;
}

double parse_number(std::string_view token, std::size_t line_number)
{
    std::string_view text = token;
    if (!text.empty() && text.front() == '+')
    {
        text.remove_prefix(1);
    }

    double value = 0.0;
    const char* const begin = text.data();
    const char* const end = begin + text.size();
    const std::from_chars_result parsed =
        std::from_chars(begin, end, value, std::chars_format::general);

    if (parsed.ec == std::errc::result_out_of_range)
    {
        throw std::invalid_argument(line_error(
            line_number, "numeric value is out of range in " + quote(token)));
    }
    if (parsed.ec != std::errc() || parsed.ptr == begin)
    {
        throw std::invalid_argument(line_error(
            line_number, "invalid numeric value " + quote(token)));
    }
    if (!std::isfinite(value))
    {
        throw std::invalid_argument(line_error(
            line_number, "numeric value must be finite in " + quote(token)));
    }

    const std::string_view suffix(
        parsed.ptr, static_cast<std::size_t>(end - parsed.ptr));
    double multiplier = 1.0;

    if (suffix.empty())
    {
        multiplier = 1.0;
    }
    else if (suffix.size() == 3 && iequals(suffix, "meg"))
    {
        multiplier = 1e6;
    }
    else if (suffix.size() == 1)
    {
        switch (lower_char(suffix.front()))
        {
        case 't': multiplier = 1e12; break;
        case 'g': multiplier = 1e9; break;
        case 'k': multiplier = 1e3; break;
        case 'm': multiplier = 1e-3; break;
        case 'u': multiplier = 1e-6; break;
        case 'n': multiplier = 1e-9; break;
        case 'p': multiplier = 1e-12; break;
        case 'f': multiplier = 1e-15; break;
        default:
            throw std::invalid_argument(line_error(
                line_number, "unsupported numeric suffix in " + quote(token)));
        }
    }
    else
    {
        throw std::invalid_argument(line_error(
            line_number, "unsupported numeric suffix in " + quote(token)));
    }

    const double result = value * multiplier;
    if (!std::isfinite(result))
    {
        throw std::invalid_argument(line_error(
            line_number, "numeric value is out of range in " + quote(token)));
    }
    return result;
}

void split_tokens(std::string_view line, std::vector<std::string_view>& tokens)
{
    tokens.clear();
    std::size_t index = 0;
    while (index < line.size())
    {
        while (index < line.size() && is_space(line[index]))
        {
            ++index;
        }
        const std::size_t start = index;
        while (index < line.size() && !is_space(line[index]))
        {
            ++index;
        }
        if (index > start)
        {
            tokens.push_back(line.substr(start, index - start));
        }
    }
}

bool looks_like_designator(std::string_view token)
{
    return token.size() >= 2 &&
        std::isalpha(static_cast<unsigned char>(token[0])) != 0 &&
        std::isdigit(static_cast<unsigned char>(token[1])) != 0;
}

void register_component_name(std::unordered_set<std::string>& names,
                             std::string_view name,
                             std::size_t line_number)
{
    if (!names.insert(to_lower(name)).second)
    {
        throw std::invalid_argument(line_error(
            line_number, "duplicate component name " + quote(name)));
    }
}

void parse_component_line(const std::vector<std::string_view>& tokens,
                          std::size_t line_number,
                          circuit& result,
                          std::unordered_set<std::string>& component_names)
{
    if (tokens.empty())
    {
        return;
    }

    const std::string_view name = tokens[0];
    const char kind = static_cast<char>(
        std::toupper(static_cast<unsigned char>(name.front())));

    if (kind == 'R' || kind == 'C' || kind == 'V')
    {
        std::size_t value_index = 3;
        if (kind == 'V' && tokens.size() == 5)
        {
            if (!iequals(tokens[3], "dc"))
            {
                throw std::invalid_argument(line_error(
                    line_number,
                    "only DC voltage sources are supported for " + quote(name)));
            }
            value_index = 4;
        }

        if (tokens.size() != value_index + 1)
        {
            throw std::invalid_argument(line_error(
                line_number, "wrong number of fields for component " + quote(name)));
        }

        const double value = parse_number(tokens[value_index], line_number);

        std::vector<std::string> nodes;
        nodes.reserve(2);
        nodes.push_back(to_lower(tokens[1]));
        nodes.push_back(to_lower(tokens[2]));

        register_component_name(component_names, name, line_number);

        if (kind == 'R')
        {
            result.add_component(std::make_unique<resistor>(
                std::string(name), std::move(nodes), value));
        }
        else if (kind == 'C')
        {
            result.add_component(std::make_unique<capacitor>(
                std::string(name), std::move(nodes), value));
        }
        else
        {
            result.add_component(std::make_unique<voltage_source>(
                std::string(name), std::move(nodes), value));
        }
        return;
    }

    if (kind == 'M')
    {
        if (tokens.size() < 6)
        {
            throw std::invalid_argument(line_error(
                line_number, "MOSFET requires a name, 4 nodes, and a model"));
        }

        double width = 0.0;
        double length = 0.0;
        bool width_seen = false;
        bool length_seen = false;

        for (std::size_t index = 6; index < tokens.size(); ++index)
        {
            const std::string_view parameter = tokens[index];
            const std::size_t equals = parameter.find('=');
            if (equals == std::string_view::npos || equals == 0 ||
                equals + 1 == parameter.size())
            {
                throw std::invalid_argument(line_error(
                    line_number, "invalid MOSFET parameter " + quote(parameter)));
            }

            const std::string_view key = parameter.substr(0, equals);
            const std::string_view value_text = parameter.substr(equals + 1);

            if (iequals(key, "w") && !width_seen)
            {
                width = parse_number(value_text, line_number);
                width_seen = true;
            }
            else if (iequals(key, "l") && !length_seen)
            {
                length = parse_number(value_text, line_number);
                length_seen = true;
            }
            else
            {
                throw std::invalid_argument(line_error(
                    line_number,
                    "unsupported or duplicate MOSFET parameter " + quote(key)));
            }
        }

        std::vector<std::string> nodes;
        nodes.reserve(4);
        for (std::size_t index = 1; index <= 4; ++index)
        {
            nodes.push_back(to_lower(tokens[index]));
        }

        register_component_name(component_names, name, line_number);

        result.add_component(std::make_unique<mosfet>(
            std::string(name), std::move(nodes), std::string(tokens[5]),
            width, length));
        return;
    }

    throw std::invalid_argument(line_error(
        line_number, "unsupported component " + quote(name)));
}

std::string read_all(std::istream& input)
{
    std::string buffer;
    char chunk[8192];
    while (input.read(chunk, sizeof(chunk)) || input.gcount() > 0)
    {
        buffer.append(chunk, static_cast<std::size_t>(input.gcount()));
    }
    return buffer;
}
} // namespace

circuit parser::parse(std::istream& input) const
{
    const std::string buffer = read_all(input);
    if (input.bad())
    {
        throw std::runtime_error("Failed while reading SPICE netlist");
    }

    std::string_view content(buffer);
    if (content.size() >= 3 &&
        static_cast<unsigned char>(content[0]) == 0xEF &&
        static_cast<unsigned char>(content[1]) == 0xBB &&
        static_cast<unsigned char>(content[2]) == 0xBF)
    {
        content.remove_prefix(3);
    }

    circuit result;
    std::vector<std::string_view> tokens;
    tokens.reserve(8);
    std::unordered_set<std::string> component_names;
    component_names.reserve(64);

    std::size_t line_number = 0;
    std::size_t position = 0;
    bool first_content_line = true;

    while (position <= content.size())
    {
        std::size_t line_end = content.find('\n', position);
        if (line_end == std::string_view::npos)
        {
            line_end = content.size();
        }

        std::string_view line = content.substr(position, line_end - position);
        if (!line.empty() && line.back() == '\r')
        {
            line.remove_suffix(1);
        }
        position = line_end + 1;
        ++line_number;

        const std::size_t inline_comment = line.find_first_of("$;");
        if (inline_comment != std::string_view::npos)
        {
            line = line.substr(0, inline_comment);
        }

        const std::size_t first_non_space = line.find_first_not_of(" \t\r\v\f");
        if (first_non_space == std::string_view::npos)
        {
            continue;
        }
        line.remove_prefix(first_non_space);
        if (line.front() == '*')
        {
            continue;
        }

        split_tokens(line, tokens);
        if (tokens.empty())
        {
            continue;
        }

        if (tokens[0].front() == '.')
        {
            if (iequals(tokens[0], ".end"))
            {
                break;
            }
            if (iequals(tokens[0], ".subckt"))
            {
                throw std::invalid_argument(line_error(
                    line_number, "subcircuits are not supported"));
            }
            continue;
        }

        if (tokens[0].front() == '+')
        {
            throw std::invalid_argument(line_error(
                line_number, "continuation lines are not supported"));
        }

        if (first_content_line)
        {
            first_content_line = false;
            if (!looks_like_designator(tokens[0]))
            {
                // A SPICE netlist may begin with a free-form title line.
                continue;
            }
        }

        parse_component_line(tokens, line_number, result, component_names);
    }

    return result;
}

} // namespace spice
