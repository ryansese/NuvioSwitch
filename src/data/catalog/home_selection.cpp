#include "data/catalog/home_selection.hpp"

#include <algorithm>
#include <cctype>
#include <unordered_set>

namespace nuvio::data::catalog
{

using namespace nuvio::domain;

namespace
{
std::string capitalized(std::string s)
{
    if (!s.empty())
        s[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(s[0])));
    return s;
}

std::string lowercase(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}
} // namespace

bool isSearchOnly(const CatalogDescriptor& catalog)
{
    // only the long form, as NuvioTV checks it
    for (const auto& extra : catalog.extra)
        if (extra.name == "search" && extra.isRequired)
            return true;
    return false;
}

bool shouldShowOnHome(const CatalogDescriptor& catalog)
{
    if (isSearchOnly(catalog))
        return false;
    return !catalog.hasExplicitShowInHome || catalog.showInHome;
}

std::vector<CatalogSource> selectHomeCatalogs(const std::vector<Addon>& addons)
{
    std::vector<CatalogSource> out;
    std::unordered_set<std::string> keys;
    for (const auto& addon : addons)
    {
        if (!addon.enabled)
            continue;
        for (const auto& catalog : addon.catalogs)
        {
            if (!shouldShowOnHome(catalog))
                continue;
            CatalogSource source{addon.id, addon.baseUrl, addon.displayName, catalog};
            if (keys.insert(source.key()).second)
                out.push_back(std::move(source));
        }
    }
    return out;
}

std::string rowTitle(const CatalogDescriptor& catalog, const std::string& movieLabel, const std::string& seriesLabel)
{
    std::string type = lowercase(catalog.type);
    std::string label;
    if (type == "movie" && !movieLabel.empty())
        label = movieLabel;
    else if (type == "series" && !seriesLabel.empty())
        label = seriesLabel;
    else
        label = capitalized(catalog.type);
    return capitalized(catalog.name) + " - " + label;
}

std::vector<MetaPreview> pickHeroItems(const std::vector<std::vector<MetaPreview>>& rows, size_t count)
{
    std::vector<MetaPreview> out;
    if (rows.empty())
        return out;

    size_t base      = count / rows.size();
    size_t remainder = count % rows.size();
    std::unordered_set<std::string> seen;

    for (size_t r = 0; r < rows.size(); r++)
    {
        size_t slot  = base + (r < remainder ? 1 : 0);
        size_t taken = 0;
        // artwork first, then the rest to fill what's left of the slot
        for (bool artwork : {true, false})
            for (const auto& item : rows[r])
            {
                if (taken == slot)
                    break;
                if (item.hasHeroArtwork() == artwork && seen.insert(item.id).second)
                {
                    out.push_back(item);
                    taken++;
                }
            }
    }
    return out;
}

} // namespace nuvio::data::catalog
