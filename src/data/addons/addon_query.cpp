#include "data/addons/addon_query.hpp"

#include <algorithm>

namespace nuvio::data::addons
{

namespace
{
std::string resourceKey(const std::string& name)
{
    return name == "subtitle" ? "subtitles" : name;
}

bool prefixesMatch(const std::vector<std::string>& prefixes, const std::string& id)
{
    return std::any_of(prefixes.begin(), prefixes.end(),
        [&id](const std::string& prefix) { return id.compare(0, prefix.size(), prefix) == 0; });
}
} // namespace

bool supportsResource(const domain::Addon& addon, const std::string& resource, const std::string& type,
    const std::string& id)
{
    const std::string wanted = resourceKey(resource);
    for (const auto& r : addon.resources)
    {
        if (resourceKey(r.name) != wanted)
            continue;
        if (!r.types.empty() && std::find(r.types.begin(), r.types.end(), type) == r.types.end())
            continue;
        if (wanted == "catalog")
            return true;
        const auto& prefixes = !r.idPrefixes.empty() ? r.idPrefixes : addon.idPrefixes;
        if (prefixes.empty() || prefixesMatch(prefixes, id))
            return true;
    }
    return false;
}

std::vector<domain::Addon> addonsFor(const std::vector<domain::Addon>& addons, const std::string& resource,
    const std::string& type, const std::string& id)
{
    std::vector<domain::Addon> out;
    for (const auto& addon : addons)
        if (addon.enabled && supportsResource(addon, resource, type, id))
            out.push_back(addon);
    return out;
}

} // namespace nuvio::data::addons
