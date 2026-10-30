#include <doctest.h>

#include <chrono>
#include <mutex>
#include <thread>

#include "data/meta/meta_cache.hpp"
#include "data/meta/meta_candidates.hpp"
#include "data/meta/meta_parser.hpp"
#include "data/meta/meta_repository.hpp"
#include "domain/meta.hpp"

using namespace nuvio;
using namespace nuvio::data::meta;
using domain::Addon;
using domain::AddonResource;
using std::chrono::minutes;
using std::chrono::seconds;

namespace
{
Addon metaAddon(const std::string& baseUrl, std::vector<std::string> types, std::vector<std::string> prefixes = {})
{
    Addon addon;
    addon.baseUrl = baseUrl;
    addon.name    = baseUrl;
    AddonResource resource;
    resource.name       = "meta";
    resource.types      = std::move(types);
    resource.idPrefixes = std::move(prefixes);
    addon.resources.push_back(resource);
    return addon;
}

// a string literal converts to both parseMeta overloads
std::optional<domain::Meta> parse(const std::string& body, const std::string& type, const std::string& addon)
{
    return parseMeta(body, type, addon);
}

std::vector<std::string> urls(const std::vector<MetaCandidate>& list)
{
    std::vector<std::string> out;
    for (const auto& c : list)
        out.push_back(c.addon.baseUrl + " " + c.type);
    return out;
}
} // namespace

TEST_CASE("inferType")
{
    CHECK(inferType("tv", "x") == "series");
    CHECK(inferType("TV", "x") == "series");
    CHECK(inferType("movie", "tt1") == "movie");
    CHECK(inferType("other", "x:movie:1") == "movie");
    CHECK(inferType("other", "x:tv:1") == "series");
    CHECK(inferType("other", "x:series:1") == "series");
    CHECK(inferType("other", "x:ANIME:1") == "anime");
    CHECK(inferType("other", "plain") == "other");
}

TEST_CASE("candidates: a prefix match wins over add-on order")
{
    std::vector<Addon> addons = {metaAddon("https://a", {"series"}, {"kitsu:"}), metaAddon("https://b", {"series"}, {"tt"})};
    CHECK(urls(candidates(addons, "series", "tt0903747")) == std::vector<std::string>{"https://b series"});
}

TEST_CASE("candidates: add-on order, de-duplication and case")
{
    std::vector<Addon> addons = {metaAddon("https://a", {"Movie"}, {"TT"}), metaAddon("https://b", {}, {})};
    CHECK(urls(candidates(addons, "movie", "tt1")) == std::vector<std::string>{"https://a movie", "https://b movie"});
}

TEST_CASE("candidates: tv is asked as series")
{
    std::vector<Addon> addons = {metaAddon("https://a", {"series"}, {"tt"})};
    CHECK(urls(candidates(addons, "tv", "tt1")) == std::vector<std::string>{"https://a series"});
}

TEST_CASE("candidates: disabled add-ons and add-ons without meta are skipped")
{
    Addon off = metaAddon("https://off", {"movie"});
    off.enabled = false;
    Addon catalogOnly;
    catalogOnly.baseUrl = "https://catalog";
    catalogOnly.resources.push_back({"catalog", {"movie"}, {}});
    std::vector<Addon> addons = {off, catalogOnly, metaAddon("https://a", {"movie"})};
    CHECK(urls(candidates(addons, "movie", "tt1")) == std::vector<std::string>{"https://a movie"});
}

TEST_CASE("candidates: fall back to add-ons without prefixes, then to manifest types")
{
    // the requested type isn't in the resource, so steps 1 to 3 find nothing; the add-on lists it in "types"
    Addon byManifest = metaAddon("https://m", {"series"});
    byManifest.types = {"movie"};
    CHECK(urls(candidates({byManifest}, "movie", "tt1")) == std::vector<std::string>{"https://m movie"});

    // a prefixed add-on that doesn't match; the one without prefixes answers
    std::vector<Addon> addons = {metaAddon("https://p", {"movie"}, {"kitsu:"}), metaAddon("https://n", {"movie"})};
    CHECK(urls(candidates(addons, "movie", "tt1")) == std::vector<std::string>{"https://n movie"});
}

