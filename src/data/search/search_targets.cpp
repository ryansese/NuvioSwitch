#include "data/search/search_targets.hpp"

#include <strings.h>
#include <unordered_set>

namespace nuvio::data::search
{

using namespace nuvio::domain;

namespace
{
bool isSearch(const std::string& name)
{
    return strcasecmp(name.c_str(), "search") == 0;
}
} // namespace

bool isSearchable(const CatalogDescriptor& catalog)
{
    if (!catalog.supportsExtra("search"))
        return false;
    for (const auto& extra : catalog.extra)
        if (extra.isRequired && !isSearch(extra.name))
            return false;
    for (const auto& name : catalog.extraRequired)
        if (!isSearch(name))
            return false;
    return true;
}

std::vector<CatalogSource> searchTargets(const std::vector<Addon>& addons)
{
    std::vector<CatalogSource> out;
    std::unordered_set<std::string> keys;
    for (const auto& addon : addons)
    {
        if (!addon.enabled)
            continue;
        for (const auto& catalog : addon.catalogs)
        {
            if (!isSearchable(catalog))
                continue;
            CatalogSource source{addon.id, addon.baseUrl, addon.displayName, catalog};
            if (keys.insert(source.key()).second)
                out.push_back(std::move(source));
        }
    }
    return out;
}

catalog::Extras searchExtras(const std::string& query)
{
    return {{"search", query}};
}

} // namespace nuvio::data::search
