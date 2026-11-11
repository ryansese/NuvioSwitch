#pragma once

#include <cstdint>
#include <functional>

#include "data/progress/watch_progress.hpp"

// When the player saves watch progress (NuvioTV PlayerRuntimeControllerPlaybackEvents.kt:
// saveWatchProgressIfNeeded, saveWatchProgressInternal). The player activity owns one per stream and
// reports what mpv tells it; the tracker decides when a save is due and what it holds.
namespace nuvio::data::progress
{

constexpr int64_t SAVE_INTERVAL_MS     = 90000; // NuvioTV WATCH_PROGRESS_SAVE_INTERVAL_MS
constexpr int64_t SAVE_MIN_MOVE_MS     = 5000;  // NuvioTV saveThresholdMs
constexpr int64_t MIN_SAVE_POSITION_MS = 1000;

// 1..120 999 ms: placeholder and error videos ("stream not ready"), never saved.
bool isShortPlaceholderDuration(int64_t durationMs);

// The larger of mpv's duration and the last known one; 0 when the position is past it and
// playback hasn't ended (a stream that reported a wrong, short duration). NuvioTV getEffectiveDuration.
int64_t effectiveDuration(int64_t positionMs, int64_t durationMs, int64_t lastKnownMs, bool ended);

class ProgressTracker
{
  public:
    using Saver = std::function<void(const WatchProgress&)>;
    using Clock = std::function<int64_t()>; // ms

    // base: the title, artwork and video of what is playing; position, duration and lastWatched are
    // filled in at each save. monotonicMs paces the periodic saves, wallMs stamps lastWatched.
    ProgressTracker(WatchProgress base, Saver save, Clock monotonicMs, Clock wallMs);

    void onFirstFrame();
    // mpv's position and duration (0 when unknown); saves when the interval passed and it moved enough.
    void onTick(int64_t positionMs, int64_t durationMs);
    void onPause();
    void onExit();
    void onNaturalEnd();

    bool hasCompleted() const { return completed; }

  private:
    void saveNow(bool ended);

    WatchProgress base;
    Saver saver;
    Clock monotonic;
    Clock wall;

    bool started            = false;
    bool completed          = false; // marked watched this session: later saves are stale
    int64_t position        = 0;
    int64_t duration        = 0;
    int64_t lastKnown       = 0;
    int64_t lastSaveAt      = 0;
    int64_t lastSavedPosition = -1;
};

} // namespace nuvio::data::progress
