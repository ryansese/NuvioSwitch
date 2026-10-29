#include "core/net/headers.hpp"

#include <cctype>

namespace nuvio::core::net
{

namespace
{
std::string lower(std::string s)
{
    for (auto& c : s)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}
} // namespace

bool CaseInsensitiveLess::operator()(const std::string& a, const std::string& b) const
{
    return lower(a) < lower(b);
}

std::string buildAcceptLanguage(const std::string& locale)
{
    std::string primary = locale;
    if (primary.empty() || lower(primary) == "und")
        primary = "en";

    std::string l = lower(primary);
    if (l == "en" || l.rfind("en-", 0) == 0)
        return primary;
    return primary + ",en;q=0.7";
}

std::string buildUserAgent(const std::string& version)
{
    return "NuvioSwitch/" + version;
}

Headers defaultHeaders(const std::string& version, const std::string& locale)
{
    return Headers{
        {"User-Agent", buildUserAgent(version)},
        {"Accept-Language", buildAcceptLanguage(locale)},
    };
}

Headers mergeHeaders(const Headers& defaults, const Headers& overrides)
{
    Headers merged = defaults;
    for (const auto& [name, value] : overrides)
    {
        merged.erase(name); // keep the caller's spelling of the name
        merged.emplace(name, value);
    }
    return merged;
}

} // namespace nuvio::core::net
