#pragma once

#include <optional>
#include <string>

namespace nuvio::core::net
{

struct CacheControl
{
    bool noStore = false;
    bool noCache = false;
    bool isPrivate = false;
    std::optional<long> maxAge;  // seconds
    std::optional<long> sMaxAge; // seconds

    // s-maxage wins over max-age; 0 when neither is set.
    long freshnessSeconds() const;
};

// Parses a Cache-Control header value. Unknown directives are ignored.
CacheControl parseCacheControl(const std::string& value);

// Whether a response may go in the disk cache: GET, 2xx, and no
// no-store / no-cache / private. (NuvioTV rewrites 4xx/5xx to no-store instead.)
bool isCacheable(const std::string& method, long status, const CacheControl& cc);

} // namespace nuvio::core::net
