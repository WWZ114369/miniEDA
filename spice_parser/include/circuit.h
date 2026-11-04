#pragma once

#include "component.h"

#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace spice
{

class circuit
{
public:
    void add_component(component_ptr part);

    // Returns nullptr when the node is not present in the circuit. The returned
    // pointer stays valid until the circuit is destroyed or moved.
    const std::vector<const component*>* components_at_node(
        const std::string& node) const;

    // SPICE uses node "0" to represent ground.
    bool has_ground() const;

    std::size_t component_count() const { return components_.size(); }
    std::size_t node_count() const { return adjacency_.size(); }

private:
    // The component objects remain at stable addresses when this vector grows.
    std::vector<component_ptr> components_;
    std::unordered_map<std::string, std::vector<const component*>> adjacency_;
};

} // namespace spice
