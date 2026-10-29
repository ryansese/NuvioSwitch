#include "data/catalog/catalog_parser.hpp"

#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <unordered_set>

namespace nuvio::data::catalog
{

using Json = nlohmann::json;
using domain::MetaPreview;

namespace
{
bool isBlank(const std::string& s)
{
    for (unsigned char c : s)
        if (!std::isspace(c))
            return false;
    return true;
}

// A string field; numbers are taken as their text, as Moshi's String adapter does.
std::optional<std::string> text(const Json& j, const char* key)
{
    auto it = j.find(key);
    if (it == j.end())
        return std::nullopt;
    if (it->is_string())
        return it->get<std::string>();
    if (it->is_number())
        return it->dump();
    return std::nullopt;
}

// Kotlin's String.toFloatOrNull().
std::optional<float> toFloat(const std::string& s)
{
    if (s.empty() || std::isspace(static_cast<unsigned char>(s.front())) ||
        std::isspace(static_cast<unsigned char>(s.back())))
        return std::nullopt;
    char* end = nullptr;
    errno     = 0;
    float f   = std::strtof(s.c_str(), &end);
    if (errno != 0 || end != s.c_str() + s.size())
        return std::nullopt;
    return f;
}

std::vector<std::string> genres(const Json& j)
{
    std::vector<std::string> out;
    auto it = j.find("genres");
    if (it == j.end() || !it->is_array())
        return out;
    for (const auto& genre : *it)
    {
        if (genre.is_string())
            out.push_back(genre.get<std::string>());
        else if (genre.is_number())
            out.push_back(genre.dump());
    }
    return out;
}
} // namespace

std::optional<ParsedCatalog> parseCatalog(const Json& response, const std::string& catalogType,
    const std::string& sourceAddonBaseUrl)
{
    if (!response.is_object())
        return std::nullopt;

    ParsedCatalog parsed;
    auto metas = response.find("metas");
    if (metas == response.end() || !metas->is_array())
        return parsed;

    parsed.rawCount = metas->size();
    std::unordered_set<std::string> seen;
    for (const auto& meta : *metas)
    {
        if (!meta.is_object())
            continue;
        auto id   = text(meta, "id");
        auto name = text(meta, "name");
        if (!id || isBlank(*id) || !name || isBlank(*name) || !seen.insert(*id).second)
            continue;

        MetaPreview item;
        item.id   = *id;
        item.name = *name;
        auto type = text(meta, "type");
        item.type = type && !isBlank(*type) ? *type : catalogType;

        item.poster      = text(meta, "poster");
        item.posterShape = domain::posterShapeFrom(text(meta, "posterShape").value_or(""));
        item.background  = text(meta, "background");
        item.logo        = text(meta, "logo");
        item.description = text(meta, "description");
        item.releaseInfo = text(meta, "releaseInfo");
        if (auto rating = text(meta, "imdbRating"))
            item.imdbRating = toFloat(*rating);
        item.genres             = genres(meta);
        item.sourceAddonBaseUrl = sourceAddonBaseUrl;
        parsed.items.push_back(std::move(item));
    }
    return parsed;
}

std::optional<ParsedCatalog> parseCatalog(const std::string& body, const std::string& catalogType,
    const std::string& sourceAddonBaseUrl)
{
    Json response = Json::parse(body, nullptr, false);
    if (response.is_discarded())
        return std::nullopt;
    return parseCatalog(response, catalogType, sourceAddonBaseUrl);
}

} // namespace nuvio::data::catalog
