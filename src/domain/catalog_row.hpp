#pragma once

#include <string>
#include <vector>

#include "domain/addon.hpp"
#include "domain/meta_preview.hpp"

// One add-on catalog as Home and See all show it (NuvioTV domain/model/CatalogRow.kt).
namespace nuvio::domain
{

// Which catalog of which add-on.
struct CatalogSource
{
    std::string addonId;
    std::string addonBaseUrl;
    std::string addonName; // display name
    CatalogDescriptor catalog;

    // "<addonId>_<type>_<catalogId>", NuvioTV's catalogRowLegacyKey.
    std::string key() const { return addonId + "_" + catalog.type + "_" + catalog.id; }
};

// One page of a catalog.
struct CatalogPage
{
    std::vector<MetaPreview> items;
    size_t rawCount  = 0; // metas in the response before filtering, which drives paging
    int nextSkip     = 0;
    bool hasMore     = false;
};

struct CatalogRow
{
    CatalogSource source;
    std::vector<MetaPreview> items;
    int nextSkip = 0;
    bool hasMore = false;
};

} // namespace nuvio::domain
