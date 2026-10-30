#pragma once

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

// Recent searches (NuvioTV data/local/SearchHistoryDataStore.kt): at most 8, newest
// first, trimmed and de-duplicated without regard to case. Not thread-safe.
namespace nuvio::data::search
{

constexpr size_t MIN_QUERY_LENGTH   = 2; // characters (NuvioTV MIN_SEARCH_QUERY_LENGTH)
constexpr size_t MAX_HISTORY_ENTRIES = 8;

// query without leading and trailing whitespace.
std::string trimQuery(const std::string& query);

// Whether the trimmed query is long enough to search.
bool isSearchableQuery(const std::string& query);

class SearchHistory
{
  public:
    // Adds the trimmed query at the front, replacing an earlier entry that differs only in
    // case, and drops the oldest entries past the cap. A query too short to search is ignored.
    // Returns whether the list changed.
    bool add(const std::string& query);

    // Removes the entry at index; out of range is ignored.
    bool remove(size_t index);
    bool clear();

    const std::vector<std::string>& entries() const { return list; }

    // A JSON array of strings; load keeps the same rules as add, in stored order.
    nlohmann::json toJson() const;
    static SearchHistory fromJson(const nlohmann::json& json);

  private:
    std::vector<std::string> list;
};

} // namespace nuvio::data::search
