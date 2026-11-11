#include "data/library/library.hpp"

#include <algorithm>
#include <cctype>

#include "core/json/json.hpp"

namespace nuvio::data::library
{

namespace
{
using Json = nlohmann::json;
using domain::PosterShape;

std::string lowercase(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

const char* shapeName(PosterShape shape)
{
    switch (shape)
    {
        case PosterShape::Landscape: return "LANDSCAPE";
        case PosterShape::Square: return "SQUARE";
        default: return "POSTER";
    }
}

void putOpt(Json& json, const char* key, const std::optional<std::string>& value)
{
    json[key] = value ? Json(*value) : Json();
}
} // namespace

std::string sortKey(Sort sort)
{
    switch (sort)
    {
        case Sort::Provider: return "default";
        case Sort::AddedOldest: return "added_asc";
        case Sort::TitleAsc: return "title_asc";
        case Sort::TitleDesc: return "title_desc";
        default: return "added_desc";
    }
}

Sort sortFromKey(const std::string& key)
{
    for (Sort sort : {Sort::Provider, Sort::AddedNewest, Sort::AddedOldest, Sort::TitleAsc, Sort::TitleDesc})
        if (sortKey(sort) == key)
            return sort;
    return Sort::AddedNewest;
}

std::string filterKey(Filter filter)
{
    switch (filter)
    {
        case Filter::Movies: return "movie";
        case Filter::Series: return "series";
        default: return "all";
    }
}

Filter filterFromKey(const std::string& key)
{
    if (key == "movie")
        return Filter::Movies;
    if (key == "series")
        return Filter::Series;
    return Filter::All;
}

Json toJson(const SavedLibraryItem& item)
{
    Json json = Json::object();
    json["id"]   = item.id;
    json["type"] = item.type;
    json["name"] = item.name;
    putOpt(json, "poster", item.poster);
    json["posterShape"] = shapeName(item.posterShape);
    putOpt(json, "background", item.background);
    putOpt(json, "logo", item.logo);
    putOpt(json, "description", item.description);
    putOpt(json, "releaseInfo", item.releaseInfo);
    json["imdbRating"] = item.imdbRating ? Json(*item.imdbRating) : Json();
    json["genres"]     = item.genres;
    putOpt(json, "addonBaseUrl", item.addonBaseUrl);
    json["addedAt"] = item.addedAt;
    return json;
}

std::optional<SavedLibraryItem> itemFromJson(const Json& json)
{
    if (!json.is_object())
        return std::nullopt;
    SavedLibraryItem item;
    item.id   = core::json::str(json, "id");
    item.type = core::json::str(json, "type");
    if (item.id.empty() || item.type.empty())
        return std::nullopt;
    item.name        = core::json::str(json, "name");
    item.poster      = core::json::optStr(json, "poster");
    item.posterShape = domain::posterShapeFrom(core::json::str(json, "posterShape"));
    item.background  = core::json::optStr(json, "background");
    item.logo        = core::json::optStr(json, "logo");
    item.description = core::json::optStr(json, "description");
    item.releaseInfo = core::json::optStr(json, "releaseInfo");
    if (json.contains("imdbRating") && json["imdbRating"].is_number())
        item.imdbRating = json["imdbRating"].get<float>();
    item.genres       = core::json::strList(json, "genres");
    item.addonBaseUrl = core::json::optStr(json, "addonBaseUrl");
    item.addedAt      = core::json::optInt(json, "addedAt").value_or(0);
    return item;
}

SavedLibraryItem itemFromMeta(const domain::Meta& meta, int64_t addedAt)
{
    SavedLibraryItem item;
    item.id          = meta.id;
    item.type        = meta.type;
    item.name        = meta.name;
    item.poster      = meta.poster;
    item.background  = meta.background;
    item.logo        = meta.logo;
    item.description = meta.description;
    item.releaseInfo = meta.releaseInfo;
    item.imdbRating  = meta.imdbRating;
    item.genres      = meta.genres;
    if (!meta.addonBaseUrl.empty())
        item.addonBaseUrl = meta.addonBaseUrl;
    item.addedAt = addedAt;
    return item;
}

domain::MetaPreview toPreview(const SavedLibraryItem& item)
{
    domain::MetaPreview preview;
    preview.id                 = item.id;
    preview.type               = item.type;
    preview.name               = item.name;
    preview.poster             = item.poster;
    preview.posterShape        = item.posterShape;
    preview.background         = item.background;
    preview.logo               = item.logo;
    preview.description        = item.description;
    preview.releaseInfo        = item.releaseInfo;
    preview.imdbRating         = item.imdbRating;
    preview.genres             = item.genres;
    preview.sourceAddonBaseUrl = item.addonBaseUrl.value_or("");
    return preview;
}

std::vector<SavedLibraryItem> sorted(std::vector<SavedLibraryItem> items, Sort sort)
{
    auto title = [](const SavedLibraryItem& item) { return lowercase(item.name.empty() ? item.id : item.name); };
    switch (sort)
    {
        case Sort::Provider: break;
        case Sort::AddedNewest:
            std::stable_sort(items.begin(), items.end(), [](const auto& a, const auto& b) { return a.addedAt > b.addedAt; });
            break;
        case Sort::AddedOldest:
            std::stable_sort(items.begin(), items.end(), [](const auto& a, const auto& b) { return a.addedAt < b.addedAt; });
            break;
        case Sort::TitleAsc:
            std::stable_sort(items.begin(), items.end(), [&](const auto& a, const auto& b) { return title(a) < title(b); });
            break;
        case Sort::TitleDesc:
            std::stable_sort(items.begin(), items.end(), [&](const auto& a, const auto& b) { return title(a) > title(b); });
            break;
    }
    return items;
}

std::vector<SavedLibraryItem> filtered(const std::vector<SavedLibraryItem>& items, Filter filter)
{
    if (filter == Filter::All)
        return items;
    std::vector<SavedLibraryItem> out;
    for (const auto& item : items)
    {
        bool series = item.type == "series" || item.type == "tv";
        if ((filter == Filter::Series) == series && (filter == Filter::Series || item.type == "movie"))
            out.push_back(item);
    }
    return out;
}

} // namespace nuvio::data::library
