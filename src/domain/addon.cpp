#include "domain/addon.hpp"

#include <strings.h>

namespace nuvio::domain
{

namespace
{
bool equalsIgnoreCase(const std::string& a, const std::string& b)
{
    return a.size() == b.size() && strcasecmp(a.c_str(), b.c_str()) == 0;
}

bool containsIgnoreCase(const std::vector<std::string>& values, const std::string& name)
{
    for (const auto& value : values)
        if (equalsIgnoreCase(value, name))
            return true;
    return false;
}
} // namespace

bool CatalogDescriptor::supportsExtra(const std::string& name) const
{
    for (const auto& e : extra)
        if (equalsIgnoreCase(e.name, name))
            return true;
    return containsIgnoreCase(extraSupported, name) || containsIgnoreCase(extraRequired, name);
}

} // namespace nuvio::domain
