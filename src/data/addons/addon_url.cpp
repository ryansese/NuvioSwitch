#include "data/addons/addon_url.hpp"

#include <algorithm>
#include <cctype>
#include <strings.h>

namespace nuvio::data::addons
{

namespace
{
const std::string MANIFEST_SUFFIX = "/manifest.json";

std::string trim(const std::string& s)
{
    size_t begin = 0;
    size_t end   = s.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(s[begin])))
        begin++;
    while (end > begin && std::isspace(static_cast<unsigned char>(s[end - 1])))
        end--;
    return s.substr(begin, end - begin);
}

std::string trimTrailingSlashes(std::string s)
{
    while (!s.empty() && s.back() == '/')
        s.pop_back();
    return s;
}

bool endsWithIgnoreCase(const std::string& s, const std::string& suffix)
{
    return s.size() >= suffix.size() &&
           strncasecmp(s.c_str() + s.size() - suffix.size(), suffix.c_str(), suffix.size()) == 0;
}

bool startsWithIgnoreCase(const std::string& s, const std::string& prefix)
{
    return s.size() >= prefix.size() && strncasecmp(s.c_str(), prefix.c_str(), prefix.size()) == 0;
}

// Splits at the first '?': {path, "?query" or ""}.
std::pair<std::string, std::string> splitQuery(const std::string& url)
{
    size_t q = url.find('?');
    if (q == std::string::npos)
        return {url, ""};
    return {url.substr(0, q), url.substr(q)};
}
} // namespace

std::string canonicalize(const std::string& url)
{
    auto [path, query] = splitQuery(trimTrailingSlashes(trim(url)));
    if (endsWithIgnoreCase(path, MANIFEST_SUFFIX))
        path.resize(path.size() - MANIFEST_SUFFIX.size());
    return trimTrailingSlashes(path) + query;
}

std::string normalizeForCompare(const std::string& url)
{
    std::string s = canonicalize(url);
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

std::string manifestUrl(const std::string& url)
{
    auto [path, query] = splitQuery(canonicalize(url));
    return trimTrailingSlashes(path) + MANIFEST_SUFFIX + query;
}

std::optional<std::string> normalizeInstallInput(const std::string& input)
{
    std::string url = trim(input);
    if (startsWithIgnoreCase(url, "stremio://"))
        url = "https://" + url.substr(10);
    if (!startsWithIgnoreCase(url, "http://") && !startsWithIgnoreCase(url, "https://"))
        return std::nullopt;
    std::string canonical = canonicalize(url);
    // nothing after the scheme ("https://" canonicalises to "https:")
    size_t scheme = canonical.find("://");
    if (scheme == std::string::npos || canonical.size() <= scheme + 3)
        return std::nullopt;
    return canonical;
}

std::string hostOf(const std::string& url)
{
    size_t scheme = url.find("://");
    size_t start  = scheme == std::string::npos ? 0 : scheme + 3;
    size_t end    = url.find_first_of("/?#", start);
    return url.substr(0, end);
}

} // namespace nuvio::data::addons
