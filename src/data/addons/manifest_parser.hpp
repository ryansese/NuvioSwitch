#pragma once

#include <optional>
#include <string>

#include <nlohmann/json.hpp>

#include "domain/addon.hpp"

// Stremio manifest JSON to Addon, accepting what NuvioTV accepts
// (data/remote/dto/AddonManifestDto.kt, data/mapper/AddonMapper.kt).
namespace nuvio::data::addons
{

// The add-on described by manifest, installed at baseUrl (already canonical).
// Malformed resources, catalogs and extras are skipped; nullopt when id, name
// or version is missing or not a string.
std::optional<domain::Addon> parseManifest(const nlohmann::json& manifest, const std::string& baseUrl);

// Same, from the response body; nullopt when it isn't JSON.
std::optional<domain::Addon> parseManifest(const std::string& body, const std::string& baseUrl);

} // namespace nuvio::data::addons
