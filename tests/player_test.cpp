#include <doctest.h>

#include <chrono>
#include <mutex>
#include <thread>

#include "data/player/player_rules.hpp"
#include "data/player/subtitle_repository.hpp"

using namespace nuvio;
using namespace nuvio::data::player;
using domain::Addon;
using domain::AddonResource;

namespace
{
Addon subtitleAddon(const std::string& baseUrl, std::vector<std::string> types = {}, std::vector<std::string> prefixes = {},
    const std::string& resourceName = "subtitles")
{
    Addon addon;
    addon.baseUrl     = baseUrl;
    addon.displayName = baseUrl;
    AddonResource resource;
    resource.name       = resourceName;
    resource.types      = std::move(types);
    resource.idPrefixes = std::move(prefixes);
    addon.resources.push_back(resource);
    return addon;
}
} // namespace

TEST_CASE("headerFields escapes, sorts and drops blanks")
{
    CHECK(headerFields({{"User-Agent", "A, B"}, {"Cookie", "x=1"}}) == "Cookie: x=1,User-Agent: A\\, B");
    CHECK(headerFields({}).empty());
    CHECK(headerFields({{"A", ""}, {" ", "x"}, {"B", "y"}}) == "B: y");
    CHECK(headerFields({{"x", "1"}, {"B", "2"}, {"a", "3"}}) == "a: 3,B: 2,x: 1");
    CHECK(headerFields({{"K", "a\\b"}}) == "K: a\\\\b");
}

TEST_CASE("loadfileArgs puts the start option after the index")
{
    CHECK(loadfileArgs("u", std::nullopt) == std::vector<std::string>{"loadfile", "u", "replace"});
    CHECK(loadfileArgs("u", 0) == std::vector<std::string>{"loadfile", "u", "replace"});
    CHECK(loadfileArgs("u", 600000) == std::vector<std::string>{"loadfile", "u", "replace", "-1", "start=+600.000"});
    CHECK(loadfileArgs("u", 1500) == std::vector<std::string>{"loadfile", "u", "replace", "-1", "start=+1.500"});
}

TEST_CASE("seekStepMs follows the repeat thresholds")
{
    for (int i : {0, 1, 2})
        CHECK(seekStepMs(i) == 10000);
    for (int i : {3, 4, 7})
        CHECK(seekStepMs(i) == 20000);
    for (int i : {8, 14})
        CHECK(seekStepMs(i) == 30000);
    for (int i : {15, 16, 100})
        CHECK(seekStepMs(i) == 60000);
    CHECK(seekStepMs(-1) == 10000);
}

TEST_CASE("subtitleUrl")
{
    CHECK(subtitleUrl("https://opensubtitles-v3.strem.io", "series", "tt0903747", "tt0903747:2:3", "abc", 123, std::nullopt) ==
          "https://opensubtitles-v3.strem.io/subtitles/series/tt0903747%3A2%3A3/videoHash=abc&videoSize=123.json");
    CHECK(subtitleUrl("https://a/cfg?x=1", "tv", "tt1", "tt1:1:1", std::nullopt, std::nullopt, "My File.mkv") ==
          "https://a/cfg/subtitles/series/tt1%3A1%3A1/filename=My%20File.mkv.json?x=1");
    CHECK(subtitleUrl("https://a/", "movie", "tt1", "tt1", std::nullopt, std::nullopt, std::nullopt) ==
          "https://a/subtitles/movie/tt1.json");
    CHECK(subtitleUrl("https://a", "Movie", "tt1", "", std::nullopt, std::nullopt, std::nullopt) ==
          "https://a/subtitles/movie/tt1.json");
}

TEST_CASE("parseTrackList")
{
    auto tracks = parseTrackList(R"([
        {"id":1,"type":"video","codec":"h264","selected":true},
        {"id":1,"type":"audio","title":"Main","lang":"eng","codec":"aac","demux-channel-count":6,"selected":true},
        {"id":2,"type":"sub","lang":"fra","external":true,"external-filename":"https://s/fr.srt"},
        {"type":"sub"}, 5])");
    REQUIRE(tracks);
    REQUIRE(tracks->size() == 3);
    CHECK((*tracks)[1].channels == 6);
    CHECK((*tracks)[1].selected);
    CHECK((*tracks)[2].external);
    CHECK((*tracks)[2].externalFilename == "https://s/fr.srt");
    CHECK_FALSE(parseTrackList("{}"));
    CHECK_FALSE(parseTrackList("nope"));
}

TEST_CASE("languageName")
{
    CHECK(languageName("eng") == "English");
    CHECK(languageName("EN") == "English");
    CHECK(languageName("pob") == "Portuguese (Brazil)");
    CHECK(languageName("xyz") == "XYZ");
    CHECK(languageName("") == "Unknown");
}

