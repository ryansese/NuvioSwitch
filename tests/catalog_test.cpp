#include <doctest.h>

#include "data/catalog/catalog_parser.hpp"
#include "data/catalog/catalog_url.hpp"
#include "domain/catalog_paging.hpp"
#include "test_util.hpp"

using namespace nuvio;
using namespace nuvio::data::catalog;
using domain::CatalogDescriptor;
using domain::CatalogExtra;
using domain::PosterShape;

namespace
{
const std::string CINEMETA = "https://v3-cinemeta.strem.io";

CatalogDescriptor catalogWith(std::vector<CatalogExtra> extra, std::optional<int> pageSize = std::nullopt)
{
    CatalogDescriptor catalog;
    catalog.type     = "movie";
    catalog.id       = "top";
    catalog.name     = "Popular";
    catalog.extra    = std::move(extra);
    catalog.pageSize = pageSize;
    return catalog;
}

CatalogExtra skipWithOptions(std::vector<std::string> options)
{
    CatalogExtra extra;
    extra.name    = "skip";
    extra.options = std::move(options);
    return extra;
}
} // namespace

TEST_CASE("Catalog URL without extras")
{
    CHECK(buildCatalogUrl(CINEMETA, "movie", "top", 0) == "https://v3-cinemeta.strem.io/catalog/movie/top.json");
    CHECK(buildCatalogUrl(CINEMETA + "/", "movie", "top", 0) == "https://v3-cinemeta.strem.io/catalog/movie/top.json");
}

TEST_CASE("Catalog URL with skip only")
{
    CHECK(buildCatalogUrl(CINEMETA, "movie", "top", 100) ==
          "https://v3-cinemeta.strem.io/catalog/movie/top/skip=100.json");
}

TEST_CASE("Catalog URL with extras: encoding, skip last, query after .json")
{
    CHECK(buildCatalogUrl("https://example.com?token=X", "movie", "picks", 50, {{"genre", "Science Fiction"}}) ==
          "https://example.com/catalog/movie/picks/genre=Science%20Fiction&skip=50.json?token=X");

    // skip given as an extra stays where it is and isn't added again
    CHECK(buildCatalogUrl(CINEMETA, "movie", "top", 50, {{"skip", "20"}, {"genre", "Drama"}}) ==
          "https://v3-cinemeta.strem.io/catalog/movie/top/skip=20&genre=Drama.json");

    // no skip at 0; URLEncoder's reserved set
    CHECK(buildCatalogUrl(CINEMETA, "series", "top", 0, {{"search", "Tom & Jerry: 100%"}}) ==
          "https://v3-cinemeta.strem.io/catalog/series/top/search=Tom%20%26%20Jerry%3A%20100%25.json");
    CHECK(encodeArg("a.b-c*d_e~f/é") == "a.b-c*d_e%7Ef%2F%C3%A9");

    // type and id are inserted as they are
    CHECK(buildCatalogUrl("https://a.example/sub/?x=1", "anime", "kitsu top", 0) ==
          "https://a.example/sub/catalog/anime/kitsu top.json?x=1");
}

TEST_CASE("Cinemeta page parses with ratings, artwork and source add-on")
{
    auto parsed = parseCatalog(readFixture("catalog/cinemeta_top_movie.json"), "movie", CINEMETA);
    REQUIRE(parsed);
    CHECK(parsed->rawCount == 12);
    REQUIRE(parsed->items.size() == 12);
    const auto& first = parsed->items[0];
    CHECK(first.id == "tt14173636");
    CHECK(first.name == "The Invite");
    CHECK(first.type == "movie");
    CHECK(first.imdbRating == doctest::Approx(7.5f));
    CHECK(first.hasHeroArtwork());
    CHECK(first.poster);
    CHECK(first.posterShape == PosterShape::Poster);
    CHECK(first.sourceAddonBaseUrl == CINEMETA);
    // an empty imdbRating string is no rating
    CHECK_FALSE(parsed->items[3].imdbRating);
}

TEST_CASE("Spec scenario: 20 metas, two without a name and one repeated id, give 17 items")
{
    auto parsed = parseCatalog(readFixture("catalog/messy.json"), "movie", "https://a.example");
    REQUIRE(parsed);
    CHECK(parsed->rawCount == 20);
    REQUIRE(parsed->items.size() == 17);

    const auto& first = parsed->items[0];
    CHECK(first.type == "series");
    CHECK(first.imdbRating == doctest::Approx(7.5f));
    CHECK(first.posterShape == PosterShape::Landscape);
    CHECK(first.genres == std::vector<std::string>{"Drama", "1999"});

    CHECK(parsed->items[1].imdbRating == doctest::Approx(8.1f)); // a number is accepted too
    CHECK(parsed->items[1].name == "Item 2");                     // the first of a repeated id wins
    CHECK_FALSE(parsed->items[2].imdbRating);                     // "n/a"
    CHECK(parsed->items[2].type == "movie");                      // blank type falls back
}

TEST_CASE("Unusable catalog responses")
{
    CHECK_FALSE(parseCatalog(std::string("<html>"), "movie", ""));
    CHECK_FALSE(parseCatalog(std::string("[]"), "movie", ""));
    auto empty = parseCatalog(std::string(R"({"metas":"nope"})"), "movie", "");
    REQUIRE(empty);
    CHECK(empty->items.empty());
    CHECK(empty->rawCount == 0);
    auto noMetas = parseCatalog(std::string(R"({"metas":[null, 3, {"id":"a"}]})"), "movie", "");
    REQUIRE(noMetas);
    CHECK(noMetas->rawCount == 3);
    CHECK(noMetas->items.empty());
}

TEST_CASE("skipStep: pageSize, then skip options, then 100")
{
    CHECK(domain::skipStep(catalogWith({}, 20)) == 20);
    CHECK(domain::skipStep(catalogWith({skipWithOptions({"0", "50", "100"})}, 0)) == 50);
    CHECK(domain::skipStep(catalogWith({skipWithOptions({"100", " 0 ", "25", "x", "-5", "25"})})) == 25);
    CHECK(domain::skipStep(catalogWith({skipWithOptions({"0"})})) == domain::DEFAULT_SKIP_STEP);
    CHECK(domain::skipStep(catalogWith({})) == 100);
}

TEST_CASE("nextPage follows skip support and empty pages")
{
    auto paged = catalogWith({skipWithOptions({})});
    auto first = domain::nextPage(paged, 0, 100);
    CHECK(first.hasMore);
    CHECK(first.skip == 100);

    auto end = domain::nextPage(paged, 100, 0);
    CHECK_FALSE(end.hasMore);
    CHECK(end.skip == 100);

    // short form counts too
    auto shortForm           = catalogWith({});
    shortForm.extraSupported = {"skip"};
    CHECK(domain::nextPage(shortForm, 0, 20).hasMore);

    CHECK_FALSE(domain::nextPage(catalogWith({}), 0, 100).hasMore);
}
