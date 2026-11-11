#include "data/progress/continue_watching.hpp"

#include <algorithm>
#include <cctype>
#include <set>

namespace nuvio::data::progress
{

std::optional<std::string> releaseDay(const std::optional<std::string>& released)
{
    if (!released || released->size() < 10)
        return std::nullopt;
    const std::string& s = *released;
    for (size_t i = 0; i < 10; i++)
    {
        bool dash = i == 4 || i == 7;
        if (dash ? s[i] != '-' : !std::isdigit(static_cast<unsigned char>(s[i])))
            return std::nullopt;
    }
    return s.substr(0, 10);
}

std::optional<domain::Video> resolveNextUp(const std::vector<domain::Video>& videos, int season, int episode,
    const std::string& videoId, const std::string& today)
{
    std::vector<const domain::Video*> episodes;
    for (const auto& video : videos)
        if (video.season && video.episode && *video.season != 0)
            episodes.push_back(&video);
    std::stable_sort(episodes.begin(), episodes.end(), [](const auto* a, const auto* b)
        { return *a->season != *b->season ? *a->season < *b->season : *a->episode < *b->episode; });

    auto watched = std::find_if(episodes.begin(), episodes.end(),
        [&](const auto* v) { return *v->season == season && *v->episode == episode; });
    if (watched == episodes.end() && !videoId.empty())
        watched = std::find_if(episodes.begin(), episodes.end(), [&](const auto* v) { return v->id == videoId; });
    if (watched == episodes.end())
        return std::nullopt;

    // with unaired episodes off, a next episode in the same season and one across a rollover
    // follow the same rule: it must have a known release day that has come
    for (auto it = watched + 1; it != episodes.end(); ++it)
    {
        auto day = releaseDay((*it)->released);
        if (day && *day <= today)
            return **it;
    }
    return std::nullopt;
}

std::vector<WatchProgress> inProgressItems(const ProgressMap& progress)
{
    std::vector<WatchProgress> items;
    for (const auto& [key, entry] : progress)
        if (isInProgress(entry))
            items.push_back(entry);
    std::stable_sort(items.begin(), items.end(),
        [](const auto& a, const auto& b) { return a.lastWatched > b.lastWatched; });

    std::set<std::string> seriesSeen;
    std::vector<WatchProgress> out;
    for (auto& item : items)
    {
        bool series = isSeriesType(item.contentType) || (item.season && item.episode);
        if (series && !seriesSeen.insert(item.contentId).second)
            continue; // an older episode of a series already in the list
        out.push_back(std::move(item));
    }
    return out;
}

std::vector<NextUpSeed> nextUpSeeds(const std::vector<WatchedItem>& watched, const ProgressMap& progress,
    const std::map<std::string, int64_t>& hidden, size_t limit)
{
    std::map<std::string, NextUpSeed> best;
    for (const auto& item : watched)
    {
        if (!item.season || !item.episode)
            continue;
        if (!item.contentType.empty() && !isSeriesType(item.contentType))
            continue;
        NextUpSeed seed{item.contentId, item.contentType, item.title, item.poster, *item.season, *item.episode,
            item.watchedAt};
        auto it = best.find(item.contentId);
        if (it == best.end())
        {
            best.emplace(item.contentId, seed);
            continue;
        }
        const NextUpSeed& old = it->second;
        if (std::tie(seed.season, seed.episode, seed.watchedAt) > std::tie(old.season, old.episode, old.watchedAt))
            it->second = seed;
    }

    std::vector<NextUpSeed> out;
    for (auto& [contentId, seed] : best)
    {
        int64_t latest = seed.watchedAt;
        bool suppressed = false;
        for (const auto& [key, entry] : progress)
        {
            if (entry.contentId != contentId)
                continue;
            latest = std::max(latest, entry.lastWatched);
            // still watching something of this series since finishing the seed: that entry stands for it
            if (isInProgress(entry) && entry.lastWatched >= seed.watchedAt)
                suppressed = true;
        }
        for (const auto& item : watched)
            if (item.contentId == contentId)
                latest = std::max(latest, item.watchedAt);
        auto hiddenAt = hidden.find(contentId);
        if (hiddenAt != hidden.end() && hiddenAt->second >= latest)
            suppressed = true;
        if (!suppressed)
            out.push_back(seed);
    }
    std::stable_sort(out.begin(), out.end(), [](const auto& a, const auto& b) { return a.watchedAt > b.watchedAt; });
    if (out.size() > limit)
        out.resize(limit);
    return out;
}

ContinueItem nextUpItem(const NextUpSeed& seed, const domain::Meta& meta, const domain::Video& next)
{
    ContinueItem item;
    item.nextUp      = true;
    WatchProgress& p = item.progress;
    p.contentId      = seed.contentId;
    p.contentType    = seed.contentType.empty() ? meta.type : seed.contentType;
    p.name           = meta.name.empty() ? seed.name : meta.name;
    p.poster         = meta.poster ? meta.poster : seed.poster;
    p.backdrop       = meta.background;
    p.logo           = meta.logo;
    p.videoId        = next.id;
    p.season         = next.season;
    p.episode        = next.episode;
    if (!next.title.empty())
        p.episodeTitle = next.title;
    p.lastWatched = seed.watchedAt;
    if (!meta.addonBaseUrl.empty())
        p.addonBaseUrl = meta.addonBaseUrl;
    return item;
}

domain::StreamRequest streamRequestFor(const WatchProgress& entry, bool startFromBeginning)
{
    domain::StreamRequest request;
    request.contentType        = entry.contentType;
    request.contentId          = entry.contentId;
    request.videoId            = entry.videoId.empty() ? entry.contentId : entry.videoId;
    request.title              = entry.name;
    request.poster             = entry.poster;
    request.backdrop           = entry.backdrop;
    request.logo               = entry.logo;
    request.season             = entry.season;
    request.episode            = entry.episode;
    request.episodeTitle       = entry.episodeTitle;
    request.startFromBeginning = startFromBeginning;
    return request;
}

domain::MetaPreview previewFor(const WatchProgress& entry)
{
    domain::MetaPreview preview;
    preview.id                 = entry.contentId;
    preview.type               = entry.contentType;
    preview.name               = entry.name;
    preview.poster             = entry.poster;
    preview.background         = entry.backdrop;
    preview.logo               = entry.logo;
    preview.sourceAddonBaseUrl = entry.addonBaseUrl.value_or("");
    return preview;
}

std::string shortDuration(int64_t ms)
{
    int64_t minutes = (std::max<int64_t>(0, ms) + 59999) / 60000;
    if (minutes < 60)
        return std::to_string(minutes) + "m";
    int64_t rest = minutes % 60;
    return std::to_string(minutes / 60) + "h" + (rest ? " " + std::to_string(rest) + "m" : "");
}

std::vector<ContinueItem> mergeContinueItems(const std::vector<WatchProgress>& inProgress,
    const std::vector<ContinueItem>& nextUps)
{
    std::vector<ContinueItem> out;
    for (const auto& entry : inProgress)
        out.push_back({false, entry});
    out.insert(out.end(), nextUps.begin(), nextUps.end());
    std::stable_sort(out.begin(), out.end(),
        [](const auto& a, const auto& b) { return a.progress.lastWatched > b.progress.lastWatched; });
    return out;
}

ContinueWatchingLoader::ContinueWatchingLoader(Lookup lookup, Cancel cancel)
    : lookup(std::move(lookup))
    , cancel(std::move(cancel))
{
}

ContinueWatchingLoader::~ContinueWatchingLoader()
{
    cancelAll();
}

void ContinueWatchingLoader::cancelAll()
{
    generation++;
    for (auto& slot : slots)
        if (slot.state == SlotState::InFlight && slot.ticket && cancel)
            cancel(slot.ticket);
    slots.clear();
    seeds.clear();
    nextUps.clear();
    nextSeed = 0;
    running  = 0;
}

void ContinueWatchingLoader::start(const ProgressMap& progress, const std::vector<WatchedItem>& watched,
    const std::map<std::string, int64_t>& hidden, const std::string& day, Listener onUpdate)
{
    cancelAll();
    listener   = std::move(onUpdate);
    today      = day;
    inProgress = inProgressItems(progress);
    seeds      = nextUpSeeds(watched, progress, hidden);
    slots.assign(seeds.size(), Slot());
    emit();
    pump();
}

void ContinueWatchingLoader::pump()
{
    if (pumping)
        return;
    pumping = true;
    while (running < NEXT_UP_PARALLEL && nextSeed < seeds.size())
    {
        size_t index        = nextSeed++;
        slots[index].state  = SlotState::InFlight;
        running++;
        uint64_t gen    = generation;
        uint64_t ticket = lookup(seeds[index], [this, gen, index](const domain::Meta* meta) { onMeta(gen, index, meta); });
        // a cached meta answers inside lookup(): the slot is done and its ticket means nothing
        if (gen == generation && index < slots.size() && slots[index].state == SlotState::InFlight)
            slots[index].ticket = ticket;
    }
    pumping = false;
}

void ContinueWatchingLoader::onMeta(uint64_t gen, size_t index, const domain::Meta* meta)
{
    if (gen != generation || index >= slots.size() || slots[index].state != SlotState::InFlight)
        return;
    slots[index].state = SlotState::Done;
    running--;
    if (meta)
    {
        const NextUpSeed& seed = seeds[index];
        if (auto next = resolveNextUp(meta->videos, seed.season, seed.episode, "", today))
        {
            nextUps.push_back(nextUpItem(seed, *meta, *next));
            emit();
        }
    }
    pump();
}

void ContinueWatchingLoader::emit()
{
    if (listener)
        listener(mergeContinueItems(inProgress, nextUps));
}

} // namespace nuvio::data::progress
