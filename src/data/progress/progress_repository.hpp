#pragma once

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "core/storage/store.hpp"
#include "data/progress/watch_progress.hpp"

namespace nuvio::data::progress
{

// The active profile's watch progress, watched items and Continue Watching hide list, loaded once
// and kept in memory. Every change is written through the stores (on their writer thread):
//   watch_progress_recent / watch_progress_archive: { key: WatchProgress }, split by splitBuckets;
//   watched_items: { "items": [WatchedItem] };
//   cw_hidden: { contentId: hidden-at ms }.
// Every method must be called on the UI thread; listeners run synchronously after a change.
class ProgressRepository
{
  public:
    using Store    = core::storage::Store;
    using Listener = std::function<void()>;

    struct Config
    {
        std::shared_ptr<Store> recent;
        std::shared_ptr<Store> archive;
        std::shared_ptr<Store> watched;
        std::shared_ptr<Store> hidden;
        std::function<int64_t()> nowMs; // epoch ms; defaults to the system clock
    };

    explicit ProgressRepository(Config config);

    // Every entry, recent and archive together.
    const ProgressMap& entries() const { return progress; }
    std::optional<WatchProgress> get(const std::string& key) const;

    // Stores an in-progress save. A save at or past the completed threshold becomes a watched
    // record instead (markCompleted).
    void save(const WatchProgress& entry);
    // Deletes the entry under key.
    void remove(const std::string& key);
    // Records the item as watched and deletes its progress entry.
    void markCompleted(const WatchProgress& entry);

    const std::vector<WatchedItem>& watched() const { return watchedItems; }
    bool isWatched(const std::string& contentId, std::optional<int> season, std::optional<int> episode) const;
    // Adds (or refreshes) a watched record and deletes the matching progress entry.
    void markWatched(WatchedItem item);
    // Removes the watched record for the movie or episode.
    void markUnwatched(const std::string& contentId, std::optional<int> season, std::optional<int> episode);

    // Continue Watching's hide list for next-up series: contentId -> hidden-at.
    const std::map<std::string, int64_t>& hiddenSeries() const { return hidden; }
    void hideSeries(const std::string& contentId);

    int64_t now() const { return config.nowMs(); }

    // Writes everything queued so far before returning (leaving the player).
    static void flush() { Store::flush(); }

    int subscribe(Listener listener);
    void unsubscribe(int token);

    static ProgressRepository& shared();
    static void setShared(ProgressRepository* repository);
    static bool hasShared();

  private:
    void writeProgress();
    void writeWatched();
    void notify();

    Config config;
    ProgressMap progress;
    std::vector<WatchedItem> watchedItems;
    std::map<std::string, int64_t> hidden;
    std::map<int, Listener> listeners;
    int nextToken = 1;
};

} // namespace nuvio::data::progress