TEST_CASE("candidates: nothing matches")
{
    CHECK(candidates({}, "movie", "tt1").empty());
    CHECK(candidates({metaAddon("https://a", {"series"}, {"kitsu:"})}, "movie", "tt1").empty());
}

TEST_CASE("buildMetaUrl encodes the path and keeps the base query")
{
    CHECK(buildMetaUrl("https://x.io/cfg?token=a", "movie", "tt1160419") == "https://x.io/cfg/meta/movie/tt1160419.json?token=a");
    CHECK(buildMetaUrl("https://x.io/", "series", "kitsu:12 3") == "https://x.io/meta/series/kitsu%3A12%203.json");
}

TEST_CASE("metaTtl follows Cache-Control with a 5 minute floor and a 6 hour default")
{
    auto ttl = [](const std::string& value)
    {
        core::net::Headers headers;
        if (!value.empty())
            headers["cache-control"] = value;
        return metaTtl(headers);
    };
    CHECK(ttl("") == seconds(6 * 3600));
    CHECK(ttl("public") == seconds(6 * 3600));
    CHECK(ttl("max-age=3600") == seconds(3600));
    CHECK(ttl("max-age=10") == minutes(5));
    CHECK(ttl("max-age=0") == minutes(5));
    CHECK(ttl("no-store") == minutes(5));
    CHECK(ttl("no-cache, max-age=86400") == minutes(5));
    CHECK(ttl("max-age=60, s-maxage=7200") == seconds(7200));
}

TEST_CASE("MetaCache evicts the least recently used and expires entries")
{
    auto now = Clock::now();
    auto meta = [](const std::string& id)
    {
        domain::Meta m;
        m.id = id;
        return m;
    };

    MetaCache cache(2);
    cache.put("a", meta("a"), minutes(10), now);
    cache.put("b", meta("b"), minutes(10), now);
    REQUIRE(cache.get("a", now)); // a is now the most recent
    cache.put("c", meta("c"), minutes(10), now);
    CHECK(cache.get("a", now));
    CHECK_FALSE(cache.get("b", now));
    CHECK(cache.get("c", now));

    CHECK(cache.get("a", now + minutes(9)));
    CHECK_FALSE(cache.get("a", now + minutes(10)));
    CHECK(cache.size() == 1);
}

TEST_CASE("parseMeta reads aliases and keeps inline streams raw")
{
    auto meta = parse(R"({"meta":{"id":"tt1","type":"series","name":"Show","imdbRating":"8.7","runtime":"45 min",
        "genres":["Drama"],"director":"A, B","cast":[{"name":"C"},"D"],
        "behaviorHints":{"defaultVideoId":"tt1:1:2"},
        "videos":[{"id":"tt1:1:1","name":"Pilot","number":"1","season":1,"description":"First","released":"2020-01-01"},
                  {"id":"tt1:1:2","title":"Two","season":"1","episode":2,"streams":[{"url":"http://x"}]},
                  {"title":"no id"}]}})",
        "series", "https://a");
    REQUIRE(meta);
    CHECK(meta->name == "Show");
    CHECK(meta->imdbRating == doctest::Approx(8.7f));
    CHECK(meta->runtime == "45 min");
    CHECK(meta->genres == std::vector<std::string>{"Drama"});
    CHECK(meta->cast == std::vector<std::string>{"C", "D"});
    CHECK(meta->defaultVideoId == "tt1:1:2");
    CHECK(meta->addonBaseUrl == "https://a");
    REQUIRE(meta->videos.size() == 2);
    CHECK(meta->videos[0].title == "Pilot");
    CHECK(meta->videos[0].episode == 1);
    CHECK(meta->videos[0].overview == "First");
    CHECK(meta->videos[1].season == 1);
    CHECK(meta->videos[1].streamsJson == R"([{"url":"http://x"}])");
}

