#include <doctest.h>

#include <memory>

#include "core/storage/store.hpp"
#include "data/library/library_repository.hpp"
#include "test_util.hpp"

using namespace nuvio::data::library;
using nuvio::core::storage::Store;

namespace
{
SavedLibraryItem item(const std::string& id, const std::string& type, const std::string& name, int64_t addedAt)
{
    SavedLibraryItem out;
    out.id      = id;
    out.type    = type;
    out.name    = name;
    out.addedAt = addedAt;
    return out;
}

std::vector<std::string> ids(const std::vector<SavedLibraryItem>& items)
{
    std::vector<std::string> out;
    for (const auto& i : items)
        out.push_back(i.id);
    return out;
}

const std::vector<SavedLibraryItem> SAMPLE = {
    item("b", "movie", "beta", 20),
    item("a", "series", "Alpha", 30),
    item("c", "movie", "Gamma", 10),
};
} // namespace

TEST_CASE("library sort options")
{
    CHECK(ids(sorted(SAMPLE, Sort::Provider)) == std::vector<std::string>{"b", "a", "c"});
    CHECK(ids(sorted(SAMPLE, Sort::AddedNewest)) == std::vector<std::string>{"a", "b", "c"});
    CHECK(ids(sorted(SAMPLE, Sort::AddedOldest)) == std::vector<std::string>{"c", "b", "a"});
    CHECK(ids(sorted(SAMPLE, Sort::TitleAsc)) == std::vector<std::string>{"a", "b", "c"}); // ignoring case
    CHECK(ids(sorted(SAMPLE, Sort::TitleDesc)) == std::vector<std::string>{"c", "b", "a"});
}

TEST_CASE("library sort and filter keys")
{
    CHECK(sortKey(Sort::TitleAsc) == "title_asc");
    CHECK(sortFromKey("title_desc") == Sort::TitleDesc);
    CHECK(sortFromKey("default") == Sort::Provider);
    CHECK(sortFromKey("bogus") == Sort::AddedNewest);
    CHECK(filterFromKey(filterKey(Filter::Series)) == Filter::Series);
}

TEST_CASE("library type filter")
{
    CHECK(ids(filtered(SAMPLE, Filter::All)).size() == 3);
    CHECK(ids(filtered(SAMPLE, Filter::Movies)) == std::vector<std::string>{"b", "c"});
    CHECK(ids(filtered(SAMPLE, Filter::Series)) == std::vector<std::string>{"a"});
}

TEST_CASE("library item JSON")
{
    SavedLibraryItem i = item("tt1", "movie", "One", 5);
    i.posterShape      = nuvio::domain::PosterShape::Landscape;
    i.imdbRating       = 7.5f;
    i.genres           = {"Drama"};
    auto json          = toJson(i);
    CHECK(json["posterShape"] == "LANDSCAPE");
    auto back = itemFromJson(json);
    REQUIRE(back);
    CHECK(back->posterShape == nuvio::domain::PosterShape::Landscape);
    CHECK(back->imdbRating == std::optional<float>(7.5f));
    CHECK(back->genres == std::vector<std::string>{"Drama"});
    CHECK_FALSE(itemFromJson(nlohmann::json{{"id", "x"}}));
}

TEST_CASE("the library repository adds without duplicates and keeps its settings")
{
    TempDir dir;
    std::string path = dir.path() + "library.json";
    {
        LibraryRepository repo({std::make_shared<Store>(path), [] { return int64_t(99); }});
        CHECK(repo.add(item("tt1", "movie", "One", 0)));
        CHECK_FALSE(repo.add(item("tt1", "movie", "One again", 0)));
        CHECK(repo.add(item("tt1", "series", "Same id, other type", 0)));
        CHECK(repo.items().size() == 2);
        CHECK(repo.items()[0].addedAt == 99);
        repo.setSort(Sort::TitleAsc);
        repo.setFilter(Filter::Series);
        CHECK(repo.remove("tt1", "series"));
        CHECK_FALSE(repo.remove("tt1", "series"));
        Store::flush();
    }
    LibraryRepository repo({std::make_shared<Store>(path), nullptr});
    CHECK(repo.items().size() == 1);
    CHECK(repo.contains("tt1", "movie"));
    CHECK(repo.sort() == Sort::TitleAsc);
    CHECK(repo.filter() == Filter::Series);
}
