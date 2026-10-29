#pragma once

#include <string>
#include <vector>

#include "domain/addon.hpp"
#include "domain/catalog_row.hpp"
#include "domain/meta_preview.hpp"

// What Home shows, from NuvioTV ui/screens/home/HomeViewModelCatalogUtils.kt and
// HomeViewModelCatalogPipeline.kt.
namespace nuvio::data::catalog
{

constexpr size_t HERO_SLOTS = 7;

// A catalog that only answers searches: its "search" extra is required.
bool isSearchOnly(const domain::CatalogDescriptor& catalog);

// Not search-only, and showInHome isn't explicitly false.
bool shouldShowOnHome(const domain::CatalogDescriptor& catalog);

// Every Home catalog of the enabled add-ons, by add-on priority then manifest
// order, each "<addonId>_<type>_<catalogId>" only once.
std::vector<domain::CatalogSource> selectHomeCatalogs(const std::vector<domain::Addon>& addons);

// "<Catalog name, first letter uppercased> - <type label>": movieLabel for "movie",
// seriesLabel for "series", otherwise the type with its first letter uppercased.
std::string rowTitle(const domain::CatalogDescriptor& catalog, const std::string& movieLabel = "Movie",
    const std::string& seriesLabel = "Series");

// Up to count hero items from the loaded rows, in row order. The slots are spread
// evenly with earlier rows taking the remainder; in each row items with a
// background come first, and an item already picked isn't picked again.
std::vector<domain::MetaPreview> pickHeroItems(const std::vector<std::vector<domain::MetaPreview>>& rows,
    size_t count = HERO_SLOTS);

} // namespace nuvio::data::catalog
