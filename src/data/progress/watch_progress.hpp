#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

// Watch progress and watched records (NuvioTV domain/model/WatchProgress.kt, WatchedItem.kt,
// data/local/WatchProgressPreferences.kt, WatchProgressBuckets.kt). Field names match NuvioTV's
// stored JSON so account sync (NuvioSwitch-0014) can push them unchanged. Times are epoch ms,
// positions ms.
namespace nuvio::data::progress
{

constexpr float IN_PROGRESS_THRESHOLD = 0.02f;
constexpr float COMPLETED_THRESHOLD   = 0.90f; // NuvioTV WatchProgress.COMPLETED_THRESHOLD
constexpr size_t RECENT_LIMIT         = 200;   // NuvioTV WATCH_PROGRESS_RECENT_LIMIT

struct WatchProgress
{
    std::string contentId;   // the meta ID of the movie or series
    std::string contentType; // "movie", "series"...
    std::string name;
    std::optional<std::string> poster;
    std::optional<std::string> backdrop;
    std::optional<std::string> logo;
    std::string videoId; // the video played: the episode's ID for series
    std::optional<int> season;
    std::optional<int> episode;
    std::optional<std::string> episodeTitle;
    int64_t position    = 0;
    int64_t duration    = 0; // 0 when unknown
    int64_t lastWatched = 0;
    std::optional<std::string> addonBaseUrl;
};

struct WatchedItem
{
    std::string contentId;
    std::string contentType;
    std::string title;
    std::optional<int> season;
    std::optional<int> episode;
    int64_t watchedAt = 0;
    std::optional<std::string> poster;
};

using ProgressMap = std::map<std::string, WatchProgress>;

// position / duration clamped to 0..1; 0 when the duration is unknown.
float progressPercentage(const WatchProgress& progress);
// From 2% and below 90%: shown in Continue Watching and resumed.
bool isInProgress(const WatchProgress& progress);
// From 90%.
bool isCompleted(const WatchProgress& progress);

// "{contentId}_s{season}e{episode}" when both are set, else "{contentId}".
std::string progressKey(const std::string& contentId, std::optional<int> season, std::optional<int> episode);
std::string progressKey(const WatchProgress& progress);

bool isSeriesType(const std::string& contentType); // "series" or "tv"

nlohmann::json toJson(const WatchProgress& progress);
std::optional<WatchProgress> progressFromJson(const nlohmann::json& json); // nullopt without a contentId
nlohmann::json toJson(const WatchedItem& item);
std::optional<WatchedItem> watchedFromJson(const nlohmann::json& json);

// Stores progress under its key. Name, poster, backdrop, logo and episode title missing from the
// new save are kept from the stored entry, and saving an episode removes a series-level entry keyed
// by the bare contentId (NuvioTV upsertProgressEntries).
void upsertProgress(ProgressMap& map, WatchProgress progress);

struct Buckets
{
    ProgressMap recent;
    ProgressMap archive;
};

// The limit newest entries by lastWatched (ties: key descending) in recent, the rest in archive.
// A series-level entry is dropped when episode entries of the same contentId exist
// (NuvioTV splitWatchProgressEntries).
Buckets splitBuckets(const ProgressMap& entries, size_t limit = RECENT_LIMIT);
// Both buckets as one map; recent wins.
ProgressMap mergeBuckets(const ProgressMap& recent, const ProgressMap& archive);

// Where playback starts for a saved entry: its position clamped to its duration when it is in
// progress, else nullopt (the beginning).
std::optional<int64_t> resumePositionMs(const std::optional<WatchProgress>& progress);

// Milliseconds left, never negative.
int64_t remainingMs(const WatchProgress& progress);

} // namespace nuvio::data::progress
