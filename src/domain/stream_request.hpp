#pragma once

#include <optional>
#include <string>

#include "domain/meta.hpp"

// What the details screen hands to the stream picker (NuvioSwitch-0007): the title,
// the video to find streams for, and the artwork and episode info the player shows.
namespace nuvio::domain
{

struct StreamRequest
{
    std::string contentType;
    std::string contentId; // the meta ID
    std::string videoId;   // what stream add-ons are asked for
    std::string title;
    std::optional<std::string> poster;
    std::optional<std::string> backdrop;
    std::optional<std::string> logo;
    std::optional<int> season;
    std::optional<int> episode;
    std::optional<std::string> episodeTitle;
};

// Play: the meta's play video (see playVideoId), with the first episode's numbers for a series.
StreamRequest streamRequestForPlay(const Meta& meta, const Season* selected);

// An episode picked from the list.
StreamRequest streamRequestForEpisode(const Meta& meta, const Video& episode);

} // namespace nuvio::domain
