#pragma once

#include <optional>
#include <string>

#include <nlohmann/json.hpp>

#include "domain/meta.hpp"

// Meta responses (NuvioTV data/remote/dto/MetaResponseDto.kt, MetadataFieldMappers).
namespace nuvio::data::meta
{

// The "meta" object of a response, or nullopt when the body isn't JSON, has no
// object under "meta", or that object has no id or name. type falls back to
// requestedType; addonBaseUrl is stored on the result.
std::optional<domain::Meta> parseMeta(const nlohmann::json& response, const std::string& requestedType,
    const std::string& addonBaseUrl);
std::optional<domain::Meta> parseMeta(const std::string& body, const std::string& requestedType,
    const std::string& addonBaseUrl);

} // namespace nuvio::data::meta