TEST_CASE("parseMeta rejects missing or unusable meta")
{
    CHECK_FALSE(parse("not json", "movie", ""));
    CHECK_FALSE(parse(R"({"meta":null})", "movie", ""));
    CHECK_FALSE(parse(R"({"meta":{"name":"x"}})", "movie", ""));
    auto meta = parse(R"({"meta":{"id":"tt1","name":"M"}})", "movie", "");
    REQUIRE(meta);
    CHECK(meta->type == "movie");
}

namespace
{
domain::Video video(const std::string& id, std::optional<int> season, std::optional<int> episode, std::optional<bool> available = std::nullopt)
{
    domain::Video v;
    v.id        = id;
    v.season    = season;
    v.episode   = episode;
    v.available = available;
    return v;
}
} // namespace

TEST_CASE("groupSeasons sorts seasons and episodes")
{
    auto seasons = domain::groupSeasons({video("c", 2, 1), video("b", 1, 2), video("a", 1, 1), video("s", 0, 1)});
    REQUIRE(seasons.size() == 3);
    CHECK(seasons[0].number == 1);
    CHECK(seasons[0].episodes[0].id == "a");
    CHECK(seasons[0].episodes[1].id == "b");
    CHECK(seasons[1].episodes[0].id == "c");
    CHECK(seasons[2].number == 0); // specials last
}

TEST_CASE("groupSeasons makes one virtual season without season numbers")
{
    auto seasons = domain::groupSeasons({video("x", std::nullopt, std::nullopt), video("y", std::nullopt, std::nullopt)});
    REQUIRE(seasons.size() == 1);
    CHECK_FALSE(seasons[0].number);
    CHECK(seasons[0].episodes.size() == 2);
    CHECK(seasons[0].episodes[0].id == "x"); // add-on order kept
    CHECK(domain::groupSeasons({}).empty());
}

TEST_CASE("initialSeasonIndex")
{
    auto seasons = domain::groupSeasons({video("s", 0, 1), video("a", 1, 1), video("b", 2, 1), video("c", 2, 2, false)});
    // order is 1, 2, specials
    CHECK(domain::initialSeasonIndex(seasons, std::nullopt) == 0);     // specials are skipped
    CHECK(domain::initialSeasonIndex(seasons, "b") == 1);              // the default video's season
    CHECK(domain::initialSeasonIndex(seasons, "s") == 2);
    CHECK(domain::initialSeasonIndex(seasons, "c") == 0);              // unavailable: ignored
    CHECK(domain::initialSeasonIndex(seasons, "missing") == 0);
    auto onlySpecials = domain::groupSeasons({video("s", 0, 1)});
    CHECK(domain::initialSeasonIndex(onlySpecials, std::nullopt) == 0);
    CHECK(domain::initialSeasonIndex({}, std::nullopt) == 0);
}

TEST_CASE("playVideoId")
{
    domain::Meta movie;
    movie.id   = "tt1160419";
    movie.type = "movie";
    CHECK(domain::playVideoId(movie, nullptr) == "tt1160419");
    movie.defaultVideoId = "tt1160419:x";
    CHECK(domain::playVideoId(movie, nullptr) == "tt1160419:x");

    domain::Meta series;
    series.id     = "tt0903747";
    series.type   = "series";
    series.videos = {video("tt0903747:2:1", 2, 1), video("tt0903747:2:2", 2, 2)};
    auto seasons  = domain::groupSeasons(series.videos);
    CHECK(domain::playVideoId(series, &seasons[0]) == "tt0903747:2:1");
    CHECK(domain::playVideoId(series, nullptr) == "tt0903747");
}

namespace
{
const char* const META_BODY = R"({"meta":{"id":"tt1","type":"movie","name":"Movie"}})";

struct RepoFixture
{
    std::mutex mutex;
    std::vector<std::function<void()>> ui;
    std::vector<std::string> requested;
    std::map<std::string, core::net::HttpResult> responses; // by URL; a missing URL is a 404
    std::unique_ptr<core::net::HttpClient> http;
    Clock::time_point now = Clock::now();
    std::unique_ptr<MetaRepository> repo;

