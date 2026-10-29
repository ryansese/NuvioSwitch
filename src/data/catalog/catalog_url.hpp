#pragma once

#include <string>
#include <utility>
#include <vector>

// Catalog request URLs (NuvioTV CatalogRepositoryImpl.buildCatalogUrl).
namespace nuvio::data::catalog
{

// Extra arguments in the order they go into the URL.
using Extras = std::vector<std::pair<std::string, std::string>>;

// {basePath}/catalog/{type}/{id}.json, .../{id}/skip={n}.json, or
// .../{id}/{k=v&...}.json with "skip" appended last when skip > 0 and not given;
// the base URL's ?query goes after ".json". type and id are inserted as they are;
// extra keys and values are URL-encoded with "%20" for spaces.
std::string buildCatalogUrl(const std::string& baseUrl, const std::string& type, const std::string& catalogId,
    int skip, const Extras& extras = {});

// Java URLEncoder.encode(value, "UTF-8") with "+" turned into "%20".
std::string encodeArg(const std::string& value);

} // namespace nuvio::data::catalog
