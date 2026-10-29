#pragma once

#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "domain/meta_preview.hpp"

// Catalog responses to MetaPreview (NuvioTV data/mapper/CatalogMapper.kt).
namespace nuvio::data::catalog
{

struct ParsedCatalog
{
    std::vector<domain::MetaPreview> items;
    size_t rawCount = 0; // entries in "metas", before anything is dropped
};

// The items of a catalog response. Entries without a non-blank id or name are
// dropped, a missing type falls back to catalogType, and only the first of each
// id is kept. nullopt when the body isn't a JSON object.
std::optional<ParsedCatalog> parseCatalog(const std::string& body, const std::string& catalogType,
    const std::string& sourceAddonBaseUrl);

std::optional<ParsedCatalog> parseCatalog(const nlohmann::json& response, const std::string& catalogType,
    const std::string& sourceAddonBaseUrl);

} // namespace nuvio::data::catalog
