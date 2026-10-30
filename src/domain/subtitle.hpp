#pragma once

#include <map>
#include <string>

// An external subtitle the player can add (NuvioTV domain/model/Subtitle.kt): one the stream
// came with or one a subtitles add-on offered.
namespace nuvio::domain
{

struct Subtitle
{
    std::string id;
    std::string url;
    std::string lang; // "und" when unknown
    std::string addonName;
    bool streamProvided = false;
    std::map<std::string, std::string> headers;
};

} // namespace nuvio::domain
