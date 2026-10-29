#pragma once

#include <optional>
#include <string>

// Add-on URL rules, as in NuvioTV AddonRepositoryImpl.canonicalizeUrl and
// AddonManagerViewModel.normalizeAddonUrl.
namespace nuvio::data::addons
{

// The add-on's identity: trimmed, without trailing '/' and without a trailing
// "/manifest.json" (any case) on the path; a "?query" is kept unchanged.
std::string canonicalize(const std::string& url);

// canonicalize() lowercased, for duplicate checks.
std::string normalizeForCompare(const std::string& url);

// Where the manifest lives: "<path>/manifest.json<?query>".
std::string manifestUrl(const std::string& url);

// The canonical URL for text typed in the install dialog, or nullopt when it
// doesn't start with http:// or https://. "stremio://" becomes "https://".
std::optional<std::string> normalizeInstallInput(const std::string& input);

// "scheme://host" of url, for log lines: configured add-ons keep tokens in the path and query.
std::string hostOf(const std::string& url);

} // namespace nuvio::data::addons
