#pragma once

#include <optional>
#include <string>
#include <vector>

// Installed Stremio add-ons and what their manifests declare
// (NuvioTV domain/model/Addon.kt).
namespace nuvio::domain
{

struct CatalogExtra
{
    std::string name; // trimmed, lowercase
    bool isRequired = false;
    std::optional<std::vector<std::string>> options; // never an empty list
    std::optional<std::string> defaultValue;
    std::optional<int> optionsLimit;

    bool operator==(const CatalogExtra& other) const
    {
        return name == other.name && isRequired == other.isRequired && options == other.options &&
               defaultValue == other.defaultValue && optionsLimit == other.optionsLimit;
    }
};

struct CatalogDescriptor
{
    std::string type; // as the manifest spells it, trimmed ("movie", "series", "anime", ...)
    std::string id;
    std::string name;
    std::vector<CatalogExtra> extra;
    std::optional<int> pageSize;
    bool showInHome            = false;
    bool hasExplicitShowInHome = false; // showInHome was present in the manifest
    std::vector<std::string> extraSupported;
    std::vector<std::string> extraRequired;

    // Whether the catalog takes extra `name` in either the long or the short form
    // (NuvioTV CatalogDescriptorExtensions.kt).
    bool supportsExtra(const std::string& name) const;
};

struct AddonResource
{
    std::string name;               // "catalog", "meta", "stream", "subtitles", ...
    std::vector<std::string> types; // empty means every type
    std::vector<std::string> idPrefixes;
};

struct AddonBehaviorHints
{
    std::optional<bool> configurable;
    std::optional<bool> configurationRequired;
    std::optional<bool> newEpisodeNotifications;
};

struct Addon
{
    std::string id;
    std::string name;
    std::string displayName; // name, the custom name, or "Name (2)" for duplicates
    std::string version;
    std::optional<std::string> description;
    std::optional<std::string> logo;
    std::optional<std::string> background;
    std::string baseUrl; // canonical install URL, the add-on's identity
    std::vector<CatalogDescriptor> catalogs;
    std::vector<std::string> types;
    std::vector<AddonResource> resources;
    std::vector<std::string> idPrefixes;
    std::optional<AddonBehaviorHints> behaviorHints;
    bool enabled = true;

    // No manifest has been loaded yet: no resources or catalogs, named after its URL.
    bool placeholder = false;
};

} // namespace nuvio::domain
