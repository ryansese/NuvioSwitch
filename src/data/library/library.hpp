#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "domain/meta.hpp"
#include "domain/meta_preview.hpp"

// The saved-items library (NuvioTV domain/model/SavedLibraryItem.kt, data/local/LibraryPreferences.kt,
// ui/screens/library/LibraryViewModel.kt). Field names match NuvioTV's stored JSON.
namespace nuvio::data::library
{

struct SavedLibraryItem
{
    std::string id;
    std::string type;
    std::string name;
    std::optional<std::string> poster;
    domain::PosterShape posterShape = domain::PosterShape::Poster;
    std::optional<std::string> background;
    std::optional<std::string> logo;
    std::optional<std::string> description;
    std::optional<std::string> releaseInfo;
    std::optional<float> imdbRating;
    std::vector<std::string> genres;
    std::optional<std::string> addonBaseUrl;
    int64_t addedAt = 0; // epoch ms
};

// NuvioTV LibrarySortOption keys.
enum class Sort
{
    Provider,    // "default": the order items were stored in
    AddedNewest, // "added_desc"
    AddedOldest, // "added_asc"
    TitleAsc,    // "title_asc"
    TitleDesc,   // "title_desc"
};

enum class Filter
{
    All,
    Movies,
    Series,
};

std::string sortKey(Sort sort);
Sort sortFromKey(const std::string& key); // unknown keys give AddedNewest, NuvioTV's local default
std::string filterKey(Filter filter);
Filter filterFromKey(const std::string& key);

nlohmann::json toJson(const SavedLibraryItem& item);
std::optional<SavedLibraryItem> itemFromJson(const nlohmann::json& json); // nullopt without id or type

SavedLibraryItem itemFromMeta(const domain::Meta& meta, int64_t addedAt);
domain::MetaPreview toPreview(const SavedLibraryItem& item);

// Sorted copy; titles compare ignoring case, ties keep the stored order.
std::vector<SavedLibraryItem> sorted(std::vector<SavedLibraryItem> items, Sort sort);
// Movies: type "movie". Series: "series" or "tv".
std::vector<SavedLibraryItem> filtered(const std::vector<SavedLibraryItem>& items, Filter filter);

} // namespace nuvio::data::library
