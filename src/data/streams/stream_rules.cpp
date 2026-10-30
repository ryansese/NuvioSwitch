#include "data/streams/stream_rules.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <set>
#include <strings.h>

#include "data/addons/addon_query.hpp"
#include "data/catalog/catalog_url.hpp"

namespace nuvio::data::streams
{

using domain::Stream;

namespace
{
std::string trim(const std::string& s)
{
    size_t begin = 0;
    size_t end   = s.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(s[begin])))
        begin++;
    while (end > begin && std::isspace(static_cast<unsigned char>(s[end - 1])))
        end--;
    return s.substr(begin, end - begin);
}

std::string trimTrailingSlashes(std::string s)
{
    while (!s.empty() && s.back() == '/')
        s.pop_back();
    return s;
}

std::string lower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

bool startsWithIgnoreCase(const std::string& s, const std::string& prefix)
{
    return s.size() >= prefix.size() && strncasecmp(s.c_str(), prefix.c_str(), prefix.size()) == 0;
}

bool isMagnet(const std::string& url)
{
    return startsWithIgnoreCase(trim(url), "magnet:");
}

// "torrent:<hash>/<idx>" links, which NuvioTV treats as torrents
bool isTorrentScheme(const std::string& url)
{
    return startsWithIgnoreCase(trim(url), "torrent:");
}

// a ".torrent" file: the path, ignoring any query or fragment
bool isTorrentFile(const std::string& url)
{
    std::string path = lower(trim(url));
    path             = path.substr(0, path.find_first_of("?#"));
    return path.size() >= 8 && path.compare(path.size() - 8, 8, ".torrent") == 0;
}

bool isHttp(const std::string& url)
{
    std::string t = trim(url);
    return startsWithIgnoreCase(t, "http://") || startsWithIgnoreCase(t, "https://");
}

bool hasText(const std::optional<std::string>& s)
{
    return s && !trim(*s).empty();
}

bool isNumber(const std::string& s)
{
    return !s.empty() && std::all_of(s.begin(), s.end(), [](unsigned char c) { return std::isdigit(c); });
}

std::vector<std::string> split(const std::string& s, char sep)
{
    std::vector<std::string> out;
    size_t start = 0;
    while (true)
    {
        size_t at = s.find(sep, start);
        if (at == std::string::npos)
        {
            out.push_back(s.substr(start));
            return out;
        }
        out.push_back(s.substr(start, at - start));
        start = at + 1;
    }
}

std::string singleLine(const std::string& text)
{
    std::string out;
    bool space = false;
    for (char c : text)
    {
        if (c == '\n' || c == '\r' || c == '\t' || c == ' ')
        {
            space = !out.empty();
            continue;
        }
        if (space)
            out += ' ';
        space = false;
        out += c;
    }
    return out;
}
} // namespace

std::vector<domain::Addon> streamAddons(const std::vector<domain::Addon>& addons, const std::string& type,
    const std::string& videoId)
{
    return addons::addonsFor(addons, "stream", type, videoId);
}

std::string buildStreamUrl(const std::string& baseUrl, const std::string& type, const std::string& videoId)
{
    std::string base  = trimTrailingSlashes(baseUrl);
    size_t queryStart = base.find('?');
    std::string basePath =
        queryStart == std::string::npos ? base : trimTrailingSlashes(base.substr(0, queryStart));
    std::string baseQuery = queryStart == std::string::npos ? "" : base.substr(queryStart);
    return basePath + "/stream/" + catalog::encodeArg(type) + "/" + catalog::encodeArg(videoId) + ".json" + baseQuery;
}

std::string contentIdFromVideoId(const std::string& videoId)
{
    std::vector<std::string> parts = split(videoId, ':');
    if (parts.size() <= 1)
        return videoId;

    size_t trailingNumeric = 0;
    for (size_t i = parts.size(); i > 0 && isNumber(parts[i - 1]); i--)
        trailingNumeric++;

    const std::string& first = parts.front();
    size_t minSegments       = (first.compare(0, 2, "tt") == 0 || isNumber(first)) ? 1 : 2;
    size_t droppable         = parts.size() > minSegments ? parts.size() - minSegments : 0;
    size_t drop              = std::min({trailingNumeric, droppable, size_t(2)});
    if (drop == 0)
        return videoId;

    std::string out;
    for (size_t i = 0; i + drop < parts.size(); i++)
        out += (i ? ":" : "") + parts[i];
    return out;
}

