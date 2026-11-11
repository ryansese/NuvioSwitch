#pragma once

#include <optional>
#include <string>
#include <vector>

#include "data/progress/watch_progress.hpp"
#include "domain/meta.hpp"

// What the Details screen's main button plays (NuvioTV NextToWatch): for a movie, the movie, resumed when
// it is in progress; for a series, its most recently watched in-progress episode, else the next-up
// episode after the furthest watched one, else the first episode.
namespace nuvio::data::progress
{

struct PlayTarget
{
    std::optional<domain::Video> episode;  // series only
    std::optional<WatchProgress> resume;   // set when the target is in progress
};

// today is "YYYY-MM-DD", for the next-up episode's release rule.
PlayTarget playTarget(const domain::Meta& meta, const ProgressMap& progress, const std::vector<WatchedItem>& watched,
    const std::string& today);

} // namespace nuvio::data::progress
