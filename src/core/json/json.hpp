#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

// Tolerant getters for the loosely typed JSON that Stremio add-ons return:
// numbers sent as strings, lists sent as a single string, fields set to null.
// None of them throw; a missing or unusable field gives the fallback or nullopt.
namespace nuvio::core::json
{

using Json = nlohmann::json;

// The string at key, or fallback when it is missing or not a string.
std::string str(const Json& j, const char* key, const std::string& fallback = "");

// The string at key, or nullopt when it is missing, null or not a string.
std::optional<std::string> optStr(const Json& j, const char* key);

// A list of strings from a string, an array, or an object with a "name".
// Array entries may be strings or objects with a "name"; other entries are dropped.
// Values are trimmed and blanks dropped (NuvioTV MetadataFieldMappers.coerceStringList).
std::vector<std::string> strList(const Json& j, const char* key);

// An integer from a number or a numeric string (NuvioTV AddonMapper optionsLimit).
std::optional<int64_t> optInt(const Json& j, const char* key);

// A bool from a bool, "true"/"false" (any case), or a number, non-zero meaning true
// (NuvioTV AddonMapper isRequired).
std::optional<bool> optBool(const Json& j, const char* key);

} // namespace nuvio::core::json
