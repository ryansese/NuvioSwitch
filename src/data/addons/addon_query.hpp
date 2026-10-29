#pragma once

#include <string>
#include <vector>

#include "domain/addon.hpp"

// Which add-ons can serve what (NuvioTV core/streams/PlaybackAvailability.kt).
namespace nuvio::data::addons
{

// Whether addon has a resource named resource ("subtitle" and "subtitles" are the
// same) for type, whose id prefixes match id. Resource-level prefixes win over
// manifest-level ones; with neither, every id matches. Catalog queries ignore id.
// The enabled flag is not checked here.
bool supportsResource(const domain::Addon& addon, const std::string& resource, const std::string& type,
    const std::string& id);

// The enabled add-ons that support the resource, in priority (list) order.
std::vector<domain::Addon> addonsFor(const std::vector<domain::Addon>& addons, const std::string& resource,
    const std::string& type, const std::string& id);

} // namespace nuvio::data::addons
