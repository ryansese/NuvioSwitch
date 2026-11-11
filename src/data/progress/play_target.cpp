#include "data/progress/play_target.hpp"

#include <algorithm>

#include "data/progress/continue_watching.hpp"

namespace nuvio::data::progress
{

PlayTarget playTarget(const domain::Meta& meta, const ProgressMap& progress, const std::vector<WatchedItem>& watched,
    const std::string& today)
{
    PlayTarget target;

    std::vector<const domain::Video*> episodes;
    for (const auto& video : meta.videos)
        if (video.season && video.episode)
            episodes.push_back(&video);

    if (episodes.empty())
    {
        auto entry = progress.find(progressKey(meta.id, std::nullopt, std::nullopt));
        if (entry != progress.end() && isInProgress(entry->second))
            target.resume = entry->second;
        return target;
    }

    auto findEpisode = [&](int season, int episode, const std::string& videoId) -> const domain::Video*
    {
        for (const auto* video : episodes)
            if (*video->season == season && *video->episode == episode)
                return video;
        for (const auto* video : episodes)
            if (!videoId.empty() && video->id == videoId)
                return video;
        return nullptr;
    };

    // the latest in-progress episode
    const WatchProgress* latest = nullptr;
    for (const auto& [key, entry] : progress)
        if (entry.contentId == meta.id && entry.season && entry.episode && isInProgress(entry) &&
            (!latest || entry.lastWatched > latest->lastWatched))
            latest = &entry;
    if (latest)
        if (const auto* video = findEpisode(*latest->season, *latest->episode, latest->videoId))
        {
            target.episode = *video;
            target.resume  = *latest;
            return target;
        }

    // the next-up episode after the furthest watched one
    const WatchedItem* furthest = nullptr;
    for (const auto& item : watched)
        if (item.contentId == meta.id && item.season && item.episode &&
            (!furthest || std::tie(*item.season, *item.episode) > std::tie(*furthest->season, *furthest->episode)))
            furthest = &item;
    if (furthest)
        if (auto next = resolveNextUp(meta.videos, *furthest->season, *furthest->episode, "", today))
        {
            target.episode = *next;
            return target;
        }

    // the first episode, specials last
    auto first = std::min_element(episodes.begin(), episodes.end(), [](const auto* a, const auto* b)
        {
            int seasonA = *a->season == 0 ? INT32_MAX : *a->season;
            int seasonB = *b->season == 0 ? INT32_MAX : *b->season;
            return seasonA != seasonB ? seasonA < seasonB : *a->episode < *b->episode;
        });
    target.episode = **first;
    return target;
}

} // namespace nuvio::data::progress
