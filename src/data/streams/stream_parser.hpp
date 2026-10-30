#pragma once

#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "domain/stream.hpp"

// Stream responses and the inline streams of a meta video
// (NuvioTV data/remote/dto/StreamDto.kt, data/mapper/StreamMapper.kt).
namespace nuvio::data::streams
{

// One stream object; nullopt when it isn't an object.
std::optional<domain::Stream> parseStream(const nlohmann::json& json, const std::string& addonName);

// A "streams" array. Entries that aren't objects are dropped.
std::vector<domain::Stream> parseStreamList(const nlohmann::json& array, const std::string& addonName);

// A `{"streams": [...]}` response: nullopt when the body isn't a JSON object, an empty
// list when it has no usable "streams" array.
std::optional<std::vector<domain::Stream>> parseStreamResponse(const std::string& body, const std::string& addonName);

// The streams of Video::streamsJson (the inline streams a meta lists for a video);
// empty when there are none.
std::vector<domain::Stream> parseInlineStreams(const std::string& streamsJson, const std::string& addonName);

} // namespace nuvio::data::streams
