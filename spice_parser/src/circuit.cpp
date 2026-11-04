#include "circuit.h"

#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace spice
{

void circuit::add_component(component_ptr part)
{
    if (!part)
    {
        throw std::invalid_argument("Cannot add a null component to a circuit");
    }

    const component* added = part.get();
    const std::vector<std::string>& nodes = added->nodes();

    // A two-terminal device may list the same node twice (for example a
    // self-loop); register each node only once so node queries stay consistent.
    std::vector<const std::string*> unique_nodes;
    unique_nodes.reserve(nodes.size());
    for (const std::string& node : nodes)
    {
        bool duplicate = false;
        for (const std::string* existing : unique_nodes)
        {
            if (*existing == node)
            {
                duplicate = true;
                break;
            }
        }
        if (!duplicate)
        {
            unique_nodes.push_back(&node);
        }
    }

    components_.push_back(std::move(part));

    try
    {
        for (const std::string* node : unique_nodes)
        {
            adjacency_[*node].push_back(added);
        }
    }
    catch (...)
    {
        // Restore the circuit to its previous state if an allocation above
        // failed halfway through building the adjacency list.
        for (const std::string* node : unique_nodes)
        {
            const auto found = adjacency_.find(*node);
            if (found == adjacency_.end())
            {
                continue;
            }
            std::vector<const component*>& bucket = found->second;
            if (!bucket.empty() && bucket.back() == added)
            {
                bucket.pop_back();
            }
            if (bucket.empty())
            {
                adjacency_.erase(found);
            }
        }
        components_.pop_back();
        throw;
    }
}

const std::vector<const component*>* circuit::components_at_node(
    const std::string& node) const
{
    const auto found = adjacency_.find(node);
    if (found == adjacency_.end())
    {
        return nullptr;
    }

    return &found->second;
}

bool circuit::has_ground() const
{
    return adjacency_.find("0") != adjacency_.end();
}

} // namespace spice
