#pragma once

#include <string>
#include <vector>

#include "data/catalog/catalog_url.hpp"
#include "domain/addon.hpp"
#include "domain/catalog_row.hpp"

// Which catalogs a search asks (NuvioTV ui/screens/search/SearchViewModel.kt, isSearchable
// and buildSearchTargets).
namespace nuvio::data::search
{

// The catalog supports the "search" extra and declares no other required extra.
// Search itself may be required (search-only catalogs qualify).
bool isSearchable(const domain::CatalogDescriptor& catalog);

// Every searchable catalog of the enabled add-ons, by add-on order then manifest order,
// each "<addonId>_<type>_<catalogId>" only once.
std::vector<domain::CatalogSource> searchTargets(const std::vector<domain::Addon>& addons);

// The extra arguments for a search request: only search=<query>.
catalog::Extras searchExtras(const std::string& query);

} // namespace nuvio::data::search
