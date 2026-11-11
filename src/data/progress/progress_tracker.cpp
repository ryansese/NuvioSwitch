#include "data/progress/progress_tracker.hpp"

#include <algorithm>
#include <cstdlib>

namespace nuvio::data::progress
{

bool isShortPlaceholderDuration(int64_t durationMs)
{
    return durationMs >= 1 && durationMs <= 120999;
}

int64_t effectiveDuration(int64_t positionMs, int64_t durationMs, int64_t lastKnownMs, bool ended)
{
    int64_t value = std::max(durationMs, lastKnownMs);
    if (value > 0 && positionMs > value && !ended)
        return 0;
    return value;
}

ProgressTracker::ProgressTracker(WatchProgress base, Saver save, Clock monotonicMs, Clock wallMs)
    : base(std::move(base))
    , saver(std::move(save))
    , monotonic(std::move(monotonicMs))
    , wall(std::move(wallMs))
{
}

void ProgressTracker::onFirstFrame()
{
    if (started)
        return;
    started    = true;
    lastSaveAt = monotonic();
}

void ProgressTracker::onTick(int64_t positionMs, int64_t durationMs)
{
    position  = std::max<int64_t>(0, positionMs);
    duration  = std::max<int64_t>(0, durationMs);
    lastKnown = std::max(lastKnown, duration);
    if (!started)
        return;
    if (monotonic() - lastSaveAt < SAVE_INTERVAL_MS)
        return;
    if (lastSavedPosition >= 0 && std::llabs(position - lastSavedPosition) < SAVE_MIN_MOVE_MS)
        return;
    saveNow(false);
}

void ProgressTracker::onPause()
{
    saveNow(false);
}

void ProgressTracker::onExit()
{
    saveNow(false);
}

void ProgressTracker::onNaturalEnd()
{
    // mpv's last position can fall a little short of the duration
    int64_t full = std::max(duration, lastKnown);
    if (full > 0)
        position = std::max(position, full);
    saveNow(true);
}

void ProgressTracker::saveNow(bool ended)
{
    if (!started || !saver)
        return;
    if (position < MIN_SAVE_POSITION_MS)
        return;
    int64_t full = effectiveDuration(position, duration, lastKnown, ended);
    if (isShortPlaceholderDuration(full))
        return;

    WatchProgress entry = base;
    entry.position      = position;
    entry.duration      = full;
    entry.lastWatched   = wall();

    lastSaveAt        = monotonic();
    lastSavedPosition = position;

    if (isCompleted(entry))
    {
        if (completed)
            return;
        completed = true;
        saver(entry);
        return;
    }
    // after completion the player can report stale values that would bring the entry back
    if (!completed)
        saver(entry);
}

} // namespace nuvio::data::progress
