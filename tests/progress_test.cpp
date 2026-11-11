#include <doctest.h>

#include <memory>

#include "core/storage/store.hpp"
#include "data/progress/progress_repository.hpp"
#include "data/progress/watch_progress.hpp"
#include "test_util.hpp"

using namespace nuvio::data::progress;
using nuvio::core::storage::Store;

namespace
{
WatchProgress movie(const std::string& id, int64_t position, int64_t duration, int64_t lastWatched = 1)
{
    WatchProgress p;
    p.contentId   = id;
    p.contentType = "movie";
    p.name        = "Film " + id;
    p.videoId     = id;
    p.position    = position;
    p.duration    = duration;
    p.lastWatched = lastWatched;
    return p;
}

WatchProgress episode(const std::string& id, int season, int number, int64_t position, int64_t duration,
    int64_t lastWatched = 1)
{
    WatchProgress p = movie(id, position, duration, lastWatched);
    p.contentType   = "series";
    p.season        = season;
    p.episode       = number;
    p.videoId       = id + ":" + std::to_string(season) + ":" + std::to_string(number);
    return p;
}

struct Stores
{
    TempDir dir;
    std::shared_ptr<Store> recent  = std::make_shared<Store>(dir.path() + "watch_progress_recent.json");
    std::shared_ptr<Store> archive = std::make_shared<Store>(dir.path() + "watch_progress_archive.json");
    std::shared_ptr<Store> watched = std::make_shared<Store>(dir.path() + "watched_items.json");
    std::shared_ptr<Store> hidden  = std::make_shared<Store>(dir.path() + "cw_hidden.json");
    int64_t clock                  = 1000;

    // queued writes land before the folder goes
    ~Stores() { Store::flush(); }

    ProgressRepository::Config config()
    {
        return {recent, archive, watched, hidden, [this] { return clock; }};
    }
};
} // namespace

TEST_CASE("progress keys")
{
    CHECK(progressKey("tt0944947", 1, 3) == "tt0944947_s1e3");
    CHECK(progressKey("tt0111161", std::nullopt, std::nullopt) == "tt0111161");
    CHECK(progressKey("tt1", 1, std::nullopt) == "tt1");
}

TEST_CASE("progress thresholds")
{
    CHECK(isCompleted(movie("a", 5400000, 6000000)));
    CHECK_FALSE(isInProgress(movie("a", 5400000, 6000000)));
    CHECK_FALSE(isInProgress(movie("a", 60000, 6000000))); // 1%
    CHECK(isInProgress(movie("a", 120000, 6000000)));      // 2%
    CHECK(isInProgress(movie("a", 5399000, 6000000)));
    CHECK(progressPercentage(movie("a", 100, 0)) == 0);
    CHECK(progressPercentage(movie("a", 900, 600)) == 1);
    CHECK_FALSE(isInProgress(movie("a", 300000, 0))); // unknown duration
}

TEST_CASE("resume position")
{
    CHECK(resumePositionMs(movie("a", 2700000, 7200000)) == std::optional<int64_t>(2700000));
    CHECK_FALSE(resumePositionMs(movie("a", 60000, 7200000)));   // below 2%
    CHECK_FALSE(resumePositionMs(movie("a", 7000000, 7200000))); // completed
    CHECK_FALSE(resumePositionMs(std::nullopt));
}

TEST_CASE("upsert keeps artwork and drops the series-level entry")
{
    ProgressMap map;
    WatchProgress first = episode("tt1", 1, 1, 100000, 3000000);
    first.poster        = "poster";
    first.logo          = "logo";
    first.episodeTitle  = "Pilot";
    upsertProgress(map, first);

    WatchProgress update = episode("tt1", 1, 1, 200000, 3000000);
    update.name.clear();
    upsertProgress(map, update);
    const auto& stored = map.at("tt1_s1e1");
    CHECK(stored.position == 200000);
    CHECK(stored.poster == std::optional<std::string>("poster"));
    CHECK(stored.logo == std::optional<std::string>("logo"));
    CHECK(stored.episodeTitle == std::optional<std::string>("Pilot"));
    CHECK(stored.name == "Film tt1");

    map["tt1"] = movie("tt1", 1, 2);
    map["tt1"].contentType = "series";
    upsertProgress(map, episode("tt1", 1, 2, 1, 2));
    CHECK_FALSE(map.count("tt1"));
    CHECK(map.count("tt1_s1e2"));
}

