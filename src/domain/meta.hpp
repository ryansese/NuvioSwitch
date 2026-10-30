#pragma once

#include <optional>
#include <string>
#include <vector>

// A title's full metadata as a meta add-on returns it (NuvioTV domain/model/Meta.kt,
// data/remote/dto/MetaResponseDto.kt).
namespace nuvio::domain
{

struct Video
{
    std::string id;
    std::string title; // "title", else "name"
    std::optional<std::string> released;
    std::optional<std::string> thumbnail;
    std::optional<int> season;
    std::optional<int> episode; // "episode", else "number"
    std::optional<std::string> overview; // "overview", else "description"
    std::optional<bool> available;
    std::string streamsJson; // the inline "streams" array as raw JSON for the stream picker, or ""
};

struct Meta
{
    std::string id;
    std::string type;
    std::string name;
    std::optional<std::string> poster;
    std::optional<std::string> background;
    std::optional<std::string> logo;
    std::optional<std::string> description;
    std::optional<std::string> releaseInfo;
    std::optional<float> imdbRating;
    std::optional<std::string> runtime;
    std::vector<std::string> genres;
    std::vector<std::string> director;
    std::vector<std::string> cast;
    std::vector<Video> videos;
    std::optional<std::string> defaultVideoId;

    // The add-on that answered, for log lines and the stream picker.
    std::string addonBaseUrl;
};

// One season's episodes in episode order. season is nullopt for the single
// virtual season made of videos that carry no season numbers.
struct Season
{
    std::optional<int> number;
    std::vector<Video> episodes;
};

// Seasons in ascending order with specials (season 0) last, each with its episodes sorted by episode number
// (NuvioTV MetaDetailsViewModel.getEpisodesForSeason). Videos without a season
// join season 0 when any video has one; with no season numbers at all every video
// goes into one virtual season, in the add-on's order.
std::vector<Season> groupSeasons(const std::vector<Video>& videos);

// Index into seasons of the one to show first: the season of the default video
// when it exists and is not marked unavailable, else the first season above 0,
// else the first one. 0 when seasons is empty.
size_t initialSeasonIndex(const std::vector<Season>& seasons, const std::optional<std::string>& defaultVideoId);

// What Play opens the stream picker with (NuvioTV's rule): a movie uses the
// default video ID or the meta ID. A series uses the first episode of the selected
// season, or the meta ID when it has none.
std::string playVideoId(const Meta& meta, const Season* selected);

} // namespace nuvio::domain
