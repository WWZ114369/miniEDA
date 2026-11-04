#include "circuit.h"
#include "component.h"
#include "parser.h"
#include "test_framework.h"

#include <cmath>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace
{
using namespace spice;

void test_component_types_and_values()
{
    resistor r("R1", {"in", "out"}, 1000.0);
    capacitor c("C1", {"out", "0"}, 1e-6);
    voltage_source v("V1", {"in", "0"}, 5.0);
    mosfet m("M1", {"d", "g", "s", "0"}, "nmos", 2e-6, 1e-6);

    TEST_EXPECT(r.type() == "R");
    TEST_EXPECT(r.resistance() == 1000.0);
    TEST_EXPECT(c.type() == "C");
    TEST_EXPECT(std::abs(c.capacitance() - 1e-6) < 1e-15);
    TEST_EXPECT(v.type() == "V");
    TEST_EXPECT(v.voltage() == 5.0);
    TEST_EXPECT(m.type() == "M");
    TEST_EXPECT(m.model() == "nmos");
    TEST_EXPECT(m.num_nodes() == 4);
}

void test_component_rejects_invalid_data()
{
    bool wrong_node_count = false;
    try
    {
        resistor invalid("R1", {"only_one"}, 1.0);
    }
    catch (const std::invalid_argument&)
    {
        wrong_node_count = true;
    }
    TEST_EXPECT(wrong_node_count);

    bool invalid_value = false;
    try
    {
        resistor invalid("R2", {"a", "b"}, std::numeric_limits<double>::infinity());
    }
    catch (const std::invalid_argument&)
    {
        invalid_value = true;
    }
    TEST_EXPECT(invalid_value);
}

void test_circuit_adjacency_and_ground()
{
    circuit network;
    network.add_component(std::make_unique<resistor>(
        "R1", std::vector<std::string>{"in", "out"}, 100.0));
    network.add_component(std::make_unique<voltage_source>(
        "V1", std::vector<std::string>{"in", "0"}, 3.3));

    TEST_EXPECT(network.component_count() == 2);
    TEST_EXPECT(network.node_count() == 3);
    TEST_EXPECT(network.has_ground());

    const std::vector<const component*>* at_input =
        network.components_at_node("in");
    TEST_EXPECT(at_input != nullptr);
    if (at_input != nullptr)
    {
        TEST_EXPECT(at_input->size() == 2);
    }

    TEST_EXPECT(network.components_at_node("missing") == nullptr);
}

void test_circuit_ignores_duplicate_nodes_in_one_component()
{
    circuit network;
    network.add_component(std::make_unique<resistor>(
        "R1", std::vector<std::string>{"a", "a"}, 1.0));

    TEST_EXPECT(network.component_count() == 1);
    TEST_EXPECT(network.node_count() == 1);

    const std::vector<const component*>* at_node = network.components_at_node("a");
    TEST_EXPECT(at_node != nullptr && at_node->size() == 1);
}

void test_circuit_pointer_stability_after_growth()
{
    circuit network;
    network.add_component(std::make_unique<resistor>(
        "R1", std::vector<std::string>{"shared", "n1"}, 1.0));

    const component* first = nullptr;
    const std::vector<const component*>* before_growth =
        network.components_at_node("shared");
    if (before_growth != nullptr && !before_growth->empty())
    {
        first = before_growth->front();
    }

    for (int index = 2; index < 1000; ++index)
    {
        network.add_component(std::make_unique<resistor>(
            "R" + std::to_string(index),
            std::vector<std::string>{"shared", "n" + std::to_string(index)},
            static_cast<double>(index)));
    }

    const std::vector<const component*>* after_growth =
        network.components_at_node("shared");
    TEST_EXPECT(first != nullptr);
    TEST_EXPECT(after_growth != nullptr);
    if (after_growth != nullptr)
    {
        TEST_EXPECT(after_growth->size() == 999);
        TEST_EXPECT(after_growth->front() == first);
        TEST_EXPECT(after_growth->front()->name() == "R1");
    }
}

void test_parser_parses_supported_elements()
{
    std::istringstream input(
        "Simple divider\n"
        "* comment\n"
        ".title divider\n"
        "V1 in 0 DC 5\n"
        "R1 in out 1k\n"
        "C1 out 0 2u $ capacitor\n"
        "M1 d g s 0 nmos W=2u L=1u\n"
        ".end\n"
        "R2 ignored 0 10\n");

    const circuit network = parser().parse(input);
    TEST_EXPECT(network.component_count() == 4);
    TEST_EXPECT(network.has_ground());
    TEST_EXPECT(network.node_count() == 6);
}

void test_parser_is_case_insensitive_for_nodes()
{
    std::istringstream input("R1 IN OUT 1k\nR2 in out 2k\n");

    const circuit network = parser().parse(input);
    TEST_EXPECT(network.component_count() == 2);
    TEST_EXPECT(network.node_count() == 2);

    const std::vector<const component*>* at_node = network.components_at_node("in");
    TEST_EXPECT(at_node != nullptr && at_node->size() == 2);
}

void test_parser_skips_utf8_bom()
{
    const std::string text = "\xEF\xBB\xBF" "R1 a 0 1k\n";
    std::istringstream input(text);

    const circuit network = parser().parse(input);
    TEST_EXPECT(network.component_count() == 1);
    TEST_EXPECT(network.has_ground());
}

void test_parser_title_and_designator_heuristics()
{
    std::istringstream with_title("Resistor divider\nR1 a b 1k\n");
    TEST_EXPECT(parser().parse(with_title).component_count() == 1);

    std::istringstream unsupported("L1 a b 1m\n");
    bool rejected = false;
    try
    {
        (void)parser().parse(unsupported);
    }
    catch (const std::invalid_argument& error)
    {
        rejected = std::string(error.what()).find("unsupported component") !=
            std::string::npos;
    }
    TEST_EXPECT(rejected);
}

void test_parser_rejects_subcircuits()
{
    std::istringstream input(".subckt inv a y\nM1 y a 0 0 nmos\n.ends\n");
    bool rejected = false;
    try
    {
        (void)parser().parse(input);
    }
    catch (const std::invalid_argument& error)
    {
        rejected = std::string(error.what()).find("subcircuits") != std::string::npos;
    }
    TEST_EXPECT(rejected);
}

void test_parser_rejects_non_dc_voltage_source()
{
    std::istringstream input("V1 a 0 AC 5\n");
    bool rejected = false;
    try
    {
        (void)parser().parse(input);
    }
    catch (const std::invalid_argument& error)
    {
        rejected = std::string(error.what()).find("only DC") != std::string::npos;
    }
    TEST_EXPECT(rejected);
}

void test_parser_accepts_voltage_without_dc()
{
    std::istringstream input("V1 a 0 5\n");
    const circuit network = parser().parse(input);

    TEST_EXPECT(network.component_count() == 1);
    const std::vector<const component*>* at_node = network.components_at_node("a");
    TEST_EXPECT(at_node != nullptr && at_node->size() == 1);
    if (at_node != nullptr && !at_node->empty())
    {
        TEST_EXPECT(at_node->front()->value() == 5.0);
    }
}

void test_parser_rejects_duplicate_component_names()
{
    std::istringstream input("R1 a b 1k\nr1 c d 2k\n");
    bool rejected = false;
    try
    {
        (void)parser().parse(input);
    }
    catch (const std::invalid_argument& error)
    {
        rejected = std::string(error.what()).find("duplicate") != std::string::npos;
    }
    TEST_EXPECT(rejected);
}

void test_parser_supports_semicolon_comments()
{
    std::istringstream input("R1 a b 1k ; load resistor\n");
    TEST_EXPECT(parser().parse(input).component_count() == 1);
}

void test_parser_rejects_continuation_lines()
{
    std::istringstream input("+ R2 a b 1k\n");
    bool rejected = false;
    try
    {
        (void)parser().parse(input);
    }
    catch (const std::invalid_argument& error)
    {
        rejected = std::string(error.what()).find("continuation") != std::string::npos;
    }
    TEST_EXPECT(rejected);
}

void test_parser_numeric_suffixes()
{
    const std::vector<std::pair<std::string, double>> cases{
        {"1", 1.0},
        {"1t", 1e12},
        {"1g", 1e9},
        {"1meg", 1e6},
        {"1k", 1e3},
        {"1m", 1e-3},
        {"1u", 1e-6},
        {"1n", 1e-9},
        {"1p", 1e-12},
        {"1f", 1e-15},
        {"2.2K", 2200.0},
        {"1e3", 1000.0}};

    for (const auto& item : cases)
    {
        std::istringstream input("R1 a b " + item.first + "\n");
        const circuit network = parser().parse(input);
        const std::vector<const component*>* at_node = network.components_at_node("b");
        TEST_EXPECT(at_node != nullptr && !at_node->empty());
        if (at_node != nullptr && !at_node->empty())
        {
            const double tolerance = std::abs(item.second) * 1e-12 + 1e-15;
            TEST_EXPECT(std::abs(at_node->front()->value() - item.second) <= tolerance);
        }
    }
}

void test_parser_rejects_invalid_suffix()
{
    std::istringstream input("R1 a b 1x\n");
    bool rejected = false;
    try
    {
        (void)parser().parse(input);
    }
    catch (const std::invalid_argument&)
    {
        rejected = true;
    }
    TEST_EXPECT(rejected);
}

void test_parser_reads_mosfet_parameters()
{
    std::istringstream input("M1 d g s 0 nmos W=2u L=3u\n");
    const circuit network = parser().parse(input);

    const std::vector<const component*>* at_node = network.components_at_node("d");
    TEST_EXPECT(at_node != nullptr && !at_node->empty());
    if (at_node != nullptr && !at_node->empty())
    {
        const mosfet* device = dynamic_cast<const mosfet*>(at_node->front());
        TEST_EXPECT(device != nullptr);
        if (device != nullptr)
        {
            TEST_EXPECT(device->model() == "nmos");
            TEST_EXPECT(std::abs(device->width() - 2e-6) < 1e-18);
            TEST_EXPECT(std::abs(device->length() - 3e-6) < 1e-18);
        }
    }
}

void test_parser_handles_empty_input()
{
    std::istringstream input("");
    const circuit network = parser().parse(input);
    TEST_EXPECT(network.component_count() == 0);
    TEST_EXPECT(!network.has_ground());
}

void test_parser_rejects_malformed_input()
{
    std::istringstream input("Title\nR1 a b not-a-number\n");
    bool rejected = false;
    try
    {
        (void)parser().parse(input);
    }
    catch (const std::invalid_argument& error)
    {
        rejected = std::string(error.what()).find("Line 2") != std::string::npos;
    }
    TEST_EXPECT(rejected);
}
} // namespace

