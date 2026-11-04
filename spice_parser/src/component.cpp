#include "component.h"

#include <cmath>
#include <stdexcept>
#include <string>
#include <utility>

namespace spice
{
namespace
{
void validate_finite_value(double value, const char* parameter_name)
{
    if (!std::isfinite(value))
    {
        throw std::invalid_argument(
            std::string(parameter_name) + " must be a finite number");
    }
}

void validate_node_count(const component& part, std::size_t expected)
{
    if (part.num_nodes() != expected)
    {
        throw std::invalid_argument(
            std::string(part.type()) + " component requires exactly " +
            std::to_string(expected) + " nodes");
    }
}
} // namespace

component::component(std::string name, std::vector<std::string> nodes)
    : name_(std::move(name)),
      nodes_(std::move(nodes))
{
    if (name_.empty())
    {
        throw std::invalid_argument("Component name cannot be empty");
    }
    if (nodes_.empty())
    {
        throw std::invalid_argument("Component must have at least one node");
    }
    for (const std::string& node : nodes_)
    {
        if (node.empty())
        {
            throw std::invalid_argument("Component node name cannot be empty");
        }
    }
}

component::~component() = default;
component::component(component&&) noexcept = default;
component& component::operator=(component&&) noexcept = default;

const std::string& component::name() const
{
    return name_;
}

const std::vector<std::string>& component::nodes() const
{
    return nodes_;
}

std::size_t component::num_nodes() const
{
    return nodes_.size();
}

resistor::resistor(
    std::string name, std::vector<std::string> nodes, double resistance)
    : component(std::move(name), std::move(nodes)),
      resistance_(resistance)
{
    validate_node_count(*this, 2);
    validate_finite_value(resistance_, "Resistance");
}

std::string_view resistor::type() const
{
    return "R";
}

double resistor::value() const
{
    return resistance_;
}

double resistor::resistance() const
{
    return resistance_;
}

capacitor::capacitor(
    std::string name, std::vector<std::string> nodes, double capacitance)
    : component(std::move(name), std::move(nodes)),
      capacitance_(capacitance)
{
    validate_node_count(*this, 2);
    validate_finite_value(capacitance_, "Capacitance");
}

std::string_view capacitor::type() const
{
    return "C";
}

double capacitor::value() const
{
    return capacitance_;
}

double capacitor::capacitance() const
{
    return capacitance_;
}

voltage_source::voltage_source(
    std::string name, std::vector<std::string> nodes, double voltage)
    : component(std::move(name), std::move(nodes)),
      voltage_(voltage)
{
    validate_node_count(*this, 2);
    validate_finite_value(voltage_, "Voltage");
}

std::string_view voltage_source::type() const
{
    return "V";
}

double voltage_source::value() const
{
    return voltage_;
}

double voltage_source::voltage() const
{
    return voltage_;
}

mosfet::mosfet(std::string name,
               std::vector<std::string> nodes,
               std::string model,
               double width,
               double length)
    : component(std::move(name), std::move(nodes)),
      model_(std::move(model)),
      width_(width),
      length_(length)
{
    validate_node_count(*this, 4);
    if (model_.empty())
    {
        throw std::invalid_argument("MOSFET model name cannot be empty");
    }
    validate_finite_value(width_, "MOSFET width");
    validate_finite_value(length_, "MOSFET length");
}

std::string_view mosfet::type() const
{
    return "M";
}

double mosfet::value() const
{
    return width_;
}

const std::string& mosfet::model() const
{
    return model_;
}

double mosfet::width() const
{
    return width_;
}

double mosfet::length() const
{
    return length_;
}

} // namespace spice
