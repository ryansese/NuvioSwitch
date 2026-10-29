#pragma once

#include <string>

#include "core/net/http_types.hpp"

namespace nuvio::core::net
{

// "<locale>,en;q=0.7", or just the locale when it is English, or "en" when it is
// unknown. Same rules as buildAcceptLanguageHeader in NuvioTV's core/di/NetworkModule.kt.
std::string buildAcceptLanguage(const std::string& locale);

// "NuvioSwitch/<version>".
std::string buildUserAgent(const std::string& version);

// User-Agent and Accept-Language for every request.
Headers defaultHeaders(const std::string& version, const std::string& locale);

// defaults with overrides applied on top; same-named headers are replaced.
Headers mergeHeaders(const Headers& defaults, const Headers& overrides);

} // namespace nuvio::core::net
