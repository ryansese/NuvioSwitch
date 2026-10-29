#include <doctest.h>

#include "data/addons/addon_display.hpp"
#include "data/addons/addon_query.hpp"

using namespace nuvio::data::addons;
using nuvio::domain::Addon;
using nuvio::domain::AddonResource;

namespace
{
Addon named(const std::string& name, const std::string& url)
{
    Addon addon;
    addon.id      = name;
    addon.name    = name;
    addon.baseUrl = url;
    return addon;
}

Addon withResources(const std::string& url, std::vector<AddonResource> resources,
    std::vector<std::string> manifestPrefixes = {})
{
    Addon addon      = named(url, url);
    addon.resources  = std::move(resources);
    addon.idPrefixes = std::move(manifestPrefixes);
    return addon;
}

std::vector<std::string> urls(const std::vector<Addon>& addons)
{
    std::vector<std::string> out;
    for (const auto& addon : addons)
        out.push_back(addon.baseUrl);
    return out;
}

using Strings = std::vector<std::string>;
} // namespace

TEST_CASE("Duplicate names get (2), (3) in list order")
{
    std::vector<Addon> addons{named("Torrentio", "https://a"), named("Cinemeta", "https://b"),
        named("Torrentio", "https://c"), named("Torrentio", "https://d")};
    applyDisplayNames(addons, {});
    CHECK(addons[0].displayName == "Torrentio");
    CHECK(addons[1].displayName == "Cinemeta");
    CHECK(addons[2].displayName == "Torrentio (2)");
    CHECK(addons[3].displayName == "Torrentio (3)");
}

TEST_CASE("A custom name wins and takes the add-on out of the numbering")
{
    std::vector<Addon> addons{named("Torrentio", "https://a"), named("Torrentio", "https://b"),
        named("Torrentio", "https://c")};
    applyDisplayNames(addons, {{"https://a", "My debrid"}, {"https://c", "   "}});
    CHECK(addons[0].displayName == "My debrid");
    CHECK(addons[1].displayName == "Torrentio");
    CHECK(addons[2].displayName == "Torrentio (2)"); // a blank custom name is ignored

    // a custom name equal to the manifest name counts as no custom name
    std::vector<Addon> same{named("Torrentio", "https://a"), named("Torrentio", "https://b")};
    applyDisplayNames(same, {{"https://b", "Torrentio"}});
    CHECK(same[1].displayName == "Torrentio (2)");
}

TEST_CASE("applyDisplayNames is idempotent")
{
    std::vector<Addon> addons{named("Torrentio", "https://a"), named("Torrentio", "https://b")};
    applyDisplayNames(addons, {});
    applyDisplayNames(addons, {});
    CHECK(addons[1].displayName == "Torrentio (2)");
}

TEST_CASE("Resource types: empty matches every type")
{
    auto any   = withResources("https://any", {{"stream", {}, {}}});
    auto movie = withResources("https://movie", {{"stream", {"movie"}, {}}});
    CHECK(supportsResource(any, "stream", "series", "tt1"));
    CHECK(supportsResource(movie, "stream", "movie", "tt1"));
    CHECK_FALSE(supportsResource(movie, "stream", "series", "tt1"));
    CHECK_FALSE(supportsResource(movie, "meta", "movie", "tt1"));
}

TEST_CASE("Resource-level prefixes win over manifest-level ones")
{
    auto resourceLevel = withResources("https://r", {{"stream", {"movie"}, {"kitsu:"}}}, {"tt"});
    CHECK(supportsResource(resourceLevel, "stream", "movie", "kitsu:1"));
    CHECK_FALSE(supportsResource(resourceLevel, "stream", "movie", "tt0111161"));

    auto manifestLevel = withResources("https://m", {{"stream", {"movie"}, {}}}, {"tt"});
    CHECK(supportsResource(manifestLevel, "stream", "movie", "tt0111161"));
    CHECK_FALSE(supportsResource(manifestLevel, "stream", "movie", "kitsu:1"));

    auto none = withResources("https://n", {{"stream", {"movie"}, {}}});
    CHECK(supportsResource(none, "stream", "movie", "anything"));
}

TEST_CASE("Catalog queries ignore the id; subtitle is an alias of subtitles")
{
    auto catalog = withResources("https://c", {{"catalog", {"movie"}, {"tt"}}}, {"tt"});
    CHECK(supportsResource(catalog, "catalog", "movie", ""));
    CHECK(supportsResource(catalog, "catalog", "movie", "kitsu:1"));

    auto subs = withResources("https://s", {{"subtitles", {"movie"}, {}}});
    CHECK(supportsResource(subs, "subtitle", "movie", "tt1"));
    auto singular = withResources("https://s1", {{"subtitle", {"movie"}, {}}});
    CHECK(supportsResource(singular, "subtitles", "movie", "tt1"));
}

TEST_CASE("addonsFor keeps priority order and skips disabled add-ons")
{
    auto a = withResources("https://a", {{"stream", {"movie"}, {}}}, {"tt"});
    auto b = withResources("https://b", {{"stream", {"movie"}, {}}}, {"kitsu:"});
    auto c = withResources("https://c", {{"stream", {}, {}}});
    auto d = withResources("https://d", {{"stream", {}, {}}});
    d.enabled = false;
    Addon placeholder = named("e", "https://e");
    placeholder.placeholder = true;

    CHECK(urls(addonsFor({c, a, b, d, placeholder}, "stream", "movie", "tt0111161")) == Strings{"https://c", "https://a"});
}
