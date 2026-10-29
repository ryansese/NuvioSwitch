#include "data/addons/addon_display.hpp"

#include <cctype>
#include <unordered_map>

namespace nuvio::data::addons
{

namespace
{
bool isBlank(const std::string& s)
{
    for (unsigned char c : s)
        if (!std::isspace(c))
            return false;
    return true;
}
} // namespace

void applyDisplayNames(std::vector<domain::Addon>& addons, const std::map<std::string, std::string>& userNames)
{
    std::unordered_map<std::string, int> counts;
    for (auto& addon : addons)
    {
        auto custom = userNames.find(addon.baseUrl);
        if (custom != userNames.end() && !isBlank(custom->second) && custom->second != addon.name)
            addon.displayName = custom->second;
        else
            addon.displayName = addon.name;

        if (addon.displayName == addon.name)
            counts[addon.name]++;
    }

    std::unordered_map<std::string, int> seen;
    for (auto& addon : addons)
    {
        if (addon.displayName != addon.name || counts[addon.name] <= 1)
            continue;
        int occurrence = ++seen[addon.name];
        if (occurrence > 1)
            addon.displayName = addon.name + " (" + std::to_string(occurrence) + ")";
    }
}

} // namespace nuvio::data::addons