std::string dedupKey(const Stream& stream)
{
    if (stream.infoHash)
        return lower(*stream.infoHash) + ":" + (stream.fileIdx ? std::to_string(*stream.fileIdx) : "");
    if (stream.url)
    {
        bool named = stream.name && !trim(*stream.name).empty();
        return *stream.url + (named ? "|" + *stream.name : "");
    }
    if (stream.externalUrl)
        return *stream.externalUrl;
    if (stream.ytId)
        return *stream.ytId;
    return stream.addonName + ":" + stream.name.value_or("null") + ":" + stream.title.value_or("null");
}

std::vector<Stream> dedup(std::vector<Stream> streams)
{
    std::set<std::string> seen;
    std::vector<Stream> out;
    for (auto& stream : streams)
        if (seen.insert(dedupKey(stream)).second)
            out.push_back(std::move(stream));
    return out;
}

std::optional<std::string> playableUrl(const Stream& stream)
{
    if (hasText(stream.url) && isHttp(*stream.url) && !isTorrentFile(*stream.url))
        return trim(*stream.url);
    return std::nullopt;
}

Playability classify(const Stream& stream)
{
    if (playableUrl(stream))
        return Playability::Playable;

    auto torrentLink = [](const std::optional<std::string>& url)
    { return hasText(url) && (isMagnet(*url) || isTorrentScheme(*url) || isTorrentFile(*url)); };
    if (hasText(stream.infoHash) || torrentLink(stream.url) || torrentLink(stream.externalUrl))
        return Playability::NeedsDebrid;
    return Playability::Unsupported;
}

std::map<std::string, std::string> requestHeaders(const Stream& stream)
{
    std::map<std::string, std::string> out;
    for (const auto& [name, value] : stream.behaviorHints.proxyRequestHeaders)
        if (strcasecmp(name.c_str(), "range") != 0)
            out[name] = value;
    return out;
}

std::string displayName(const Stream& stream)
{
    for (const auto* field : {&stream.name, &stream.title, &stream.description})
        if (*field)
            return singleLine(**field);
    return "";
}

std::string displayDescription(const Stream& stream)
{
    if (stream.description)
        return *stream.description;
    return stream.title.value_or("");
}

std::vector<std::string> descriptionLines(const std::string& text, size_t maxLines)
{
    std::vector<std::string> out;
    for (const auto& line : split(text, '\n'))
    {
        std::string t = trim(line);
        if (t.empty())
            continue;
        out.push_back(t);
        if (out.size() == maxLines)
            break;
    }
    return out;
}

std::string formatSize(int64_t bytes)
{
    if (bytes <= 0)
        return "";
    constexpr double MB = 1024.0 * 1024.0;
    constexpr double GB = MB * 1024.0;
    char buffer[32];
    if (bytes >= GB)
        std::snprintf(buffer, sizeof buffer, "%.1f GB", bytes / GB);
    else
        std::snprintf(buffer, sizeof buffer, "%.0f MB", bytes / MB);
    return buffer;
}

std::optional<domain::PlaybackRequest> playbackRequest(const Stream& stream, const domain::StreamRequest& request)
{
    auto url = playableUrl(stream);
    if (!url)
        return std::nullopt;

    domain::PlaybackRequest out;
    out.url        = *url;
    out.headers    = requestHeaders(stream);
    out.subtitles  = stream.subtitles;
    out.bingeGroup = stream.behaviorHints.bingeGroup;
    out.filename   = stream.behaviorHints.filename;
    out.videoHash  = stream.behaviorHints.videoHash;
    out.videoSize  = stream.behaviorHints.videoSize;
    out.addonName  = stream.addonName;
    out.streamName = displayName(stream);

    out.contentType  = request.contentType;
    out.contentId    = request.contentId;
    out.videoId      = request.videoId;
    out.season       = request.season;
    out.episode      = request.episode;
    out.title        = request.title;
    out.episodeTitle = request.episodeTitle;
    out.poster       = request.poster;
    out.backdrop     = request.backdrop;
    out.logo         = request.logo;
    return out;
}

} // namespace nuvio::data::streams
