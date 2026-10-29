#include "core/net/cache_control.hpp"

#include <cctype>
#include <cerrno>
#include <cstdlib>

namespace nuvio::core::net
{

namespace
{
std::string trimLower(const std::string& s)
{
    size_t begin = 0;
    size_t end   = s.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(s[begin])))
        begin++;
    while (end > begin && std::isspace(static_cast<unsigned char>(s[end - 1])))
        end--;
    std::string out = s.substr(begin, end - begin);
    for (auto& c : out)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

std::optional<long> parseSeconds(std::string value)
{
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
        value = value.substr(1, value.size() - 2);
    if (value.empty() || !std::isdigit(static_cast<unsigned char>(value[0])))
        return std::nullopt;
    char* end = nullptr;
    errno     = 0;
    long seconds = std::strtol(value.c_str(), &end, 10);
    if (errno != 0 || *end != '\0')
        return std::nullopt;
    return seconds;
}
} // namespace

long CacheControl::freshnessSeconds() const
{
    if (sMaxAge)
        return *sMaxAge;
    return maxAge.value_or(0);
}

CacheControl parseCacheControl(const std::string& value)
{
    CacheControl cc;
    size_t start = 0;
    while (start <= value.size())
    {
        size_t comma = value.find(',', start);
        if (comma == std::string::npos)
            comma = value.size();
        std::string directive = value.substr(start, comma - start);
        start                 = comma + 1;

        std::string name = directive;
        std::string arg;
        if (size_t eq = directive.find('='); eq != std::string::npos)
        {
            name = directive.substr(0, eq);
            arg  = directive.substr(eq + 1);
        }
        name = trimLower(name);
        arg  = trimLower(arg);

        if (name == "no-store")
            cc.noStore = true;
        else if (name == "no-cache")
            cc.noCache = true;
        else if (name == "private")
            cc.isPrivate = true;
        else if (name == "max-age")
            cc.maxAge = parseSeconds(arg);
        else if (name == "s-maxage")
            cc.sMaxAge = parseSeconds(arg);
    }
    return cc;
}

bool isCacheable(const std::string& method, long status, const CacheControl& cc)
{
    if (method != "GET" || status < 200 || status > 299)
        return false;
    return !cc.noStore && !cc.noCache && !cc.isPrivate;
}

} // namespace nuvio::core::net