    RepoFixture()
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
            core::net::HttpResult notFound;
            notFound.status = 404;
            notFound.error  = core::net::HttpError::Http;
            return notFound;
        };
        config.post = [this](std::function<void()> fn)
        {
            std::lock_guard<std::mutex> lock(mutex);
            ui.push_back(std::move(fn));
        };
        http = std::make_unique<core::net::HttpClient>(config);
        MetaRepository::Config repoConfig;
        repoConfig.http = http.get();
        repoConfig.now  = [this] { return now; };
        repo            = std::make_unique<MetaRepository>(repoConfig);
    }

    void ok(const std::string& url, const std::string& body = META_BODY, const std::string& cacheControl = "")
    {
        core::net::HttpResult result;
        result.ok     = true;
        result.status = 200;
        result.body   = body;
        if (!cacheControl.empty())
            result.headers["cache-control"] = cacheControl;
        responses[url] = result;
    }

    // Runs posted callbacks until done() holds.
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

    size_t requestCount()
    {
        std::lock_guard<std::mutex> lock(mutex);
        return requested.size();
    }
};
} // namespace

TEST_CASE("MetaRepository uses the next candidate after a failure")
{
    RepoFixture f;
    f.ok("https://b/meta/movie/tt1.json");
    std::vector<Addon> addons = {metaAddon("https://a", {"movie"}), metaAddon("https://b", {"movie"})};

    std::optional<MetaRepository::Result> result;
    f.repo->lookup("movie", "tt1", "", addons, [&result](MetaRepository::Result r) { result = std::move(r); });
    REQUIRE(f.await([&] { return result.has_value(); }));
    CHECK(result->ok);
    CHECK(result->meta.name == "Movie");
    CHECK(result->meta.addonBaseUrl == "https://b");
    CHECK(f.requested == std::vector<std::string>{"https://a/meta/movie/tt1.json", "https://b/meta/movie/tt1.json"});
}

TEST_CASE("MetaRepository falls back to the source add-on")
{
    RepoFixture f;
    f.ok("https://src/meta/movie/tt1.json");
    // the source add-on has no meta resource, so only the fallback reaches it
    Addon source;
    source.baseUrl = "https://src";
    std::vector<Addon> addons = {metaAddon("https://a", {"series"}, {"kitsu:"}), source};

    std::optional<MetaRepository::Result> result;
    f.repo->lookup("movie", "tt1", "https://src/", addons, [&result](MetaRepository::Result r) { result = std::move(r); });
    REQUIRE(f.await([&] { return result.has_value(); }));
    CHECK(result->ok);
    CHECK(f.requested == std::vector<std::string>{"https://src/meta/movie/tt1.json"});
}

TEST_CASE("MetaRepository reports a title found nowhere, and Retry looks again")
{
    RepoFixture f;
    std::vector<Addon> addons = {metaAddon("https://a", {"movie"})};

    std::optional<MetaRepository::Result> result;
    f.repo->lookup("movie", "tt1", "", addons, [&result](MetaRepository::Result r) { result = std::move(r); });
    REQUIRE(f.await([&] { return result.has_value(); }));
    CHECK_FALSE(result->ok);

    f.ok("https://a/meta/movie/tt1.json");
    result.reset();
    f.repo->lookup("movie", "tt1", "", addons, [&result](MetaRepository::Result r) { result = std::move(r); });
    REQUIRE(f.await([&] { return result.has_value(); }));
    CHECK(result->ok);
}

