#include <doctest.h>

#include "data/streams/stream_parser.hpp"
#include "data/streams/stream_rules.hpp"

using namespace nuvio;
using namespace nuvio::data::streams;
using domain::Addon;
using domain::AddonResource;
using domain::Stream;

namespace
{
Addon streamAddon(const std::string& baseUrl, std::vector<std::string> types, std::vector<std::string> prefixes,
    bool enabled = true)
{
    Addon addon;
    addon.baseUrl     = baseUrl;
    addon.displayName = baseUrl;
    addon.enabled     = enabled;
    AddonResource resource;
    resource.name       = "stream";
    resource.types      = std::move(types);
    resource.idPrefixes = std::move(prefixes);
    addon.resources.push_back(resource);
    return addon;
}

Stream parse(const std::string& json)
{
    auto streams = parseStreamResponse("{\"streams\":[" + json + "]}", "Addon");
    REQUIRE(streams);
    REQUIRE(streams->size() == 1);
    return streams->front();
}

std::vector<std::string> names(const std::vector<Addon>& addons)
{
    std::vector<std::string> out;
    for (const auto& a : addons)
        out.push_back(a.baseUrl);
    return out;
}
} // namespace

TEST_CASE("streamAddons filters by resource, type, prefix and enabled flag")
{
    std::vector<Addon> addons = {streamAddon("https://a", {"series"}, {"tt"}), streamAddon("https://b", {"series"}, {"kitsu:"}),
        streamAddon("https://c", {"movie"}, {}), streamAddon("https://d", {}, {}), streamAddon("https://e", {}, {}, false)};
    addons.push_back(Addon{});
    addons.back().baseUrl = "https://nostream";

    CHECK(names(streamAddons(addons, "series", "tt0903747:2:3")) == std::vector<std::string>{"https://a", "https://d"});
    CHECK(names(streamAddons(addons, "movie", "tt1")) == std::vector<std::string>{"https://c", "https://d"});
    CHECK(streamAddons({addons[1]}, "series", "tt1").empty());
}

TEST_CASE("buildStreamUrl encodes the video ID and puts the base query after .json")
{
    CHECK(buildStreamUrl("https://torrentio.example/cfg?x=1", "series", "tt0903747:2:3") ==
          "https://torrentio.example/cfg/stream/series/tt0903747%3A2%3A3.json?x=1");
    CHECK(buildStreamUrl("https://a/", "movie", "tt1") == "https://a/stream/movie/tt1.json");
    CHECK(buildStreamUrl("https://a", "other type", "a b") == "https://a/stream/other%20type/a%20b.json");
}

TEST_CASE("contentIdFromVideoId drops the season and episode")
{
    CHECK(contentIdFromVideoId("tt1234567:1:5") == "tt1234567");
    CHECK(contentIdFromVideoId("mal:63375:1:5") == "mal:63375");
    CHECK(contentIdFromVideoId("kitsu:12345:2") == "kitsu:12345");
    CHECK(contentIdFromVideoId("tt1234567") == "tt1234567");
    CHECK(contentIdFromVideoId("kitsu:12345") == "kitsu:12345");
    CHECK(contentIdFromVideoId("12345:1:2") == "12345");
    CHECK(contentIdFromVideoId("abc:def") == "abc:def");
}

TEST_CASE("parseStreamResponse reads fields, hints and subtitles")
{
    Stream s = parse(R"({"name":"Torrentio\n4K","title":"Movie.mkv\n12.3 GB","url":"https://cdn/x.mp4",
        "infoHash":"ABC","fileIdx":"3","behaviorHints":{"bingeGroup":"g","filename":"x.mp4","videoHash":"h","videoSize":123456,
        "proxyHeaders":{"request":{"Referer":"https://a.io","range":"bytes=0-"}}},
        "subtitles":[{"url":"https://s/en.srt","lang":"en"},{"url":" "},{"id":"x","url":"https://s/fr.srt"}]})");
    CHECK(*s.name == "Torrentio\n4K");
    CHECK(*s.url == "https://cdn/x.mp4");
    CHECK(*s.fileIdx == 3);
    CHECK(*s.behaviorHints.bingeGroup == "g");
    CHECK(*s.behaviorHints.videoSize == 123456);
    CHECK(s.behaviorHints.proxyRequestHeaders.size() == 2);
    REQUIRE(s.subtitles.size() == 2);
    CHECK(s.subtitles[0].id == "https://s/en.srt");
    CHECK(s.subtitles[1].lang == "Unknown");
    CHECK(s.addonName == "Addon");
}