TEST_CASE("formatTime")
{
    CHECK(formatTime(2710) == "45:10");
    CHECK(formatTime(-2690) == "-44:50");
    CHECK(formatTime(3725) == "1:02:05");
    CHECK(formatTime(0) == "0:00");
    CHECK(formatTime(59.9) == "0:59");
}

TEST_CASE("subtitleAddons filters by resource, type and prefix")
{
    std::vector<Addon> addons = {subtitleAddon("https://a", {"series"}, {"tt"}), subtitleAddon("https://b", {"movie"}),
        subtitleAddon("https://c", {}, {"kitsu:"}), subtitleAddon("https://d", {"tv"}, {}, "subtitle")};
    addons.push_back(subtitleAddon("https://e"));
    addons.back().enabled = false;
    addons.push_back(Addon{});
    addons.back().baseUrl = "https://nosubs";

    std::vector<std::string> names;
    for (const auto& a : subtitleAddons(addons, "tv", "tt1:1:1"))
        names.push_back(a.baseUrl);
    CHECK(names == std::vector<std::string>{"https://a", "https://d"});
}

TEST_CASE("parseSubtitleResponse")
{
    auto subs = parseSubtitleResponse(R"({"subtitles":[{"id":"1","url":"https://s/a.srt","lang":"eng"},{"url":""},
        {"url":"https://s/b.srt","language":"fra","headers":{"X":"y"}},{"url":"https://s/c.srt"}]})", "Add");
    REQUIRE(subs);
    REQUIRE(subs->size() == 3);
    CHECK((*subs)[0].id == "1");
    CHECK((*subs)[1].lang == "fra");
    CHECK((*subs)[1].headers.at("X") == "y");
    CHECK((*subs)[2].lang == "und");
    CHECK((*subs)[2].addonName == "Add");
    CHECK_FALSE(parseSubtitleResponse("x", "A"));
    CHECK(parseSubtitleResponse("{}", "A")->empty());
}

namespace
{
struct Fixture
{
    std::mutex mutex;
    std::vector<std::function<void()>> ui;
    std::vector<std::string> requested;
    std::map<std::string, core::net::HttpResult> responses;
    std::unique_ptr<core::net::HttpClient> http;
    std::unique_ptr<SubtitleRepository> repo;

    Fixture()
    {
        core::net::HttpClient::Config config;
        config.threads   = 1;
        config.transport = [this](const core::net::Request& request, const std::atomic<bool>&)
        {
            std::lock_guard<std::mutex> lock(mutex);
            requested.push_back(request.url);
            auto it = responses.find(request.url);
            if (it != responses.end())
                return it->second;
            core::net::HttpResult failed;
            failed.error        = core::net::HttpError::Timeout;
            failed.errorMessage = "timeout";
            return failed;
        };
        config.post = [this](std::function<void()> fn)
        {
            std::lock_guard<std::mutex> lock(mutex);
            ui.push_back(std::move(fn));
        };
        http = std::make_unique<core::net::HttpClient>(config);
        repo = std::make_unique<SubtitleRepository>(SubtitleRepository::Config{http.get()});
    }

    template <class Done> bool await(Done done)
    {
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (!done() && std::chrono::steady_clock::now() < deadline)
        {
            std::vector<std::function<void()>> pending;
            {
                std::lock_guard<std::mutex> lock(mutex);
                pending.swap(ui);
            }
            for (auto& fn : pending)
                fn();
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        return done();
    }
};
} // namespace

TEST_CASE("SubtitleRepository merges the stream's subtitles with the add-ons' and survives a failing add-on")
{
    Fixture f;
    core::net::HttpResult ok;
    ok.ok     = true;
    ok.status = 200;
    ok.body   = R"({"subtitles":[{"id":"x","url":"https://s/en.srt","lang":"eng"}]})";
    f.responses["https://a/subtitles/series/tt1%3A1%3A2/videoHash=h.json"] = ok;

    domain::PlaybackRequest request;
    request.contentType = "series";
    request.contentId   = "tt1";
    request.videoId     = "tt1:1:2";
    request.videoHash   = "h";
    request.addonName   = "Streamer";
    request.subtitles.push_back({"own", "https://s/own.srt", "eng", {}});

    std::vector<size_t> updates;
    bool done = false;
    f.repo->fetchAll(request, {subtitleAddon("https://a"), subtitleAddon("https://b")},
        [&updates](const std::vector<domain::Subtitle>& all) { updates.push_back(all.size()); }, [&done] { done = true; });
    REQUIRE(f.await([&] { return done; }));

    CHECK(updates == std::vector<size_t>{1, 2});
}

TEST_CASE("SubtitleRepository with no subtitles add-on finishes at once")
{
    Fixture f;
    bool done = false;
    f.repo->fetchAll(domain::PlaybackRequest{}, {}, nullptr, [&done] { done = true; });
    CHECK(done);
}
