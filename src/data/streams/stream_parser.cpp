#include "data/streams/stream_parser.hpp"

#include "core/json/json.hpp"

namespace nuvio::data::streams
{

using core::json::Json;
using domain::Stream;
using domain::StreamSubtitle;

namespace
{
bool blank(const std::string& s)
{
    return s.find_first_not_of(" \t\r\n") == std::string::npos;
}

std::map<std::string, std::string> stringMap(const Json& j)
{
    std::map<std::string, std::string> out;
    if (!j.is_object())
        return out;
    for (auto it = j.begin(); it != j.end(); ++it)
        if (it.value().is_string())
            out[it.key()] = it.value().get<std::string>();
    return out;
}

std::vector<StreamSubtitle> parseSubtitles(const Json& j)
{
    std::vector<StreamSubtitle> out;
    auto it = j.find("subtitles");
    if (it == j.end() || !it->is_array())
        return out;
    for (const auto& s : *it)
    {
        if (!s.is_object())
            continue;
        std::string url = core::json::str(s, "url");
        if (blank(url))
            continue;
        StreamSubtitle subtitle;
        subtitle.url  = url;
        std::string id = core::json::str(s, "id");
        subtitle.id   = blank(id) ? url : id;
        std::string lang = core::json::str(s, "lang");
        subtitle.lang = blank(lang) ? "Unknown" : lang;
        if (auto h = s.find("headers"); h != s.end())
            subtitle.headers = stringMap(*h);
        out.push_back(std::move(subtitle));
    }
    return out;
}
} // namespace

std::optional<Stream> parseStream(const Json& j, const std::string& addonName)
{
    if (!j.is_object())
        return std::nullopt;

    Stream stream;
    stream.addonName   = addonName;
    stream.name        = core::json::optStr(j, "name");
    stream.title       = core::json::optStr(j, "title");
    stream.description = core::json::optStr(j, "description");
    stream.url         = core::json::optStr(j, "url");
    stream.ytId        = core::json::optStr(j, "ytId");
    stream.infoHash    = core::json::optStr(j, "infoHash");
    stream.externalUrl = core::json::optStr(j, "externalUrl");
    if (auto idx = core::json::optInt(j, "fileIdx"))
        stream.fileIdx = static_cast<int>(*idx);
    stream.subtitles = parseSubtitles(j);

    if (auto hints = j.find("behaviorHints"); hints != j.end() && hints->is_object())
    {
        stream.behaviorHints.bingeGroup = core::json::optStr(*hints, "bingeGroup");
        stream.behaviorHints.filename   = core::json::optStr(*hints, "filename");
        stream.behaviorHints.videoHash  = core::json::optStr(*hints, "videoHash");
        stream.behaviorHints.videoSize  = core::json::optInt(*hints, "videoSize");
        if (auto proxy = hints->find("proxyHeaders"); proxy != hints->end() && proxy->is_object())
            if (auto request = proxy->find("request"); request != proxy->end())
                stream.behaviorHints.proxyRequestHeaders = stringMap(*request);
    }
    return stream;
}

std::vector<Stream> parseStreamList(const Json& array, const std::string& addonName)
{
    std::vector<Stream> out;
    if (!array.is_array())
        return out;
    for (const auto& entry : array)
        if (auto stream = parseStream(entry, addonName))
            out.push_back(std::move(*stream));
    return out;
}

std::optional<std::vector<Stream>> parseStreamResponse(const std::string& body, const std::string& addonName)
{
    Json json = Json::parse(body, nullptr, false);
    if (!json.is_object())
        return std::nullopt;
    auto it = json.find("streams");
    if (it == json.end())
        return std::vector<Stream>{};
    return parseStreamList(*it, addonName);
}

std::vector<Stream> parseInlineStreams(const std::string& streamsJson, const std::string& addonName)
{
    if (streamsJson.empty())
        return {};
    return parseStreamList(Json::parse(streamsJson, nullptr, false), addonName);
}

} // namespace nuvio::data::streams
