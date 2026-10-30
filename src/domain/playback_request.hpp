#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "domain/stream.hpp"

// What the stream picker hands to the player (NuvioSwitch-0008): the chosen stream's
// URL, headers and subtitles, and the title it belongs to. Mirrors NuvioTV's
// PlayerNavigationArgs without the torrent and debrid fields.
namespace nuvio::domain
{

struct PlaybackRequest
{
    std::string url;
    std::map<std::string, std::string> headers; // proxyHeaders.request without Range
    std::vector<StreamSubtitle> subtitles;
    std::optional<std::string> bingeGroup;
    std::optional<std::string> filename;
    std::optional<std::string> videoHash;
    std::optional<int64_t> videoSize;
    std::string addonName;
    std::string streamName; // the card's heading
    std::optional<int64_t> startPositionMs; // where to start; NuvioSwitch-0009 supplies the resume point

    std::string contentType;
    std::string contentId;
    std::string videoId;
    std::optional<int> season;
    std::optional<int> episode;
    std::string title;
    std::optional<std::string> episodeTitle;
    std::optional<std::string> poster;
    std::optional<std::string> backdrop;
    std::optional<std::string> logo;
};

} // namespace nuvio::domain
