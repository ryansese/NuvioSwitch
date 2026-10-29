#include <doctest.h>

#include "data/addons/manifest_parser.hpp"
#include "test_util.hpp"

using namespace nuvio::data::addons;
using namespace nuvio::domain;

namespace
{
const AddonResource* findResource(const Addon& addon, const std::string& name)
{
    for (const auto& resource : addon.resources)
        if (resource.name == name)
            return &resource;
    return nullptr;
}

const CatalogDescriptor* findCatalog(const Addon& addon, const std::string& type, const std::string& id)
{
    for (const auto& catalog : addon.catalogs)
        if (catalog.type == type && catalog.id == id)
            return &catalog;
    return nullptr;
}

using Strings = std::vector<std::string>;
} // namespace

TEST_CASE("Cinemeta manifest")
{
    auto addon = parseManifest(readFixture("cinemeta.json"), "https://v3-cinemeta.strem.io");
    REQUIRE(addon);
    CHECK(addon->id == "com.linvo.cinemeta");
    CHECK(addon->name == "Cinemeta");
    CHECK(addon->displayName == "Cinemeta");
    CHECK(addon->baseUrl == "https://v3-cinemeta.strem.io");
    CHECK(addon->types == Strings{"movie", "series"});
    CHECK(addon->idPrefixes == Strings{"tt"});
    CHECK_FALSE(addon->placeholder);

    // string resources inherit the manifest types and have no prefixes
    REQUIRE(addon->resources.size() == 3);
    const auto* meta = findResource(*addon, "meta");
    REQUIRE(meta);
    CHECK(meta->types == Strings{"movie", "series"});
    CHECK(meta->idPrefixes.empty());

    const auto* top = findCatalog(*addon, "movie", "top");
    REQUIRE(top);
    CHECK(top->name == "Popular");
    CHECK(top->supportsExtra("search"));
    CHECK(top->supportsExtra("Genre"));
    CHECK_FALSE(top->hasExplicitShowInHome);
    REQUIRE(top->extra.size() == 3);
    CHECK(top->extra[0].name == "genre");
    REQUIRE(top->extra[0].options);
    CHECK(top->extra[0].options->front() == "Action");

    const auto* year = findCatalog(*addon, "movie", "year");
    REQUIRE(year);
    CHECK(year->extra[0].isRequired);
    CHECK(year->extraRequired == Strings{"genre"});

    // extra names are lowercased, as NuvioTV does
    const auto* lastVideos = findCatalog(*addon, "series", "last-videos");
    REQUIRE(lastVideos);
    CHECK(lastVideos->extra[0].name == "lastvideosids");
    CHECK(lastVideos->extra[0].optionsLimit == 100);
}

TEST_CASE("OpenSubtitles v3 manifest")
{
    auto addon = parseManifest(readFixture("opensubtitles_v3.json"), "https://opensubtitles-v3.strem.io");
    REQUIRE(addon);
    CHECK(addon->id == "org.stremio.opensubtitlesv3");
    CHECK(addon->catalogs.empty());
    REQUIRE(addon->resources.size() == 1);
    CHECK(addon->resources[0].name == "subtitles");
    CHECK(addon->resources[0].types == Strings{"movie", "series"});
}

