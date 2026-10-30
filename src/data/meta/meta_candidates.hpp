#pragma once

#include <string>
#include <vector>

#include "domain/addon.hpp"

// Which add-ons are asked for a title's metadata, and how
// (NuvioTV MetaRepositoryImpl.getMetaFromAllAddons, supportsMetaType, supportsMetaId).
namespace nuvio::data::meta
{

struct MetaCandidate
{
    domain::Addon addon;
    std::string type; // the type to put in the request URL

    bool operator==(const MetaCandidate& other) const { return addon.baseUrl == other.addon.baseUrl && type == other.type; }
};

// NuvioTV inferCanonicalType: "tv" is "series". A type other than movie, series,
// channel or anime is read from a ":movie:", ":series:", ":tv:" or ":anime:" marker
// in id (":tv:" gives "series"); with no marker the type is returned as given.
std::string inferType(const std::string& type, const std::string& id);

// The ordered, de-duplicated (add-on, type) pairs to try, from the enabled add-ons
// with a "meta" resource. Matching is case-insensitive. The five steps are in the
// change's spec (details: "Meta add-on candidate order").
std::vector<MetaCandidate> candidates(const std::vector<domain::Addon>& addons, const std::string& type,
    const std::string& id);

// {basePath}/meta/{type}/{id}.json{baseQuery}; type and id are encoded like Java's
// URLEncoder with "+" as "%20" (NuvioTV encodePathSegment).
std::string buildMetaUrl(const std::string& baseUrl, const std::string& type, const std::string& id);

} // namespace nuvio::data::meta
