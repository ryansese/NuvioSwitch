#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

// The pure rules of the player (NuvioTV NuvioMpvSurfaceView.kt, PlayerScrubRates.kt,
// SubtitleRepositoryImpl.kt): what to tell mpv, and how to read what it says back.
namespace nuvio::data::player
{

// The value of mpv's http-header-fields: headers with a non-blank key and value, sorted by
// lowercase key then value, as "Key: Value" with "\" and "," escaped, joined by ",".
std::string headerFields(const std::map<std::string, std::string>& headers);

// The arguments of mpv's loadfile command: `loadfile <url> replace`, and with a start
// position above 0 also `-1 start=+<seconds>` (the options go after the index).
std::vector<std::string> loadfileArgs(const std::string& url, std::optional<int64_t> startMs);

// How far one Left/Right seek goes (ms) for the key's repeat count: 10 s, 20 s from the
// 3rd repeat, 30 s from the 8th, 60 s from the 15th.
int64_t seekStepMs(int repeat);

// "tv" is "series"; anything else lowercased.
std::string subtitleType(const std::string& type);

// {basePath}/subtitles/{type}/{id}[/{params}].json{baseQuery}. The ID is videoId when there is one
// (always for series), else contentId. params are videoHash, videoSize and filename, as far as known.
std::string subtitleUrl(const std::string& baseUrl, const std::string& type, const std::string& contentId,
    const std::string& videoId, const std::optional<std::string>& videoHash, const std::optional<int64_t>& videoSize,
    const std::optional<std::string>& filename);

struct Track
{
    int64_t id       = 0;
    std::string type; // "audio", "sub" or "video"
    std::string title;
    std::string lang;
    std::string codec;
    int channels     = 0;
    bool selected    = false;
    bool external    = false;
    std::string externalFilename;
};

// mpv's `track-list` property as JSON text: nullopt for anything that isn't an array.
// Entries without an id or a type are dropped.
std::optional<std::vector<Track>> parseTrackList(const std::string& json);

// A language's name for an ISO 639-1/-2 code ("eng" and "en" give "English"); the code uppercased when unknown.
std::string languageName(const std::string& code);

// "m:ss" or "h:mm:ss" of whole seconds; a negative value gets a leading "-".
std::string formatTime(double seconds);

} // namespace nuvio::data::player
