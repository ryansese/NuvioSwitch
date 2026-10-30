#include "data/meta/meta_parser.hpp"

#include <cctype>
#include <cerrno>
#include <cstdlib>

#include "core/json/json.hpp"

namespace nuvio::data::meta
{

using Json = nlohmann::json;
using domain::Meta;
using domain::Video;
namespace json = core::json;

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

// The first of keys that holds a non-blank value.
std::optional<std::string> firstText(const Json& j, std::initializer_list<const char*> keys)
{
    for (const char* key : keys)
    {
        auto value = text(j, key);
        if (value && !isBlank(*value))
            return value;
    }
    return std::nullopt;
}

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

// A whole number sent as a number or a string ("3", "3.0").
std::optional<int> toInt(const Json& j, const char* key)
{
    auto it = j.find(key);
    if (it == j.end())
        return std::nullopt;
    if (it->is_number())
        return static_cast<int>(it->get<double>());
    if (it->is_string())
    {
        auto value = toFloat(it->get<std::string>());
        if (value)
            return static_cast<int>(*value);
    }
    return std::nullopt;
}

Video parseVideo(const Json& j)
{
    Video video;
    video.id        = text(j, "id").value_or("");
    video.title     = firstText(j, {"title", "name"}).value_or("");
    video.released  = text(j, "released");
    video.thumbnail = text(j, "thumbnail");
    video.season    = toInt(j, "season");
    // "episode", else "number"
    video.episode  = toInt(j, "episode");
    if (!video.episode)
        video.episode = toInt(j, "number");
    video.overview = firstText(j, {"overview", "description"});
    if (auto it = j.find("available"); it != j.end() && it->is_boolean())
        video.available = it->get<bool>();
    if (auto it = j.find("streams"); it != j.end() && it->is_array() && !it->empty())
        video.streamsJson = it->dump();
    return video;
}
} // namespace

std::optional<Meta> parseMeta(const Json& response, const std::string& requestedType, const std::string& addonBaseUrl)
{
    if (!response.is_object())
        return std::nullopt;
    auto it = response.find("meta");
    if (it == response.end() || !it->is_object())
        return std::nullopt;
    const Json& j = *it;

    auto id   = text(j, "id");
    auto name = text(j, "name");
    if (!id || isBlank(*id) || !name || isBlank(*name))
        return std::nullopt;

    Meta meta;
    meta.id           = *id;
    meta.name         = *name;
    auto type         = text(j, "type");
    meta.type         = type && !isBlank(*type) ? *type : requestedType;
    meta.poster       = text(j, "poster");
    meta.background   = text(j, "background");
    meta.logo         = text(j, "logo");
    meta.description  = text(j, "description");
    meta.releaseInfo  = text(j, "releaseInfo");
    if (auto rating = text(j, "imdbRating"))
        meta.imdbRating = toFloat(*rating);
    meta.runtime      = firstText(j, {"runtime"});
    meta.genres       = json::strList(j, "genres");
    // "director" and "cast" come as a string, a list, or a list of objects with a name
    meta.director = json::strList(j, "director");
    meta.cast     = json::strList(j, "cast");
    if (auto hints = j.find("behaviorHints"); hints != j.end() && hints->is_object())
        meta.defaultVideoId = firstText(*hints, {"defaultVideoId"});
    meta.addonBaseUrl = addonBaseUrl;

    if (auto videos = j.find("videos"); videos != j.end() && videos->is_array())
        for (const auto& entry : *videos)
            if (entry.is_object())
            {
                Video video = parseVideo(entry);
                if (!video.id.empty())
                    meta.videos.push_back(std::move(video));
            }
    return meta;
}

std::optional<Meta> parseMeta(const std::string& body, const std::string& requestedType,
    const std::string& addonBaseUrl)
{
    Json response = Json::parse(body, nullptr, false);
    if (response.is_discarded())
        return std::nullopt;
    return parseMeta(response, requestedType, addonBaseUrl);
}

} // namespace nuvio::data::meta