TEST_CASE("parseStreamResponse tolerates odd responses")
{
    CHECK_FALSE(parseStreamResponse("not json", "A"));
    CHECK_FALSE(parseStreamResponse("[]", "A"));
    CHECK(parseStreamResponse("{}", "A")->empty());
    CHECK(parseStreamResponse(R"({"streams":null})", "A")->empty());
    CHECK(parseStreamResponse(R"({"streams":[1,"x",{"url":"https://a/b"}]})", "A")->size() == 1);
    CHECK(parseInlineStreams("", "A").empty());
    CHECK(parseInlineStreams(R"([{"url":"https://a/1"},{"url":"https://a/2"}])", "A").size() == 2);
}

TEST_CASE("dedupKey priority")
{
    CHECK(dedupKey(parse(R"({"infoHash":"AbC","fileIdx":2,"url":"https://a"})")) == "abc:2");
    CHECK(dedupKey(parse(R"({"infoHash":"AbC"})")) == "abc:");
    CHECK(dedupKey(parse(R"({"url":"https://a","name":"HD"})")) == "https://a|HD");
    CHECK(dedupKey(parse(R"({"url":"https://a"})")) == "https://a");
    CHECK(dedupKey(parse(R"({"externalUrl":"https://e","ytId":"y"})")) == "https://e");
    CHECK(dedupKey(parse(R"({"ytId":"y"})")) == "y");
    CHECK(dedupKey(parse(R"({"name":"N","title":"T"})")) == "Addon:N:T");
}

TEST_CASE("dedup keeps the first of each key")
{
    auto streams = parseStreamResponse(
        R"({"streams":[{"infoHash":"A","fileIdx":1,"name":"first"},{"infoHash":"a","fileIdx":1,"name":"second"},
        {"infoHash":"a","fileIdx":2},{"url":"https://u","name":"x"},{"url":"https://u","name":"y"}]})", "A");
    auto out = dedup(*streams);
    REQUIRE(out.size() == 4);
    CHECK(*out[0].name == "first");
}

TEST_CASE("classify")
{
    CHECK(classify(parse(R"({"url":"https://cdn.example/movie.mp4"})")) == Playability::Playable);
    CHECK(classify(parse(R"({"url":"http://cdn/movie.mp4","infoHash":"abc"})")) == Playability::Playable);
    CHECK(classify(parse(R"({"infoHash":"abc","fileIdx":1})")) == Playability::NeedsDebrid);
    CHECK(classify(parse(R"({"url":"magnet:?xt=urn:btih:abc"})")) == Playability::NeedsDebrid);
    CHECK(classify(parse(R"({"externalUrl":"magnet:?xt=urn:btih:abc"})")) == Playability::NeedsDebrid);
    CHECK(classify(parse(R"({"url":"https://x/y.torrent?k=1"})")) == Playability::NeedsDebrid);
    CHECK(classify(parse(R"({"url":"https://x/y.TORRENT"})")) == Playability::NeedsDebrid);
    CHECK(classify(parse(R"({"url":"torrent:0123/1"})")) == Playability::NeedsDebrid);
    CHECK(classify(parse(R"({"externalUrl":"https://site.example/watch"})")) == Playability::Unsupported);
    CHECK(classify(parse(R"({"ytId":"abc"})")) == Playability::Unsupported);
    CHECK(classify(parse(R"({"name":"x"})")) == Playability::Unsupported);
    CHECK_FALSE(playableUrl(parse(R"({"infoHash":"abc"})")));
}

TEST_CASE("requestHeaders removes Range in any case")
{
    Stream s = parse(R"({"url":"https://a","behaviorHints":{"proxyHeaders":{"request":{"Referer":"https://a.io","range":"bytes=0-","RANGE":"x"}}}})");
    auto headers = requestHeaders(s);
    REQUIRE(headers.size() == 1);
    CHECK(headers.at("Referer") == "https://a.io");
}

TEST_CASE("display name and description fall back")
{
    CHECK(displayName(parse(R"({"name":"Torrentio\n4K","title":"T"})")) == "Torrentio 4K");
    CHECK(displayName(parse(R"({"title":"T","description":"D"})")) == "T");
    CHECK(displayName(parse(R"({"description":"D"})")) == "D");
    CHECK(displayName(parse(R"({"url":"https://a"})")).empty());
    CHECK(displayDescription(parse(R"({"title":"T","description":"D"})")) == "D");
    CHECK(displayDescription(parse(R"({"title":"T"})")) == "T");
    CHECK(descriptionLines("a\n\n b \nc\nd", 3) == std::vector<std::string>{"a", "b", "c"});
}

TEST_CASE("formatSize")
{
    CHECK(formatSize(0).empty());
    CHECK(formatSize(850LL * 1024 * 1024) == "850 MB");
    CHECK(formatSize(13200000000LL) == "12.3 GB");
}

TEST_CASE("playbackRequest carries the stream and the title")
{
    Stream s = parse(R"({"url":"https://a/v.mkv","name":"HD","behaviorHints":{"bingeGroup":"bg","videoSize":10,
        "proxyHeaders":{"request":{"Referer":"https://a.io","Range":"bytes=0-"}}},"subtitles":[{"url":"https://s/en.srt","lang":"en"}]})");
    domain::StreamRequest request;
    request.contentType = "series";
    request.contentId   = "tt1";
    request.videoId     = "tt1:2:3";
    request.title       = "Show";
    request.season      = 2;
    request.episode     = 3;

    auto out = playbackRequest(s, request);
    REQUIRE(out);
    CHECK(out->url == "https://a/v.mkv");
    CHECK(out->headers.size() == 1);
    CHECK(out->subtitles.size() == 1);
    CHECK(*out->bingeGroup == "bg");
    CHECK(*out->videoSize == 10);
    CHECK(out->videoId == "tt1:2:3");
    CHECK(*out->season == 2);
    CHECK(out->streamName == "HD");

    CHECK_FALSE(playbackRequest(parse(R"({"infoHash":"abc"})"), request));
}
