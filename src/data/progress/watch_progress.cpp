#include "data/progress/watch_progress.hpp"

#include <algorithm>
#include <set>
#include <vector>

#include "core/json/json.hpp"

namespace nuvio::data::progress
{

namespace
{
using Json = nlohmann::json;

std::optional<int> optInt32(const Json& json, const char* key)
{
    if (auto value = core::json::optInt(json, key))
        return static_cast<int>(*value);
    return std::nullopt;
}

void putOpt(Json& json, const char* key, const std::optional<std::string>& value)
{
    json[key] = value ? Json(*value) : Json();
}

void putOpt(Json& json, const char* key, const std::optional<int>& value)
{
    json[key] = value ? Json(*value) : Json();
}
} // namespace

float progressPercentage(const WatchProgress& progress)
{
    if (progress.duration <= 0)
        return 0;
    float value = static_cast<float>(progress.position) / static_cast<float>(progress.duration);
    return std::clamp(value, 0.0f, 1.0f);
}

bool isCompleted(const WatchProgress& progress)
{
    return progressPercentage(progress) >= COMPLETED_THRESHOLD;
}

bool isInProgress(const WatchProgress& progress)
{
    float value = progressPercentage(progress);
    return value >= IN_PROGRESS_THRESHOLD && value < COMPLETED_THRESHOLD;
}

std::string progressKey(const std::string& contentId, std::optional<int> season, std::optional<int> episode)
{
    if (season && episode)
        return contentId + "_s" + std::to_string(*season) + "e" + std::to_string(*episode);
    return contentId;
}

std::string progressKey(const WatchProgress& progress)
{
    return progressKey(progress.contentId, progress.season, progress.episode);
}

bool isSeriesType(const std::string& contentType)
{
    return contentType == "series" || contentType == "tv";
}

Json toJson(const WatchProgress& p)
{
    Json json = Json::object();
    json["contentId"]   = p.contentId;
    json["contentType"] = p.contentType;
    json["name"]        = p.name;
    putOpt(json, "poster", p.poster);
    putOpt(json, "backdrop", p.backdrop);
    putOpt(json, "logo", p.logo);
    json["videoId"] = p.videoId;
    putOpt(json, "season", p.season);
    putOpt(json, "episode", p.episode);
    putOpt(json, "episodeTitle", p.episodeTitle);
    json["position"]    = p.position;
    json["duration"]    = p.duration;
    json["lastWatched"] = p.lastWatched;
    putOpt(json, "addonBaseUrl", p.addonBaseUrl);
    return json;
}

std::optional<WatchProgress> progressFromJson(const Json& json)
{
    if (!json.is_object())
        return std::nullopt;
    WatchProgress p;
    p.contentId = core::json::str(json, "contentId");
    if (p.contentId.empty())
        return std::nullopt;
    p.contentType  = core::json::str(json, "contentType");
    p.name         = core::json::str(json, "name");
    p.poster       = core::json::optStr(json, "poster");
    p.backdrop     = core::json::optStr(json, "backdrop");
    p.logo         = core::json::optStr(json, "logo");
    p.videoId      = core::json::str(json, "videoId", p.contentId);
    p.season       = optInt32(json, "season");
    p.episode      = optInt32(json, "episode");
    p.episodeTitle = core::json::optStr(json, "episodeTitle");
    p.position     = core::json::optInt(json, "position").value_or(0);
    p.duration     = core::json::optInt(json, "duration").value_or(0);
    p.lastWatched  = core::json::optInt(json, "lastWatched").value_or(0);
    p.addonBaseUrl = core::json::optStr(json, "addonBaseUrl");
    return p;
}

Json toJson(const WatchedItem& item)
{
    Json json = Json::object();
    json["contentId"]   = item.contentId;
    json["contentType"] = item.contentType;
    json["title"]       = item.title;
    putOpt(json, "season", item.season);
    putOpt(json, "episode", item.episode);
    json["watchedAt"] = item.watchedAt;
    putOpt(json, "poster", item.poster);
    return json;
}

std::optional<WatchedItem> watchedFromJson(const Json& json)
{
    if (!json.is_object())
        return std::nullopt;
    WatchedItem item;
    item.contentId = core::json::str(json, "contentId");
    if (item.contentId.empty())
        return std::nullopt;
    item.contentType = core::json::str(json, "contentType");
    item.title       = core::json::str(json, "title");
    item.season      = optInt32(json, "season");
    item.episode     = optInt32(json, "episode");
    item.watchedAt   = core::json::optInt(json, "watchedAt").value_or(0);
    item.poster      = core::json::optStr(json, "poster");
    return item;
}

void upsertProgress(ProgressMap& map, WatchProgress progress)
{
    std::string key = progressKey(progress);
    auto existing   = map.find(key);
    if (existing != map.end())
    {
        const WatchProgress& old = existing->second;
        if (progress.name.empty())
            progress.name = old.name;
        if (!progress.poster)
            progress.poster = old.poster;
        if (!progress.backdrop)
            progress.backdrop = old.backdrop;
        if (!progress.logo)
            progress.logo = old.logo;
        if (!progress.episodeTitle)
            progress.episodeTitle = old.episodeTitle;
    }
    bool episode = progress.season && progress.episode;
    std::string seriesKey = progress.contentId;
    map[key] = std::move(progress);
    if (episode && seriesKey != key)
        map.erase(seriesKey);
}

Buckets splitBuckets(const ProgressMap& entries, size_t limit)
{
    std::set<std::string> withEpisodes;
    for (const auto& [key, p] : entries)
        if (p.season && p.episode)
            withEpisodes.insert(p.contentId);

    std::vector<const ProgressMap::value_type*> sorted;
    for (const auto& entry : entries)
    {
        const WatchProgress& p = entry.second;
        bool seriesLevel = entry.first == p.contentId && !p.season && !p.episode && withEpisodes.count(p.contentId);
        if (!seriesLevel)
            sorted.push_back(&entry);
    }
    std::sort(sorted.begin(), sorted.end(), [](const auto* a, const auto* b)
        {
            if (a->second.lastWatched != b->second.lastWatched)
                return a->second.lastWatched > b->second.lastWatched;
            return a->first > b->first;
        });

    Buckets out;
    for (size_t i = 0; i < sorted.size(); i++)
        (i < limit ? out.recent : out.archive).insert(*sorted[i]);
    return out;
}

ProgressMap mergeBuckets(const ProgressMap& recent, const ProgressMap& archive)
{
    ProgressMap out = archive;
    for (const auto& [key, p] : recent)
        out[key] = p;
    return out;
}

std::optional<int64_t> resumePositionMs(const std::optional<WatchProgress>& progress)
{
    if (!progress || !isInProgress(*progress))
        return std::nullopt;
    return std::clamp<int64_t>(progress->position, 0, progress->duration);
}

int64_t remainingMs(const WatchProgress& progress)
{
    return std::max<int64_t>(0, progress.duration - progress.position);
}

} // namespace nuvio::data::progress
