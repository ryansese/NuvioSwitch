#include "data/addons/manifest_parser.hpp"

#include <algorithm>
#include <cctype>

#include "core/json/json.hpp"

namespace nuvio::data::addons
{

using core::json::Json;
using namespace nuvio::domain;
namespace json = core::json;

namespace
{
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

std::string trimLower(const std::string& s)
{
    std::string out = trim(s);
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return std::tolower(c); });
    return out;
}

// The string entries of the array at key, untrimmed; anything else gives nullopt.
std::optional<std::vector<std::string>> stringArray(const Json& j, const char* key)
{
    auto it = j.find(key);
    if (it == j.end() || !it->is_array())
        return std::nullopt;
    std::vector<std::string> out;
    for (const auto& entry : *it)
        if (entry.is_string())
            out.push_back(entry.get<std::string>());
    return out;
}

std::vector<AddonResource> parseResources(const Json& manifest, const std::vector<std::string>& manifestTypes)
{
    std::vector<AddonResource> out;
    auto it = manifest.find("resources");
    if (it == manifest.end() || !it->is_array())
        return out;
    for (const auto& entry : *it)
    {
        if (entry.is_string())
        {
            out.push_back({entry.get<std::string>(), manifestTypes, {}});
        }
        else if (entry.is_object())
        {
            auto name = json::optStr(entry, "name");
            if (!name)
                continue;
            AddonResource resource;
            resource.name       = *name;
            resource.types      = stringArray(entry, "types").value_or(manifestTypes);
            resource.idPrefixes = stringArray(entry, "idPrefixes").value_or(std::vector<std::string>{});
            out.push_back(std::move(resource));
        }
    }
    return out;
}

std::optional<CatalogExtra> parseExtra(const Json& raw)
{
    CatalogExtra extra;
    if (raw.is_string())
    {
        extra.name = trimLower(raw.get<std::string>());
    }
    else if (raw.is_object())
    {
        extra.name       = trimLower(json::str(raw, "name"));
        extra.isRequired = json::optBool(raw, "isRequired").value_or(false);

        auto options = raw.find("options");
        if (options != raw.end() && options->is_array())
        {
            std::vector<std::string> values;
            for (const auto& option : *options)
            {
                if (option.is_string())
                    values.push_back(option.get<std::string>());
                else if (!option.is_null())
                    values.push_back(option.dump());
            }
            if (!values.empty())
                extra.options = std::move(values);
        }

        auto defaultValue = json::optStr(raw, "default");
        if (defaultValue && !trim(*defaultValue).empty())
            extra.defaultValue = defaultValue;

        if (auto limit = json::optInt(raw, "optionsLimit"))
            extra.optionsLimit = static_cast<int>(*limit);
    }
    if (extra.name.empty())
        return std::nullopt;
    return extra;
}

std::optional<CatalogDescriptor> parseCatalog(const Json& raw)
{
    if (!raw.is_object())
        return std::nullopt;
    auto type = json::optStr(raw, "type");
    auto id   = json::optStr(raw, "id");
    auto name = json::optStr(raw, "name");
    if (!type || !id || !name)
        return std::nullopt;

    CatalogDescriptor catalog;
    // trimmed but not lowercased: the type goes back into catalog URLs as written
    catalog.type = trim(*type);
    catalog.id   = *id;
    catalog.name = *name;

    auto extras = raw.find("extra");
    if (extras != raw.end() && extras->is_array())
        for (const auto& entry : *extras)
            if (auto extra = parseExtra(entry))
                if (std::find(catalog.extra.begin(), catalog.extra.end(), *extra) == catalog.extra.end())
                    catalog.extra.push_back(std::move(*extra));

    if (auto pageSize = json::optInt(raw, "pageSize"))
        catalog.pageSize = static_cast<int>(*pageSize);

    if (auto showInHome = json::optBool(raw, "showInHome"))
    {
        catalog.showInHome            = *showInHome;
        catalog.hasExplicitShowInHome = true;
    }

    catalog.extraSupported = stringArray(raw, "extraSupported").value_or(std::vector<std::string>{});
    catalog.extraRequired  = stringArray(raw, "extraRequired").value_or(std::vector<std::string>{});
    return catalog;
}
} // namespace

std::optional<Addon> parseManifest(const Json& manifest, const std::string& baseUrl)
{
    if (!manifest.is_object())
        return std::nullopt;
    auto id      = json::optStr(manifest, "id");
    auto name    = json::optStr(manifest, "name");
    auto version = json::optStr(manifest, "version");
    if (!id || !name || !version)
        return std::nullopt;

    Addon addon;
    addon.id          = *id;
    addon.name        = *name;
    addon.displayName = *name;
    addon.version     = *version;
    addon.description = json::optStr(manifest, "description");
    addon.logo        = json::optStr(manifest, "logo");
    addon.background  = json::optStr(manifest, "background");
    addon.baseUrl     = baseUrl;
    addon.types       = json::strList(manifest, "types");
    addon.resources   = parseResources(manifest, addon.types);
    addon.idPrefixes  = stringArray(manifest, "idPrefixes").value_or(std::vector<std::string>{});

    auto catalogs = manifest.find("catalogs");
    if (catalogs != manifest.end() && catalogs->is_array())
        for (const auto& entry : *catalogs)
            if (auto catalog = parseCatalog(entry))
                addon.catalogs.push_back(std::move(*catalog));

    auto hints = manifest.find("behaviorHints");
    if (hints != manifest.end() && hints->is_object())
        addon.behaviorHints = AddonBehaviorHints{json::optBool(*hints, "configurable"),
            json::optBool(*hints, "configurationRequired"), json::optBool(*hints, "newEpisodeNotifications")};

    return addon;
}

std::optional<Addon> parseManifest(const std::string& body, const std::string& baseUrl)
{
    Json manifest = Json::parse(body, nullptr, false);
    if (manifest.is_discarded())
        return std::nullopt;
    return parseManifest(manifest, baseUrl);
}

} // namespace nuvio::data::addons
