#include <doctest.h>

#include <vector>

#include "data/progress/progress_tracker.hpp"

using namespace nuvio::data::progress;

namespace
{
constexpr int64_t FILM = 6000000; // 100 minutes

struct Harness
{
    int64_t clock = 0;
    std::vector<WatchProgress> saves;
    ProgressTracker tracker;

    Harness()
        : tracker(base(), [this](const WatchProgress& p) { saves.push_back(p); }, [this] { return clock; },
              [this] { return 1700000000000 + clock; })
    {
    }

    static WatchProgress base()
    {
        WatchProgress p;
        p.contentId   = "tt1";
        p.contentType = "movie";
        p.name        = "Film";
        p.videoId     = "tt1";
        return p;
    }
};
} // namespace

TEST_CASE("placeholder durations")
{
    CHECK(isShortPlaceholderDuration(30000));
    CHECK(isShortPlaceholderDuration(120999));
    CHECK_FALSE(isShortPlaceholderDuration(121000));
    CHECK_FALSE(isShortPlaceholderDuration(0));
}

TEST_CASE("effective duration")
{
    CHECK(effectiveDuration(1000, 5000, 7000, false) == 7000);
    CHECK(effectiveDuration(9000, 5000, 7000, false) == 0);
    CHECK(effectiveDuration(9000, 5000, 7000, true) == 7000);
    CHECK(effectiveDuration(1000, 0, 0, false) == 0);
}

TEST_CASE("nothing is saved before the first frame")
{
    Harness h;
    h.tracker.onTick(500000, FILM);
    h.tracker.onPause();
    h.tracker.onExit();
    CHECK(h.saves.empty());
}

TEST_CASE("periodic saves need 90 s and 5 s of movement")
{
    Harness h;
    h.tracker.onFirstFrame();
    h.clock = 89999;
    h.tracker.onTick(90000, FILM);
    CHECK(h.saves.empty());
    h.clock = 90000;
    h.tracker.onTick(90000, FILM);
    REQUIRE(h.saves.size() == 1);
    CHECK(h.saves[0].position == 90000);
    CHECK(h.saves[0].duration == FILM);
    CHECK(h.saves[0].lastWatched == 1700000090000);

    // paused in place: the interval passes but the position hasn't moved
    h.clock = 200000;
    h.tracker.onTick(92000, FILM);
    CHECK(h.saves.size() == 1);
    h.tracker.onTick(96000, FILM);
    CHECK(h.saves.size() == 2);
}

TEST_CASE("pause, exit and end save at once")
{
    Harness h;
    h.tracker.onFirstFrame();
    h.tracker.onTick(300000, FILM);
    h.tracker.onPause();
    h.tracker.onExit();
    CHECK(h.saves.size() == 2);
}

TEST_CASE("positions below a second are not saved")
{
    Harness h;
    h.tracker.onFirstFrame();
    h.tracker.onTick(999, FILM);
    h.tracker.onExit();
    CHECK(h.saves.empty());
}

TEST_CASE("a placeholder stream played to the end writes nothing")
{
    Harness h;
    h.tracker.onFirstFrame();
    h.tracker.onTick(29000, 30000);
    h.tracker.onNaturalEnd();
    h.tracker.onExit();
    CHECK(h.saves.empty());
}

TEST_CASE("completion is saved once and later stale saves are dropped")
{
    Harness h;
    h.tracker.onFirstFrame();
    h.tracker.onTick(5520000, FILM); // 92%
    h.tracker.onPause();
    REQUIRE(h.saves.size() == 1);
    CHECK(isCompleted(h.saves[0]));
    CHECK(h.tracker.hasCompleted());

    h.tracker.onTick(300000, FILM); // a stale 5%
    h.tracker.onExit();
    h.tracker.onNaturalEnd();
    CHECK(h.saves.size() == 1);
}

TEST_CASE("the natural end completes even when mpv stops a little short")
{
    Harness h;
    h.tracker.onFirstFrame();
    h.tracker.onTick(FILM - 400, FILM);
    h.tracker.onNaturalEnd();
    REQUIRE(h.saves.size() == 1);
    CHECK(h.saves[0].position == FILM);
}

TEST_CASE("an unknown duration saves the position only")
{
    Harness h;
    h.tracker.onFirstFrame();
    h.tracker.onTick(400000, 0);
    h.tracker.onExit();
    REQUIRE(h.saves.size() == 1);
    CHECK(h.saves[0].duration == 0);
    CHECK_FALSE(isInProgress(h.saves[0]));
}