TEST_CASE("Configurable add-on: object resources, both extra forms, showInHome")
{
    auto addon = parseManifest(readFixture("configurable.json"), "https://example.com?token=X");
    REQUIRE(addon);
    CHECK(addon->baseUrl == "https://example.com?token=X");
    CHECK(addon->types == Strings{"movie", "series", "anime"});
    CHECK(addon->logo == "https://example.com/logo.png");
    REQUIRE(addon->behaviorHints);
    CHECK(addon->behaviorHints->configurable == true);
    CHECK(addon->behaviorHints->configurationRequired == false);
    CHECK_FALSE(addon->behaviorHints->newEpisodeNotifications);

    const auto* stream = findResource(*addon, "stream");
    REQUIRE(stream);
    CHECK(stream->types == Strings{"movie", "series"});
    CHECK(stream->idPrefixes == Strings{"tt"});

    // an object without types inherits the manifest's
    const auto* subtitle = findResource(*addon, "subtitle");
    REQUIRE(subtitle);
    CHECK(subtitle->types == Strings{"movie", "series", "anime"});

    const auto* trending = findCatalog(*addon, "anime", "kitsu-trending");
    REQUIRE(trending);
    CHECK(trending->showInHome);
    CHECK(trending->hasExplicitShowInHome);
    CHECK(trending->pageSize == 20);
    // " Skip " twice collapses to one; blank names are dropped
    REQUIRE(trending->extra.size() == 3);
    CHECK(trending->extra[0].name == "skip");
    CHECK(trending->extra[1].name == "genre");
    CHECK(trending->extra[1].isRequired);
    REQUIRE(trending->extra[1].options);
    CHECK(*trending->extra[1].options == Strings{"Action", "1999"});
    CHECK(trending->extra[1].defaultValue == "Action");
    CHECK(trending->extra[1].optionsLimit == 1);
    CHECK(trending->extra[2].name == "search");
    CHECK_FALSE(trending->extra[2].isRequired);
    CHECK_FALSE(trending->extra[2].options);
    CHECK_FALSE(trending->extra[2].defaultValue);

    const auto* recent = findCatalog(*addon, "movie", "recent");
    REQUIRE(recent);
    CHECK_FALSE(recent->showInHome);
    CHECK(recent->hasExplicitShowInHome);
    CHECK(recent->extra.empty());
    CHECK(recent->supportsExtra("search"));
    CHECK(recent->supportsExtra("skip"));
    CHECK_FALSE(recent->supportsExtra("genre"));

    const auto* required = findCatalog(*addon, "series", "required-number");
    REQUIRE(required);
    CHECK(required->extra[0].isRequired);
}

TEST_CASE("Malformed entries are skipped, not fatal")
{
    auto addon = parseManifest(readFixture("malformed_entries.json"), "https://sloppy.example");
    REQUIRE(addon);
    CHECK_FALSE(addon->description);
    CHECK_FALSE(addon->logo);
    CHECK_FALSE(addon->behaviorHints);
    CHECK(addon->types == Strings{"movie"});

    // 42, null and the nameless object are dropped; a non-list "types" falls back
    REQUIRE(addon->resources.size() == 2);
    CHECK(addon->resources[0].name == "stream");
    CHECK(addon->resources[0].types == Strings{"movie"});
    CHECK(addon->resources[1].name == "meta");

    REQUIRE(addon->catalogs.size() == 1);
    CHECK(addon->catalogs[0].id == "ok");
    CHECK(addon->catalogs[0].extra.empty());
    CHECK_FALSE(addon->catalogs[0].pageSize);
}

TEST_CASE("A manifest without id, name or version is rejected")
{
    CHECK_FALSE(parseManifest(readFixture("missing_id.json"), "https://x.example"));
    CHECK_FALSE(parseManifest(std::string(R"({"id":"a","name":"A"})"), "https://x.example"));
    CHECK_FALSE(parseManifest(std::string(R"({"id":"a","name":"A","version":1})"), "https://x.example"));
    CHECK_FALSE(parseManifest(std::string("<html>Not Found</html>"), "https://x.example"));
    CHECK_FALSE(parseManifest(std::string("[]"), "https://x.example"));
}

TEST_CASE("Spec scenario: mixed resources")
{
    auto addon = parseManifest(
        std::string(R"({"id":"a","name":"A","version":"1",
            "types":["movie","series"],
            "resources":["catalog",{"name":"stream","types":["movie"],"idPrefixes":["tt"]}]})"),
        "https://a.example");
    REQUIRE(addon);
    REQUIRE(addon->resources.size() == 2);
    CHECK(addon->resources[0].name == "catalog");
    CHECK(addon->resources[0].types == Strings{"movie", "series"});
    CHECK(addon->resources[0].idPrefixes.empty());
    CHECK(addon->resources[1].types == Strings{"movie"});
    CHECK(addon->resources[1].idPrefixes == Strings{"tt"});
}
