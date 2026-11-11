#include <doctest.h>

#include <map>

#include "data/progress/continue_watching.hpp"

using namespace nuvio::data::progress;
using nuvio::domain::Meta;
using nuvio::domain::Video;

namespace
{
const std::string TODAY = "2026-11-11";

Video ep(int season, int number, std::optional<std::string> released = std::string("2020-01-01"))
{
    Video v;
    v.id       = "tt1:" + std::to_string(season) + ":" + std::to_string(number);
    v.title    = "E" + std::to_string(number);
    v.season   = season;
    v.episode  = number;
    v.released = std::move(released);
    return v;
}

WatchProgress progress(const std::string& id, std::optional<int> season, std::optional<int> episode, int64_t position,
    int64_t lastWatched)
{
    WatchProgress p;
    p.contentId   = id;
    p.contentType = season ? "series" : "movie";
    p.name        = id;
    p.videoId     = id;
    p.season      = season;
    p.episode     = episode;
    p.position    = position;
    p.duration    = 1000;
    p.lastWatched = lastWatched;
    return p;
}

WatchedItem watched(const std::string& id, int season, int episode, int64_t at)
{
    return {id, "series", id, season, episode, at, std::nullopt};
}

ProgressMap mapOf(std::vector<WatchProgress> list)
{
    ProgressMap map;
    for (auto& p : list)
        map[progressKey(p)] = p;
    return map;
}
} // namespace

TEST_CASE("release days")
{
    CHECK(releaseDay(std::string("2008-01-20T02:00:00.000Z")) == std::optional<std::string>("2008-01-20"));
    CHECK(releaseDay(std::string("2008-01-20")) == std::optional<std::string>("2008-01-20"));
    CHECK_FALSE(releaseDay(std::string("Jan 2008")));
    CHECK_FALSE(releaseDay(std::nullopt));
}

TEST_CASE("next-up: the next episode in the same season")
{
    auto next = resolveNextUp({ep(1, 4), ep(1, 3), ep(1, 5)}, 1, 3, "", TODAY);
    REQUIRE(next);
    CHECK(*next->episode == 4);
}

TEST_CASE("next-up: rolls over into an aired season")
{
    auto next = resolveNextUp({ep(1, 10), ep(2, 1)}, 1, 10, "", TODAY);
    REQUIRE(next);
    CHECK(*next->season == 2);
}

TEST_CASE("next-up: a season that hasn't aired gives nothing")
{
    CHECK_FALSE(resolveNextUp({ep(1, 10), ep(2, 1, std::string("2026-12-11"))}, 1, 10, "", TODAY));
}

TEST_CASE("next-up: episodes without a date are skipped")
{
    CHECK_FALSE(resolveNextUp({ep(1, 1), ep(1, 2, std::nullopt)}, 1, 1, "", TODAY));
    auto next = resolveNextUp({ep(1, 1), ep(1, 2, std::nullopt), ep(1, 3)}, 1, 1, "", TODAY);
    REQUIRE(next);
    CHECK(*next->episode == 3);
}

TEST_CASE("next-up: season 0 is left out, and the last episode has no next")
{
    auto next = resolveNextUp({ep(0, 1), ep(1, 1), ep(1, 2)}, 1, 1, "", TODAY);
    REQUIRE(next);
    CHECK(*next->episode == 2);
    CHECK_FALSE(resolveNextUp({ep(1, 1), ep(0, 1)}, 1, 1, "", TODAY));
    CHECK_FALSE(resolveNextUp({ep(1, 1)}, 3, 3, "", TODAY)); // seed not in the meta
    CHECK(resolveNextUp({ep(1, 1), ep(1, 2)}, 9, 9, "tt1:1:1", TODAY)); // found by video ID
}

TEST_CASE("in-progress items keep one episode per series")
{
    auto items = inProgressItems(mapOf({
        progress("show", 1, 2, 400, 10),
        progress("show", 1, 5, 400, 30),
        progress("film", std::nullopt, std::nullopt, 500, 20),
        progress("barely", std::nullopt, std::nullopt, 10, 40), // 1%
    }));
    REQUIRE(items.size() == 2);
    CHECK(items[0].contentId == "show");
    CHECK(*items[0].episode == 5);
    CHECK(items[1].contentId == "film");
}

TEST_CASE("next-up seeds: furthest episode, suppression and the hide list")
{
    std::vector<WatchedItem> list = {watched("a", 1, 3, 100), watched("a", 1, 1, 200), watched("b", 2, 1, 50),
        watched("c", 1, 1, 70)};
    // b has an in-progress episode watched after its seed
    auto map = mapOf({progress("b", 2, 2, 400, 60)});
    std::map<std::string, int64_t> hidden{{"c", 80}};

    auto seeds = nextUpSeeds(list, map, hidden);
    REQUIRE(seeds.size() == 1);
    CHECK(seeds[0].contentId == "a");
    CHECK(seeds[0].episode == 3);

    // c comes back once it has newer activity than the hide
    list.push_back(watched("c", 1, 2, 90));
    seeds = nextUpSeeds(list, map, hidden);
    CHECK(seeds.size() == 2);

    std::vector<WatchedItem> many;
    for (int i = 0; i < 40; i++)
        many.push_back(watched("s" + std::to_string(i), 1, 1, i));
    seeds = nextUpSeeds(many, {}, {});
    CHECK(seeds.size() == MAX_NEXT_UP_LOOKUPS);
    CHECK(seeds[0].contentId == "s39");
}

