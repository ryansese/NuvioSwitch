#include <doctest.h>

#include "data/addons/manifest_parser.hpp"
#include "data/catalog/home_selection.hpp"
#include "test_util.hpp"

using namespace nuvio;
using namespace nuvio::data::catalog;
using domain::Addon;
using domain::CatalogDescriptor;
using domain::MetaPreview;

namespace
{
using Strings = std::vector<std::string>;

CatalogDescriptor catalog(const std::string& type, const std::string& id, const std::string& name)
{
    CatalogDescriptor c;
    c.type = type;
    c.id   = id;
    c.name = name;
    return c;
}

Addon addon(const std::string& id, std::vector<CatalogDescriptor> catalogs, bool enabled = true)
{
    Addon a;
    a.id          = id;
    a.name        = id;
    a.displayName = id;
    a.baseUrl     = "https://" + id + ".example";
    a.catalogs    = std::move(catalogs);
    a.enabled     = enabled;
    return a;
}

Strings keys(const std::vector<domain::CatalogSource>& sources)
{
    Strings out;
    for (const auto& source : sources)
        out.push_back(source.key());
    return out;
}

MetaPreview item(const std::string& id, bool artwork)
{
    MetaPreview m;
    m.id   = id;
    m.name = id;
    if (artwork)
        m.background = "https://img.example/" + id + ".jpg";
    return m;
}

Strings ids(const std::vector<MetaPreview>& items)
{
    Strings out;
    for (const auto& m : items)
        out.push_back(m.id);
    return out;
}
} // namespace

TEST_CASE("Spec scenario: default add-ons show Cinemeta's catalogs in manifest order")
{
    auto cinemeta = data::addons::parseManifest(readFixture("cinemeta.json"), "https://v3-cinemeta.strem.io");
    auto subs     = data::addons::parseManifest(readFixture("opensubtitles_v3.json"), "https://opensubtitles-v3.strem.io");
    REQUIRE(cinemeta);
    REQUIRE(subs);

    auto selected = selectHomeCatalogs({*cinemeta, *subs});
    // the year catalogs require "genre" but not "search", so they stay, as in NuvioTV
    CHECK(keys(selected) == Strings{"com.linvo.cinemeta_movie_top", "com.linvo.cinemeta_series_top",
                                "com.linvo.cinemeta_movie_year", "com.linvo.cinemeta_series_year",
                                "com.linvo.cinemeta_movie_imdbRating", "com.linvo.cinemeta_series_imdbRating",
                                "com.linvo.cinemeta_series_last-videos", "com.linvo.cinemeta_series_calendar-videos"});
    CHECK(selected[0].addonName == "Cinemeta");
    CHECK(selected[0].addonBaseUrl == "https://v3-cinemeta.strem.io");
}

TEST_CASE("Search-only and showInHome:false catalogs are left out")
{
    auto searchOnly = catalog("movie", "search", "Search");
    searchOnly.extra.push_back({"search", true});
    auto optionalSearch = catalog("movie", "browse", "Browse");
    optionalSearch.extra.push_back({"search", false});
    auto hidden                  = catalog("movie", "hidden", "Hidden");
    hidden.hasExplicitShowInHome = true;
    hidden.showInHome            = false;
    auto shown                   = catalog("movie", "shown", "Shown");
    shown.hasExplicitShowInHome  = true;
    shown.showInHome             = true;

    CHECK(keys(selectHomeCatalogs({addon("a", {searchOnly, optionalSearch, hidden, shown})})) ==
          Strings{"a_movie_browse", "a_movie_shown"});
}

TEST_CASE("Order follows add-on priority; disabled add-ons and repeated keys are skipped")
{
    auto a = addon("a", {catalog("movie", "one", "One"), catalog("series", "two", "Two")});
    auto b = addon("b", {catalog("movie", "three", "Three")});
    auto c = addon("c", {catalog("movie", "four", "Four")}, false);
    auto dup = addon("a", {catalog("movie", "one", "One again"), catalog("movie", "five", "Five")});

    CHECK(keys(selectHomeCatalogs({a, b, c, dup})) ==
          Strings{"a_movie_one", "a_series_two", "b_movie_three", "a_movie_five"});
    CHECK(keys(selectHomeCatalogs({b, a})) == Strings{"b_movie_three", "a_movie_one", "a_series_two"});
}

TEST_CASE("Row titles")
{
    CHECK(rowTitle(catalog("movie", "top", "popular")) == "Popular - Movie");
    CHECK(rowTitle(catalog("series", "top", "Popular")) == "Popular - Series");
    CHECK(rowTitle(catalog("anime", "x", "Trending")) == "Trending - Anime");
    CHECK(rowTitle(catalog("Movie", "x", "Top"), "Film", "Serie") == "Top - Film");
}

TEST_CASE("Spec scenario: hero takes 4 items from the first row and 3 from the second")
{
    std::vector<MetaPreview> first{item("a1", false), item("a2", true), item("a3", true), item("a4", true),
        item("a5", true), item("a6", true)};
    std::vector<MetaPreview> second{item("b1", true), item("b2", false), item("b3", true), item("b4", true)};

    auto hero = pickHeroItems({first, second});
    CHECK(ids(hero) == Strings{"a2", "a3", "a4", "a5", "b1", "b3", "b4"});
}

TEST_CASE("Hero fills with artwork-less items, skips repeats and copes with many or no rows")
{
    // artwork-less items fill what artwork can't
    CHECK(ids(pickHeroItems({{item("a", true), item("b", false), item("c", false)}}, 3)) == Strings{"a", "b", "c"});

    // an item already picked from an earlier row isn't picked again
    CHECK(ids(pickHeroItems({{item("x", true)}, {item("x", true), item("y", true)}}, 2)) == Strings{"x", "y"});

    // more rows than slots: the first rows get one each
    std::vector<std::vector<MetaPreview>> nine;
    for (int i = 0; i < 9; i++)
        nine.push_back({item("r" + std::to_string(i), true)});
    CHECK(pickHeroItems(nine).size() == 7);

    CHECK(pickHeroItems({}).empty());
}
