#include "data/meta/meta_candidates.hpp"

#include <algorithm>
#include <cctype>

#include "data/catalog/catalog_url.hpp"

namespace nuvio::data::meta
{

using domain::Addon;
using domain::AddonResource;

namespace
{
std::string lower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

bool equalsIgnoreCase(const std::string& a, const std::string& b)
{
    return lower(a) == lower(b);
}

bool startsWithIgnoreCase(const std::string& s, const std::string& prefix)
{
    return s.size() >= prefix.size() && lower(s.substr(0, prefix.size())) == lower(prefix);
}

std::string trimTrailingSlashes(std::string s)
{
    while (!s.empty() && s.back() == '/')
        s.pop_back();
    return s;
}

std::string trimmed(const std::string& s)
{
    size_t begin = 0;
    size_t end   = s.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(s[begin])))
        begin++;
    while (end > begin && std::isspace(static_cast<unsigned char>(s[end - 1])))
        end--;
    return s.substr(begin, end - begin);
}

// The "meta" resources of addon.
std::vector<const AddonResource*> metaResources(const Addon& addon)
{
    std::vector<const AddonResource*> out;
    for (const auto& r : addon.resources)
        if (equalsIgnoreCase(trimmed(r.name), "meta"))
            out.push_back(&r);
    return out;
}

bool typeListed(const std::vector<std::string>& types, const std::string& type)
{
    return std::any_of(types.begin(), types.end(), [&type](const std::string& t) { return equalsIgnoreCase(trimmed(t), type); });
}

// ID prefixes come from the resource when it declares any, else the add-on.
bool idMatches(const Addon& addon, const AddonResource& resource, const std::string& id)
{
    const auto& prefixes = !resource.idPrefixes.empty() ? resource.idPrefixes : addon.idPrefixes;
    if (prefixes.empty())
        return true;
    return std::any_of(prefixes.begin(), prefixes.end(),
        [&id](const std::string& prefix) { return startsWithIgnoreCase(id, trimmed(prefix)); });
}

bool hasPrefixes(const Addon& addon, const AddonResource& resource)
{
    return !(!resource.idPrefixes.empty() ? resource.idPrefixes : addon.idPrefixes).empty();
}

// A meta resource that lists type (or lists none, meaning every type) with matching prefixes.
bool supportsTypeAndId(const Addon& addon, const std::string& type, const std::string& id)
{
    for (const auto* r : metaResources(addon))
        if ((r->types.empty() || typeListed(r->types, type)) && idMatches(addon, *r, id))
            return true;
    return false;
}

// A meta resource for type that declares no ID prefixes at all.
bool supportsTypeWithoutPrefixes(const Addon& addon, const std::string& type)
{
    for (const auto* r : metaResources(addon))
        if ((r->types.empty() || typeListed(r->types, type)) && !hasPrefixes(addon, *r))
            return true;
    return false;
}

bool hasMetaWithoutPrefixes(const Addon& addon)
{
    for (const auto* r : metaResources(addon))
        if (!hasPrefixes(addon, *r))
            return true;
    return false;
}

// The type the add-on is asked for in step 3: the requested one when its meta resource
// lists it, else the inferred one.
std::string typeFor(const Addon& addon, const std::string& type, const std::string& inferred)
{
    for (const auto* r : metaResources(addon))
        if (r->types.empty() || typeListed(r->types, type))
            return type;
    return inferred;
}

void add(std::vector<MetaCandidate>& out, const Addon& addon, const std::string& type)
{
    MetaCandidate candidate{addon, type};
    if (std::find(out.begin(), out.end(), candidate) == out.end())
        out.push_back(std::move(candidate));
}
} // namespace

std::string inferType(const std::string& type, const std::string& id)
{
    std::string t = lower(trimmed(type));
    if (t == "tv")
        return "series";
    if (t == "movie" || t == "series" || t == "channel" || t == "anime")
        return t;

    std::string lowerId = lower(id);
    if (lowerId.find(":movie:") != std::string::npos)
        return "movie";
    if (lowerId.find(":series:") != std::string::npos || lowerId.find(":tv:") != std::string::npos)
        return "series";
    if (lowerId.find(":anime:") != std::string::npos)
        return "anime";
    return type;
}

std::vector<MetaCandidate> candidates(const std::vector<Addon>& addons, const std::string& type, const std::string& id)
{
    std::vector<Addon> metaAddons;
    for (const auto& addon : addons)
        if (addon.enabled && !metaResources(addon).empty())
            metaAddons.push_back(addon);

    std::string inferred = inferType(type, id);
    std::vector<MetaCandidate> out;

    // 1. the requested type with matching prefixes
    for (const auto& addon : metaAddons)
        if (supportsTypeAndId(addon, type, id))
            add(out, addon, type);

    // 2. the same for the inferred type
    if (!equalsIgnoreCase(inferred, type))
        for (const auto& addon : metaAddons)
            if (supportsTypeAndId(addon, inferred, id))
                add(out, addon, inferred);

    // 3. the first add-on whose prefixes match, whichever of the two types it supports
    for (const auto& addon : metaAddons)
    {
        if (supportsTypeAndId(addon, type, id) || supportsTypeAndId(addon, inferred, id))
        {
            add(out, addon, typeFor(addon, type, inferred));
            break;
        }
    }

    // 4. add-ons without prefixes
    if (out.empty())
    {
        for (const auto& addon : metaAddons)
            if (supportsTypeWithoutPrefixes(addon, type))
                add(out, addon, type);
        for (const auto& addon : metaAddons)
            if (hasMetaWithoutPrefixes(addon))
            {
                add(out, addon, typeFor(addon, type, inferred));
                break;
            }
    }

    // 5. any add-on whose manifest lists the type
    if (out.empty())
        for (const auto& addon : metaAddons)
            if (typeListed(addon.types, type))
                add(out, addon, type);

    return out;
}

std::string buildMetaUrl(const std::string& baseUrl, const std::string& type, const std::string& id)
{
    std::string base  = trimTrailingSlashes(baseUrl);
    size_t queryStart = base.find('?');
    std::string basePath =
        queryStart == std::string::npos ? base : trimTrailingSlashes(base.substr(0, queryStart));
    std::string baseQuery = queryStart == std::string::npos ? "" : base.substr(queryStart);
    return basePath + "/meta/" + catalog::encodeArg(type) + "/" + catalog::encodeArg(id) + ".json" + baseQuery;
}

} // namespace nuvio::data::meta
