#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "domain/addon.hpp"
#include "domain/playback_request.hpp"
#include "domain/stream.hpp"
#include "domain/stream_request.hpp"

// The pure rules of the stream picker: which add-ons are asked, what a stream is
// called, whether it can play and what the player gets
// (NuvioTV core/streams/PlaybackAvailability.kt, domain/model/Stream.kt,
// data/repository/StreamRepositoryImpl.kt, data/mapper/StreamMapper.kt).
namespace nuvio::data::streams
{

// The enabled add-ons with a "stream" resource for type and videoId, in installed order.
std::vector<domain::Addon> streamAddons(const std::vector<domain::Addon>& addons, const std::string& type,
    const std::string& videoId);

// {basePath}/stream/{type}/{videoId}.json{baseQuery}; type and videoId are encoded like
// Java's URLEncoder with "+" as "%20" (NuvioTV encodePathSegment).
std::string buildStreamUrl(const std::string& baseUrl, const std::string& type, const std::string& videoId);

// The meta ID of a video ID for inline streams: up to two trailing numeric ":" segments
// are dropped, keeping one segment for IDs starting with "tt" or a number and two otherwise.
std::string contentIdFromVideoId(const std::string& videoId);

// The key streams of one add-on are de-duplicated by: lowercased info hash and file index,
// else the URL with the name, else the external URL, else the YouTube ID, else add-on, name and title.
std::string dedupKey(const domain::Stream& stream);

// The streams with the first of each key kept, in order.
std::vector<domain::Stream> dedup(std::vector<domain::Stream> streams);

enum class Playability
{
    Playable,
    NeedsDebrid, // torrent only
    Unsupported, // a web link to open elsewhere, or YouTube
};

// Playable: an HTTP(S) "url" that isn't a torrent. NeedsDebrid: an info hash, magnet link or
// torrent URL without one. Everything else is unsupported.
Playability classify(const domain::Stream& stream);

// The URL to play, when classify() says Playable.
std::optional<std::string> playableUrl(const domain::Stream& stream);

// proxyHeaders.request without any Range header (any case).
std::map<std::string, std::string> requestHeaders(const domain::Stream& stream);

// "name", else "title", else "description", else "" (the UI shows its own "Unknown stream"),
// with line breaks turned into spaces.
std::string displayName(const domain::Stream& stream);

// "description", else "title"; "" when neither.
std::string displayDescription(const domain::Stream& stream);

// The first lines of text, each trimmed, blanks dropped, at most maxLines.
std::vector<std::string> descriptionLines(const std::string& text, size_t maxLines);

// "12.3 GB" from 1 GB up, "850 MB" below (1024-based); "" for zero or less.
std::string formatSize(int64_t bytes);

// What the player receives for a playable stream, with the title's details from the
// details screen. nullopt when the stream isn't playable.
std::optional<domain::PlaybackRequest> playbackRequest(const domain::Stream& stream,
    const domain::StreamRequest& request);

} // namespace nuvio::data::streams