int main()
{
    test::run("component types and values", test_component_types_and_values);
    test::run("component validation", test_component_rejects_invalid_data);
    test::run("circuit adjacency and ground", test_circuit_adjacency_and_ground);
    test::run("circuit duplicate nodes", test_circuit_ignores_duplicate_nodes_in_one_component);
    test::run("circuit pointer stability", test_circuit_pointer_stability_after_growth);
    test::run("parser supported elements", test_parser_parses_supported_elements);
    test::run("parser case insensitive nodes", test_parser_is_case_insensitive_for_nodes);
    test::run("parser UTF-8 BOM", test_parser_skips_utf8_bom);
    test::run("parser title heuristic", test_parser_title_and_designator_heuristics);
    test::run("parser rejects subcircuits", test_parser_rejects_subcircuits);
    test::run("parser rejects non-DC sources", test_parser_rejects_non_dc_voltage_source);
    test::run("parser voltage without DC", test_parser_accepts_voltage_without_dc);
    test::run("parser rejects duplicate names", test_parser_rejects_duplicate_component_names);
    test::run("parser semicolon comments", test_parser_supports_semicolon_comments);
    test::run("parser continuation lines", test_parser_rejects_continuation_lines);
    test::run("parser numeric suffixes", test_parser_numeric_suffixes);
    test::run("parser invalid suffix", test_parser_rejects_invalid_suffix);
    test::run("parser MOSFET parameters", test_parser_reads_mosfet_parameters);
    test::run("parser empty input", test_parser_handles_empty_input);
    test::run("parser malformed input", test_parser_rejects_malformed_input);

    return test::failures == 0 ? 0 : 1;
}
