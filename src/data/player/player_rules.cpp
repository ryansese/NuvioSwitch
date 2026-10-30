#include "data/player/player_rules.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>

#include <nlohmann/json.hpp>

#include "core/json/json.hpp"
#include "data/catalog/catalog_url.hpp"

namespace nuvio::data::player
{

namespace
{
bool blank(const std::string& s)
{
    return std::all_of(s.begin(), s.end(), [](unsigned char c) { return std::isspace(c); });
}

std::string lower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

std::string trimTrailingSlashes(std::string s)
{
    while (!s.empty() && s.back() == '/')
        s.pop_back();
    return s;
}
} // namespace

std::string headerFields(const std::map<std::string, std::string>& headers)
{
    std::vector<std::pair<std::string, std::string>> entries;
    for (const auto& [key, value] : headers)
        if (!blank(key) && !blank(value))
            entries.emplace_back(key, value);
    std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b)
        {
            std::string ka = lower(a.first);
            std::string kb = lower(b.first);
            return ka != kb ? ka < kb : a.second < b.second;
        });

    std::string out;
    for (const auto& [key, value] : entries)
    {
        std::string field = key + ": " + value;
        std::string escaped;
        for (char c : field)
        {
            if (c == '\\' || c == ',')
                escaped += '\\';
            escaped += c;
        }
        out += (out.empty() ? "" : ",") + escaped;
    }
    return out;
}

std::vector<std::string> loadfileArgs(const std::string& url, std::optional<int64_t> startMs)
{
    if (!startMs || *startMs <= 0)
        return {"loadfile", url, "replace"};
    char start[48];
    std::snprintf(start, sizeof start, "start=+%lld.%03lld", static_cast<long long>(*startMs / 1000),
        static_cast<long long>(*startMs % 1000));
    return {"loadfile", url, "replace", "-1", start};
}

int64_t seekStepMs(int repeat)
{
    if (repeat >= 15)
        return 60000;
    if (repeat >= 8)
        return 30000;
    if (repeat >= 3)
        return 20000;
    return 10000;
}

std::string subtitleType(const std::string& type)
{
    return lower(type) == "tv" ? "series" : lower(type);
}

std::string subtitleUrl(const std::string& baseUrl, const std::string& type, const std::string& contentId,
    const std::string& videoId, const std::optional<std::string>& videoHash, const std::optional<int64_t>& videoSize,
    const std::optional<std::string>& filename)
{
    std::string base  = trimTrailingSlashes(baseUrl);
    size_t queryStart = base.find('?');
    std::string basePath =
        queryStart == std::string::npos ? base : trimTrailingSlashes(base.substr(0, queryStart));
    std::string baseQuery = queryStart == std::string::npos ? "" : base.substr(queryStart);

    std::string id = !blank(videoId) ? videoId : contentId;

    std::string params;
    auto add = [&params](const std::string& p) { params += (params.empty() ? "" : "&") + p; };
    if (videoHash)
        add("videoHash=" + *videoHash);
    if (videoSize)
        add("videoSize=" + std::to_string(*videoSize));
    if (filename)
        add("filename=" + catalog::encodeArg(*filename));

    std::string path = basePath + "/subtitles/" + catalog::encodeArg(subtitleType(type)) + "/" + catalog::encodeArg(id);
    return path + (params.empty() ? "" : "/" + params) + ".json" + baseQuery;
}

std::optional<std::vector<Track>> parseTrackList(const std::string& text)
{
    auto json = nlohmann::json::parse(text, nullptr, false);
    if (!json.is_array())
        return std::nullopt;

    std::vector<Track> out;
    for (const auto& entry : json)
    {
        if (!entry.is_object())
            continue;
        auto id   = core::json::optInt(entry, "id");
        auto type = core::json::optStr(entry, "type");
        if (!id || !type)
            continue;
        Track track;
        track.id     = *id;
        track.type   = *type;
        track.title  = core::json::str(entry, "title");
        track.lang   = core::json::str(entry, "lang");
        track.codec  = core::json::str(entry, "codec");
        track.channels = static_cast<int>(core::json::optInt(entry, "demux-channel-count").value_or(0));
        track.selected = core::json::optBool(entry, "selected").value_or(false);
        track.external = core::json::optBool(entry, "external").value_or(false);
        track.externalFilename = core::json::str(entry, "external-filename");
        out.push_back(std::move(track));
    }
    return out;
}

std::string languageName(const std::string& code)
{
    static const std::map<std::string, std::string> names = {
        {"en", "English"}, {"eng", "English"}, {"es", "Spanish"}, {"spa", "Spanish"}, {"fr", "French"},
        {"fre", "French"}, {"fra", "French"}, {"de", "German"}, {"ger", "German"}, {"deu", "German"},
        {"it", "Italian"}, {"ita", "Italian"}, {"pt", "Portuguese"}, {"por", "Portuguese"},
        {"pob", "Portuguese (Brazil)"}, {"pt-br", "Portuguese (Brazil)"}, {"ru", "Russian"}, {"rus", "Russian"},
        {"ja", "Japanese"}, {"jpn", "Japanese"}, {"ko", "Korean"}, {"kor", "Korean"}, {"zh", "Chinese"},
        {"chi", "Chinese"}, {"zho", "Chinese"}, {"zht", "Chinese (Traditional)"}, {"ar", "Arabic"},
        {"ara", "Arabic"}, {"hi", "Hindi"}, {"hin", "Hindi"}, {"tr", "Turkish"}, {"tur", "Turkish"},
        {"nl", "Dutch"}, {"dut", "Dutch"}, {"nld", "Dutch"}, {"pl", "Polish"}, {"pol", "Polish"},
        {"sv", "Swedish"}, {"swe", "Swedish"}, {"da", "Danish"}, {"dan", "Danish"}, {"no", "Norwegian"},
        {"nor", "Norwegian"}, {"fi", "Finnish"}, {"fin", "Finnish"}, {"el", "Greek"}, {"gre", "Greek"},
        {"ell", "Greek"}, {"he", "Hebrew"}, {"heb", "Hebrew"}, {"cs", "Czech"}, {"cze", "Czech"},
        {"ces", "Czech"}, {"hu", "Hungarian"}, {"hun", "Hungarian"}, {"ro", "Romanian"}, {"rum", "Romanian"},
        {"ron", "Romanian"}, {"bg", "Bulgarian"}, {"bul", "Bulgarian"}, {"uk", "Ukrainian"}, {"ukr", "Ukrainian"},
        {"th", "Thai"}, {"tha", "Thai"}, {"vi", "Vietnamese"}, {"vie", "Vietnamese"}, {"id", "Indonesian"},
        {"ind", "Indonesian"}, {"hr", "Croatian"}, {"hrv", "Croatian"}, {"sr", "Serbian"}, {"srp", "Serbian"},
        {"und", "Unknown"}};
    std::string key = lower(code);
    auto it         = names.find(key);
    if (it != names.end())
        return it->second;
    std::string out = key;
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return std::toupper(c); });
    return out.empty() ? "Unknown" : out;
}

std::string formatTime(double seconds)
{
    bool negative = seconds < 0;
    long long total = static_cast<long long>(std::floor(std::fabs(seconds)));
    long long h = total / 3600;
    long long m = (total % 3600) / 60;
    long long s = total % 60;
    char buffer[32];
    if (h > 0)
        std::snprintf(buffer, sizeof buffer, "%lld:%02lld:%02lld", h, m, s);
    else
        std::snprintf(buffer, sizeof buffer, "%lld:%02lld", m, s);
    return (negative ? "-" : "") + std::string(buffer);
}

} // namespace nuvio::data::player