TEST_CASE("the row merges in-progress and next-up items newest first")
{
    Meta meta;
    meta.name = "Show";
    NextUpSeed seed{"show", "series", "Show", std::nullopt, 1, 3, 50};
    auto next = nextUpItem(seed, meta, ep(1, 4));
    CHECK(next.nextUp);
    CHECK(next.progress.videoId == "tt1:1:4");
    CHECK(next.progress.position == 0);

    auto row = mergeContinueItems({progress("film", std::nullopt, std::nullopt, 500, 20)}, {next});
    REQUIRE(row.size() == 2);
    CHECK(row[0].nextUp);
    CHECK_FALSE(row[1].nextUp);
}

TEST_CASE("the loader looks up at most 4 at a time and adds next-up items as metas arrive")
{
    std::vector<WatchedItem> list;
    for (int i = 0; i < 6; i++)
        list.push_back(watched("s" + std::to_string(i), 1, 1, i));

    std::map<uint64_t, ContinueWatchingLoader::MetaCallback> pending;
    std::vector<uint64_t> cancelled;
    uint64_t nextTicket = 1;
    ContinueWatchingLoader loader(
        [&](const NextUpSeed&, ContinueWatchingLoader::MetaCallback cb)
        {
            pending[nextTicket] = std::move(cb);
            return nextTicket++;
        },
        [&](uint64_t ticket) { cancelled.push_back(ticket); });

    std::vector<std::vector<ContinueItem>> updates;
    loader.start(mapOf({progress("film", std::nullopt, std::nullopt, 500, 100)}), list, {}, TODAY,
        [&](const std::vector<ContinueItem>& row) { updates.push_back(row); });
    REQUIRE(updates.size() == 1);
    CHECK(updates[0].size() == 1); // the film, before any lookup answers
    CHECK(pending.size() == 4);

    Meta meta;
    meta.videos = {ep(1, 1), ep(1, 2)};
    auto first  = pending.begin();
    auto cb     = first->second;
    pending.erase(first);
    cb(&meta);
    CHECK(updates.size() == 2);
    CHECK(updates.back().size() == 2);
    CHECK(pending.size() == 4); // the fifth started

    auto second = pending.begin();
    cb          = second->second;
    pending.erase(second);
    cb(nullptr); // no meta: nothing added
    CHECK(updates.size() == 2);
    CHECK(loader.isLoading());
}

TEST_CASE("the loader copes with lookups that answer at once, and cancels the rest when it goes")
{
    std::vector<WatchedItem> list;
    for (int i = 0; i < 6; i++)
        list.push_back(watched("s" + std::to_string(i), 1, 1, i));
    Meta meta;
    meta.videos = {ep(1, 1), ep(1, 2)};

    std::vector<uint64_t> cancelled;
    size_t updates = 0;
    {
        int calls = 0;
        ContinueWatchingLoader loader(
            [&](const NextUpSeed&, ContinueWatchingLoader::MetaCallback cb) -> uint64_t
            {
                calls++;
                if (calls <= 5)
                {
                    cb(&meta); // cached
                    return 0;
                }
                return 77;
            },
            [&](uint64_t ticket) { cancelled.push_back(ticket); });
        loader.start({}, list, {}, TODAY, [&](const std::vector<ContinueItem>&) { updates++; });
        CHECK(calls == 6);
        CHECK(updates == 6); // the empty start, then one per cached answer
    }
    CHECK(cancelled == std::vector<uint64_t>{77});
}

TEST_CASE("card hand-offs and time left")
{
    WatchProgress p = progress("tt1", 1, 4, 400, 10);
    p.videoId       = "tt1:1:4";
    auto request    = streamRequestFor(p, true);
    CHECK(request.videoId == "tt1:1:4");
    CHECK(request.season == std::optional<int>(1));
    CHECK(request.startFromBeginning);
    CHECK(previewFor(p).type == "series");

    CHECK(shortDuration(52 * 60000) == "52m");
    CHECK(shortDuration(52 * 60000 + 1) == "53m");
    CHECK(shortDuration(65 * 60000) == "1h 5m");
    CHECK(shortDuration(120 * 60000) == "2h");
    CHECK(shortDuration(0) == "0m");
}

#include "data/progress/play_target.hpp"

TEST_CASE("play target: a movie resumes when in progress")
{
    Meta meta;
    meta.id = "film";
    CHECK_FALSE(playTarget(meta, {}, {}, TODAY).resume);
    auto target = playTarget(meta, mapOf({progress("film", std::nullopt, std::nullopt, 500, 1)}), {}, TODAY);
    REQUIRE(target.resume);
    CHECK_FALSE(target.episode);
}

TEST_CASE("play target: series episodes")
{
    Meta meta;
    meta.id     = "tt1";
    meta.videos = {ep(0, 1), ep(1, 1), ep(1, 2), ep(1, 3), ep(1, 4)};

    // nothing watched: the first episode, not the special
    auto target = playTarget(meta, {}, {}, TODAY);
    REQUIRE(target.episode);
    CHECK(*target.episode->season == 1);
    CHECK(*target.episode->episode == 1);

    // finished S1E3: S1E4
    target = playTarget(meta, {}, {watched("tt1", 1, 3, 5), watched("tt1", 1, 1, 9)}, TODAY);
    REQUIRE(target.episode);
    CHECK(*target.episode->episode == 4);
    CHECK_FALSE(target.resume);

    // an in-progress episode wins
    target = playTarget(meta, mapOf({progress("tt1", 1, 2, 400, 7)}), {watched("tt1", 1, 3, 5)}, TODAY);
    REQUIRE(target.episode);
    CHECK(*target.episode->episode == 2);
    CHECK(target.resume);
}
