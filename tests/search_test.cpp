#include <doctest.h>

#include "data/catalog/catalog_url.hpp"
#include "data/search/search_history.hpp"
#include "data/search/search_targets.hpp"

using namespace nuvio;
using namespace nuvio::data::search;
using domain::Addon;
using domain::CatalogDescriptor;
using domain::CatalogExtra;

namespace
{
CatalogDescriptor catalogWith(const std::string& id, std::vector<CatalogExtra> extra)
{
    CatalogDescriptor catalog;
    catalog.type  = "movie";
    catalog.id    = id;
    catalog.name  = id;
    catalog.extra = std::move(extra);
    return catalog;
}

CatalogExtra extra(const std::string& name, bool required = false)
{
    CatalogExtra e;
    e.name       = name;
    e.isRequired = required;
    return e;
}

Addon addonWith(const std::string& id, std::vector<CatalogDescriptor> catalogs)
{
    Addon addon;
    addon.id          = id;
    addon.displayName = id;
    addon.baseUrl     = "https://" + id;
    addon.catalogs    = std::move(catalogs);
    return addon;
}

std::vector<std::string> keys(const std::vector<domain::CatalogSource>& sources)
{
    std::vector<std::string> out;
    for (const auto& s : sources)
        out.push_back(s.key());
    return out;
}
} // namespace

TEST_CASE("searchTargets picks catalogs that support search and need nothing else")
{
    std::vector<Addon> addons = {addonWith("a",
        {catalogWith("top", {extra("skip")}),
            catalogWith("search-movies", {extra("search")}),
            catalogWith("genre-search", {extra("search"), extra("genre", true)}),
            catalogWith("only-search", {extra("search", true)})})};
    CHECK(keys(searchTargets(addons)) == std::vector<std::string>{"a_movie_search-movies", "a_movie_only-search"});
}

TEST_CASE("searchTargets reads the short extraSupported and extraRequired forms")
{
    CatalogDescriptor shortForm;
    shortForm.type           = "series";
    shortForm.id             = "s";
    shortForm.extraSupported = {"search", "skip"};
    CatalogDescriptor requiresGenre;
    requiresGenre.type           = "series";
    requiresGenre.id             = "g";
    requiresGenre.extraSupported = {"search", "genre"};
    requiresGenre.extraRequired  = {"genre"};
    CHECK(keys(searchTargets({addonWith("a", {shortForm, requiresGenre})})) == std::vector<std::string>{"a_series_s"});
}

TEST_CASE("searchTargets skips disabled add-ons and keeps add-on then catalog order")
{
    Addon off = addonWith("off", {catalogWith("c", {extra("search")})});
    off.enabled = false;
    std::vector<Addon> addons = {addonWith("b", {catalogWith("y", {extra("search")}), catalogWith("x", {extra("search")})}),
        off, addonWith("a", {catalogWith("z", {extra("search")})})};
    CHECK(keys(searchTargets(addons)) == std::vector<std::string>{"b_movie_y", "b_movie_x", "a_movie_z"});
    CHECK(searchTargets({}).empty());
}

TEST_CASE("search catalog URL")
{
    CHECK(data::catalog::buildCatalogUrl("https://example.com/abc", "series", "imdb-search", 0, searchExtras("the office")) ==
          "https://example.com/abc/catalog/series/imdb-search/search=the%20office.json");
    CHECK(data::catalog::buildCatalogUrl("https://x.io/cfg?token=a", "movie", "s", 0, searchExtras("a&b")) ==
          "https://x.io/cfg/catalog/movie/s/search=a%26b.json?token=a");
}

TEST_CASE("query rules")
{
    CHECK(trimQuery("  dune \n") == "dune");
    CHECK_FALSE(isSearchableQuery("d"));
    CHECK_FALSE(isSearchableQuery("  d "));
    CHECK(isSearchableQuery("du"));
    CHECK(isSearchableQuery("é é")); // 3 characters
    CHECK_FALSE(isSearchableQuery("é"));  // 2 bytes, 1 character
}

TEST_CASE("SearchHistory: newest first, case-insensitive de-duplication, cap of 8")
{
    SearchHistory h;
    CHECK(h.add("  Dune "));
    CHECK(h.add("alien"));
    CHECK(h.entries() == std::vector<std::string>{"alien", "Dune"});

    CHECK(h.add("DUNE")); // moves to the top with its new spelling
    CHECK(h.entries() == std::vector<std::string>{"DUNE", "alien"});

    CHECK_FALSE(h.add("x"));
    CHECK_FALSE(h.add("   "));

    for (int i = 0; i < 10; i++)
        h.add("query " + std::to_string(i));
    CHECK(h.entries().size() == MAX_HISTORY_ENTRIES);
    CHECK(h.entries().front() == "query 9");
    CHECK(h.entries().back() == "query 2");
}

TEST_CASE("SearchHistory: the eighth entry is replaced, not duplicated")
{
    SearchHistory h;
    for (int i = 0; i < 8; i++)
        h.add("title " + std::to_string(i));
    h.add("TITLE 0"); // already there: the list stays at 8
    CHECK(h.entries().size() == 8);
    CHECK(h.entries().front() == "TITLE 0");
    CHECK(h.entries().back() == "title 1");
}

TEST_CASE("SearchHistory remove, clear and JSON round trip")
{
    SearchHistory h;
    h.add("one");
    h.add("two");
    h.add("three");
    CHECK(h.remove(1));
    CHECK(h.entries() == std::vector<std::string>{"three", "one"});
    CHECK_FALSE(h.remove(5));

    auto restored = SearchHistory::fromJson(h.toJson());
    CHECK(restored.entries() == h.entries());

    CHECK(SearchHistory::fromJson(nlohmann::json("not an array")).entries().empty());
    CHECK(SearchHistory::fromJson(nlohmann::json::parse(R"(["ab", 5, "cd", "x"])")).entries() ==
          std::vector<std::string>{"ab", "cd"});

    CHECK(h.clear());
    CHECK_FALSE(h.clear());
    CHECK(h.entries().empty());
}
