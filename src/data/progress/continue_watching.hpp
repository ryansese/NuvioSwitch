#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "data/progress/watch_progress.hpp"
#include "domain/meta.hpp"
#include "domain/meta_preview.hpp"
#include "domain/stream_request.hpp"

// Home's Continue Watching row: a trimmed port of NuvioTV's loadContinueWatchingPipeline
// (ui/screens/home/HomeViewModelContinueWatching.kt) without remote sources, TMDB enrichment or
// unaired next-up episodes.
namespace nuvio::data::progress
{

constexpr size_t MAX_NEXT_UP_LOOKUPS = 32; // NuvioTV CW_MAX_NEXT_UP_LOOKUPS
constexpr size_t NEXT_UP_PARALLEL    = 4;

// The "YYYY-MM-DD" day a video's release string starts with ("2008-01-20" or "2008-01-20T02:00:00.000Z"),
// or nullopt when it doesn't start with one.
std::optional<std::string> releaseDay(const std::optional<std::string>& released);

// The episode after season/episode in videos (NuvioTV resolveNextUpVideoFromMeta, showUnairedNextUp off):
// episodes ordered by season then episode, season 0 left out. The watched one is found by its numbers,
// else by videoId. The next is the first after it whose release day is known and not after today;
// an episode without a date or released later is skipped. nullopt when there is none.
std::optional<domain::Video> resolveNextUp(const std::vector<domain::Video>& videos, int season, int episode,
    const std::string& videoId, const std::string& today);

// A series' furthest watched episode, from which its next-up episode is looked up.
struct NextUpSeed
{
    std::string contentId;
    std::string contentType;
    std::string name;
    std::optional<std::string> poster;
    int season       = 0;
    int episode      = 0;
    int64_t watchedAt = 0;
};

// In-progress entries, newest first, with only the most recently watched one per series.
std::vector<WatchProgress> inProgressItems(const ProgressMap& progress);

// One seed per series with watched episodes: the furthest by season, episode, then watchedAt. A series is
// left out when one of its in-progress episodes was watched after the seed, or when it was hidden from the
// row after its latest activity. Newest first, at most limit.
std::vector<NextUpSeed> nextUpSeeds(const std::vector<WatchedItem>& watched, const ProgressMap& progress,
    const std::map<std::string, int64_t>& hidden, size_t limit = MAX_NEXT_UP_LOOKUPS);

struct ContinueItem
{
    bool nextUp = false;
    // for a next-up item: the episode to play, position 0, lastWatched the seed's watchedAt
    WatchProgress progress;
};

ContinueItem nextUpItem(const NextUpSeed& seed, const domain::Meta& meta, const domain::Video& next);

// The stream picker request that plays the entry's video (resuming unless startFromBeginning).
domain::StreamRequest streamRequestFor(const WatchProgress& entry, bool startFromBeginning = false);
// What the Details screen opens with for the entry's title.
domain::MetaPreview previewFor(const WatchProgress& entry);
// Time as "1h 5m" or "52m", rounded up to the minute: the "… left" of a card.
std::string shortDuration(int64_t ms);

// In-progress and next-up items together, by lastWatched, newest first.
std::vector<ContinueItem> mergeContinueItems(const std::vector<WatchProgress>& inProgress,
    const std::vector<ContinueItem>& nextUps);

// Builds the row: the in-progress items at once, then next-up items as their metas arrive, at most
// NEXT_UP_PARALLEL lookups at a time. Lookups may answer before lookup() returns. Destroying the loader
// cancels the lookups still running. Single-threaded: callbacks must arrive on the caller's thread.
class ContinueWatchingLoader
{
  public:
    using MetaCallback = std::function<void(const domain::Meta*)>; // nullptr when no add-on had it
    using Lookup       = std::function<uint64_t(const NextUpSeed&, MetaCallback)>;
    using Cancel       = std::function<void(uint64_t)>;
    using Listener     = std::function<void(const std::vector<ContinueItem>&)>;

    ContinueWatchingLoader(Lookup lookup, Cancel cancel);
    ~ContinueWatchingLoader();

    // Starts over with this data; onUpdate gets the in-progress items right away and the whole row again
    // each time a next-up episode is found. today is "YYYY-MM-DD".
    void start(const ProgressMap& progress, const std::vector<WatchedItem>& watched,
        const std::map<std::string, int64_t>& hidden, const std::string& today, Listener onUpdate);

    bool isLoading() const { return running > 0 || nextSeed < seeds.size(); }

  private:
    enum class SlotState
    {
        Waiting,
        InFlight,
        Done,
    };
    struct Slot
    {
        SlotState state = SlotState::Waiting;
        uint64_t ticket = 0;
    };

    void cancelAll();
    void pump();
    void onMeta(uint64_t generation, size_t index, const domain::Meta* meta);
    void emit();

    Lookup lookup;
    Cancel cancel;
    Listener listener;
    std::string today;
    std::vector<WatchProgress> inProgress;
    std::vector<NextUpSeed> seeds;
    std::vector<Slot> slots;
    std::vector<ContinueItem> nextUps;
    size_t nextSeed     = 0;
    size_t running      = 0;
    uint64_t generation = 0;
    bool pumping        = false;
};

} // namespace nuvio::data::progress
