#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace spice
{

class component
{
public:
    component(std::string name, std::vector<std::string> nodes);
    virtual ~component();

    component(const component&) = delete;
    component& operator=(const component&) = delete;
    component(component&&) noexcept;
    component& operator=(component&&) noexcept;

    const std::string& name() const;
    const std::vector<std::string>& nodes() const;
    std::size_t num_nodes() const;

    virtual std::string_view type() const = 0;

    // Electrical value of the device. For a MOSFET this is the channel width;
    // use width(), length() and model() for the full device geometry.
    virtual double value() const = 0;

private:
    std::string name_;
    std::vector<std::string> nodes_;
};

class resistor : public component
{
public:
    resistor(std::string name, std::vector<std::string> nodes, double resistance);

    std::string_view type() const override;
    double value() const override;
    double resistance() const;

private:
    double resistance_;
};

class capacitor : public component
{
public:
    capacitor(std::string name, std::vector<std::string> nodes, double capacitance);

    std::string_view type() const override;
    double value() const override;
    double capacitance() const;

private:
    double capacitance_;
};

class voltage_source : public component
{
public:
    voltage_source(std::string name, std::vector<std::string> nodes, double voltage);

    std::string_view type() const override;
    double value() const override;
    double voltage() const;

private:
    double voltage_;
};

class mosfet : public component
{
public:
    mosfet(std::string name,
           std::vector<std::string> nodes,
           std::string model,
           double width = 0.0,
           double length = 0.0);

    std::string_view type() const override;
    double value() const override;
    const std::string& model() const;
    double width() const;
    double length() const;

private:
    std::string model_;
    double width_;
    double length_;
};

using component_ptr = std::unique_ptr<component>;

} // namespace spice
