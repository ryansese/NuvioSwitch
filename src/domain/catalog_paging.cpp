#include "domain/catalog_paging.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <optional>
#include <vector>

namespace nuvio::domain
{

namespace
{
// Kotlin's trim().toIntOrNull(): the whole trimmed string must be an integer.
std::optional<int> toInt(const std::string& s)
{
    size_t begin = s.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos)
        return std::nullopt;
    size_t end        = s.find_last_not_of(" \t\r\n") + 1;
    std::string value = s.substr(begin, end - begin);
    char* parsed      = nullptr;
    errno             = 0;
    long n            = std::strtol(value.c_str(), &parsed, 10);
    if (errno != 0 || parsed != value.c_str() + value.size() || n < INT32_MIN || n > INT32_MAX)
        return std::nullopt;
    return static_cast<int>(n);
}
} // namespace

int skipStep(const CatalogDescriptor& catalog)
{
    if (catalog.pageSize && *catalog.pageSize > 0)
        return *catalog.pageSize;

    for (const auto& extra : catalog.extra)
    {
        if (extra.name != "skip")
            continue;
        std::vector<int> options;
        for (const auto& option : extra.options.value_or(std::vector<std::string>{}))
            if (auto n = toInt(option); n && *n >= 0)
                options.push_back(*n);
        std::sort(options.begin(), options.end());
        options.erase(std::unique(options.begin(), options.end()), options.end());

        std::optional<int> step;
        for (size_t i = 1; i < options.size(); i++)
            if (int gap = options[i] - options[i - 1]; gap > 0 && (!step || gap < *step))
                step = gap;
        if (step)
            return *step;
        break; // NuvioTV only looks at the first "skip" extra
    }
    return DEFAULT_SKIP_STEP;
}

NextPage nextPage(const CatalogDescriptor& catalog, int skip, size_t rawCount)
{
    bool more = catalog.supportsExtra("skip") && rawCount > 0;
    return {more ? skip + static_cast<int>(rawCount) : skip, more};
}

} // namespace nuvio::domain