TEST_CASE("MetaRepository caches until the TTL and shares in-flight lookups")
{
    RepoFixture f;
    f.ok("https://a/meta/movie/tt1.json", META_BODY, "no-store");
    std::vector<Addon> addons = {metaAddon("https://a", {"movie"})};

    int calls = 0;
    auto count = [&calls](MetaRepository::Result) { calls++; };
    f.repo->lookup("movie", "tt1", "", addons, count);
    f.repo->lookup("movie", "tt1", "", addons, count); // joins the first
    REQUIRE(f.await([&] { return calls == 2; }));
    CHECK(f.requestCount() == 1);

    f.repo->lookup("movie", "tt1", "", addons, count); // cached: answered at once
    CHECK(calls == 3);
    CHECK(f.requestCount() == 1);

    f.now += minutes(4); // no-store still holds for 5 minutes
    f.repo->lookup("movie", "tt1", "", addons, count);
    CHECK(calls == 4);
    CHECK(f.requestCount() == 1);

    f.now += minutes(2);
    f.repo->lookup("movie", "tt1", "", addons, count);
    REQUIRE(f.await([&] { return calls == 5; }));
    CHECK(f.requestCount() == 2);
}

TEST_CASE("MetaRepository cancel drops the callback")
{
    RepoFixture f;
    f.ok("https://a/meta/movie/tt1.json");
    std::vector<Addon> addons = {metaAddon("https://a", {"movie"})};

    bool called = false;
    auto ticket = f.repo->lookup("movie", "tt1", "", addons, [&called](MetaRepository::Result) { called = true; });
    f.repo->cancel(ticket);
    CHECK(f.await([&] { return f.requestCount() == 1; }));
    f.await([] { return false; }); // let the posted result run
    CHECK_FALSE(called);
}

#include "domain/meta_format.hpp"
#include "domain/stream_request.hpp"

TEST_CASE("episodeHeading and releaseDate")
{
    domain::Video v = video("x", 2, 3);
    v.title         = "Pilot";
    CHECK(domain::episodeHeading(v) == "S2E3 · Pilot");
    v.title = "";
    CHECK(domain::episodeHeading(v) == "S2E3");
    v.season = std::nullopt;
    v.title  = "Pilot";
    CHECK(domain::episodeHeading(v) == "E3 · Pilot");
    CHECK(domain::episodeHeading(video("y", std::nullopt, std::nullopt)) == "");

    CHECK(domain::releaseDate("2020-01-02T00:00:00.000Z") == "2020-01-02");
    CHECK(domain::releaseDate("Jan 2020") == "Jan 2020");
    CHECK(domain::releaseDate(std::nullopt) == "");
}

TEST_CASE("infoLine skips missing fields")
{
    domain::Meta meta;
    CHECK(domain::infoLine(meta) == "");
    meta.releaseInfo = "2008–2013";
    meta.imdbRating  = 9.5f;
    CHECK(domain::infoLine(meta) == "2008–2013  ·  IMDb 9.5");
    meta.runtime = "47 min";
    CHECK(domain::infoLine(meta) == "2008–2013  ·  47 min  ·  IMDb 9.5");
    CHECK(domain::shorten("héllo wörld", 2) == "h…"); // cut on a UTF-8 boundary
}

TEST_CASE("stream requests for Play and for an episode")
{
    domain::Meta series;
    series.id         = "tt0903747";
    series.type       = "series";
    series.name       = "Breaking Bad";
    series.background = "https://bg";
    domain::Video e   = video("tt0903747:2:3", 2, 3);
    e.title           = "Bit by a Dead Bee";
    series.videos     = {video("tt0903747:2:1", 2, 1), e};
    auto seasons      = domain::groupSeasons(series.videos);

    auto episode = domain::streamRequestForEpisode(series, e);
    CHECK(episode.videoId == "tt0903747:2:3");
    CHECK(episode.contentId == "tt0903747");
    CHECK(episode.season == 2);
    CHECK(episode.episode == 3);
    CHECK(episode.episodeTitle == "Bit by a Dead Bee");
    CHECK(episode.backdrop == "https://bg");

    auto play = domain::streamRequestForPlay(series, &seasons[0]);
    CHECK(play.videoId == "tt0903747:2:1");
    CHECK(play.episode == 1);

    domain::Meta movie;
    movie.id   = "tt1160419";
    movie.type = "movie";
    movie.name = "Dune";
    auto moviePlay = domain::streamRequestForPlay(movie, nullptr);
    CHECK(moviePlay.videoId == "tt1160419");
    CHECK_FALSE(moviePlay.season);
}
