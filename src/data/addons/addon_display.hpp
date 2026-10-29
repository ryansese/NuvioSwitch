#pragma once

#include <map>
#include <string>
#include <vector>

#include "domain/addon.hpp"

namespace nuvio::data::addons
{

// Sets displayName on each add-on, in list order (NuvioTV AddonRepositoryImpl.applyDisplayNames):
// the custom name from userNames (keyed by canonical URL) when set and different
// from the manifest name; otherwise the manifest name, with the 2nd, 3rd... add-on
// sharing a name among those without a custom one shown as "Name (2)", "Name (3)".
void applyDisplayNames(std::vector<domain::Addon>& addons, const std::map<std::string, std::string>& userNames);

} // namespace nuvio::data::addons
