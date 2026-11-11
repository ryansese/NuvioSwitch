#include "data/progress/progress_repository.hpp"

#include <algorithm>
#include <chrono>

namespace nuvio::data::progress
{

namespace
{
ProgressRepository* sharedRepository = nullptr;

ProgressMap readMap(const std::shared_ptr<core::storage::Store>& store)
{
    ProgressMap out;
    if (!store)
        return out;
    auto all = store->all();
    for (auto it = all.begin(); it != all.end(); ++it)
        if (auto entry = progressFromJson(it.value())) // one bad entry doesn't drop the rest
            out[it.key()] = std::move(*entry);
    return out;
}

nlohmann::json writeMap(const ProgressMap& map)
{
    auto json = nlohmann::json::object();
    for (const auto& [key, entry] : map)
        json[key] = toJson(entry);
    return json;
}

bool sameItem(const WatchedItem& item, const std::string& contentId, std::optional<int> season,
    std::optional<int> episode)
{
    return item.contentId == contentId && item.season == season && item.episode == episode;
}
} // namespace

ProgressRepository::ProgressRepository(Config value) : config(std::move(value))
{
    if (!config.nowMs)
        config.nowMs = []
        {
            using namespace std::chrono;
            return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
        };

    progress = mergeBuckets(readMap(config.recent), readMap(config.archive));

    if (config.watched)
    {
        auto items = config.watched->get("items");
        if (items.is_array())
            for (const auto& json : items)
                if (auto item = watchedFromJson(json))
                    watchedItems.push_back(std::move(*item));
    }
    if (config.hidden)
    {
        auto all = config.hidden->all();
        for (auto it = all.begin(); it != all.end(); ++it)
            if (it.value().is_number_integer())
                hidden[it.key()] = it.value().get<int64_t>();
    }
}

std::optional<WatchProgress> ProgressRepository::get(const std::string& key) const
{
    auto it = progress.find(key);
    if (it == progress.end())
        return std::nullopt;
    return it->second;
}

void ProgressRepository::save(const WatchProgress& entry)
{
    if (isCompleted(entry))
        return markCompleted(entry);
    upsertProgress(progress, entry);
    writeProgress();
    notify();
}

void ProgressRepository::remove(const std::string& key)
{
    if (!progress.erase(key))
        return;
    writeProgress();
    notify();
}

void ProgressRepository::markCompleted(const WatchProgress& entry)
{
    WatchedItem item;
    item.contentId   = entry.contentId;
    item.contentType = entry.contentType;
    item.title       = entry.name;
    item.season      = entry.season;
    item.episode     = entry.episode;
    item.watchedAt   = entry.lastWatched > 0 ? entry.lastWatched : now();
    item.poster      = entry.poster;
    markWatched(std::move(item));
}

bool ProgressRepository::isWatched(const std::string& contentId, std::optional<int> season,
    std::optional<int> episode) const
{
    return std::any_of(watchedItems.begin(), watchedItems.end(),
        [&](const WatchedItem& item) { return sameItem(item, contentId, season, episode); });
}

void ProgressRepository::markWatched(WatchedItem item)
{
    if (item.watchedAt <= 0)
        item.watchedAt = now();
    watchedItems.erase(std::remove_if(watchedItems.begin(), watchedItems.end(),
                           [&](const WatchedItem& w) { return sameItem(w, item.contentId, item.season, item.episode); }),
        watchedItems.end());
    std::string key = progressKey(item.contentId, item.season, item.episode);
    watchedItems.push_back(std::move(item));
    writeWatched();
    if (progress.erase(key))
        writeProgress();
    notify();
}

void ProgressRepository::markUnwatched(const std::string& contentId, std::optional<int> season,
    std::optional<int> episode)
{
    auto before = watchedItems.size();
    watchedItems.erase(std::remove_if(watchedItems.begin(), watchedItems.end(),
                           [&](const WatchedItem& w) { return sameItem(w, contentId, season, episode); }),
        watchedItems.end());
    if (watchedItems.size() == before)
        return;
    writeWatched();
    notify();
}

void ProgressRepository::hideSeries(const std::string& contentId)
{
    hidden[contentId] = now();
    if (config.hidden)
    {
        config.hidden->set(contentId, hidden[contentId]);
        config.hidden->save();
    }
    notify();
}

void ProgressRepository::writeProgress()
{
    auto buckets = splitBuckets(progress);
    // the split also drops stale series-level entries, so memory follows it
    progress = mergeBuckets(buckets.recent, buckets.archive);
    if (config.recent)
    {
        config.recent->replaceAll(writeMap(buckets.recent));
        config.recent->save();
    }
    if (config.archive)
    {
        config.archive->replaceAll(writeMap(buckets.archive));
        config.archive->save();
    }
}

void ProgressRepository::writeWatched()
{
    if (!config.watched)
        return;
    auto items = nlohmann::json::array();
    for (const auto& item : watchedItems)
        items.push_back(toJson(item));
    config.watched->set("items", std::move(items));
    config.watched->save();
}

int ProgressRepository::subscribe(Listener listener)
{
    int token = nextToken++;
    listeners[token] = std::move(listener);
    return token;
}

void ProgressRepository::unsubscribe(int token)
{
    listeners.erase(token);
}

void ProgressRepository::notify()
{
    // a listener may unsubscribe itself (a view going away)
    auto copy = listeners;
    for (auto& [token, listener] : copy)
        if (listeners.count(token))
            listener();
}

ProgressRepository& ProgressRepository::shared()
{
    return *sharedRepository;
}

void ProgressRepository::setShared(ProgressRepository* repository)
{
    sharedRepository = repository;
}

bool ProgressRepository::hasShared()
{
    return sharedRepository != nullptr;
}

} // namespace nuvio::data::progress
