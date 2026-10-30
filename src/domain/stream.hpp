#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

// One stream an add-on offers for a video (NuvioTV domain/model/Stream.kt,
// data/remote/dto/StreamDto.kt). Fields a response leaves out stay empty.
namespace nuvio::domain
{

struct StreamSubtitle
{
    std::string id; // the add-on's, else the URL
    std::string url;
    std::string lang; // "Unknown" when the add-on gave none
    std::map<std::string, std::string> headers;
};

struct StreamBehaviorHints
{
    // proxyHeaders.request as sent, including any Range header; see requestHeaders()
    std::map<std::string, std::string> proxyRequestHeaders;
    std::optional<std::string> bingeGroup;
    std::optional<std::string> filename;
    std::optional<std::string> videoHash;
    std::optional<int64_t> videoSize;
};

struct Stream
{
    std::optional<std::string> name;
    std::optional<std::string> title;
    std::optional<std::string> description;
    std::optional<std::string> url;
    std::optional<std::string> ytId;
    std::optional<std::string> infoHash;
    std::optional<int> fileIdx;
    std::optional<std::string> externalUrl;
    StreamBehaviorHints behaviorHints;
    std::vector<StreamSubtitle> subtitles;

    std::string addonName; // the add-on's display name
};

} // namespace nuvio::domain