TEST_CASE("bucket split")
{
    ProgressMap map;
    for (int i = 0; i < 201; i++)
        map["m" + std::to_string(i)] = movie("m" + std::to_string(i), 1, 2, 100 + i);
    auto buckets = splitBuckets(map);
    CHECK(buckets.recent.size() == 200);
    CHECK(buckets.archive.size() == 1);
    CHECK(buckets.archive.count("m0")); // the oldest
    CHECK(mergeBuckets(buckets.recent, buckets.archive).size() == 201);

    // ties go by key, descending
    ProgressMap tie{{"a", movie("a", 1, 2, 5)}, {"b", movie("b", 1, 2, 5)}};
    auto split = splitBuckets(tie, 1);
    CHECK(split.recent.count("b"));
    CHECK(split.archive.count("a"));

    // recent wins over archive
    ProgressMap recent{{"a", movie("a", 10, 20)}};
    ProgressMap archive{{"a", movie("a", 1, 20)}};
    CHECK(mergeBuckets(recent, archive).at("a").position == 10);
}

TEST_CASE("progress JSON uses NuvioTV's field names")
{
    WatchProgress p = episode("tt1", 2, 3, 500, 1000, 42);
    p.addonBaseUrl  = "https://addon";
    auto json       = toJson(p);
    CHECK(json["contentId"] == "tt1");
    CHECK(json["season"] == 2);
    CHECK(json["lastWatched"] == 42);
    CHECK(json["poster"].is_null());
    auto back = progressFromJson(json);
    REQUIRE(back);
    CHECK(back->videoId == "tt1:2:3");
    CHECK(back->addonBaseUrl == std::optional<std::string>("https://addon"));
    CHECK_FALSE(progressFromJson(nlohmann::json::object()));
}

TEST_CASE("the repository turns a completed save into a watched record")
{
    Stores stores;
    ProgressRepository repo(stores.config());
    int changes = 0;
    repo.subscribe([&changes] { changes++; });

    repo.save(episode("tt1", 1, 3, 1000000, 3000000, 10));
    CHECK(repo.get("tt1_s1e3"));

    repo.save(episode("tt1", 1, 3, 2760000, 3000000, 20)); // 92%
    CHECK_FALSE(repo.get("tt1_s1e3"));
    CHECK(repo.isWatched("tt1", 1, 3));
    REQUIRE(repo.watched().size() == 1);
    CHECK(repo.watched()[0].watchedAt == 20);
    CHECK(changes == 2);

    repo.markUnwatched("tt1", 1, 3);
    CHECK_FALSE(repo.isWatched("tt1", 1, 3));
}

TEST_CASE("the repository reloads what it wrote")
{
    Stores stores;
    {
        ProgressRepository repo(stores.config());
        repo.save(movie("tt9", 600000, 6000000, 5));
        repo.markWatched({"tt2", "movie", "Two", std::nullopt, std::nullopt, 7, std::nullopt});
        repo.hideSeries("tt3");
        Store::flush();
    }
    Stores again;
    again.recent  = std::make_shared<Store>(stores.recent->path());
    again.archive = std::make_shared<Store>(stores.archive->path());
    again.watched = std::make_shared<Store>(stores.watched->path());
    again.hidden  = std::make_shared<Store>(stores.hidden->path());
    ProgressRepository repo(again.config());
    CHECK(repo.get("tt9"));
    CHECK(repo.isWatched("tt2", std::nullopt, std::nullopt));
    CHECK(repo.hiddenSeries().at("tt3") == 1000);
}
