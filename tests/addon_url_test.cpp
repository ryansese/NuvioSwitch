#include <doctest.h>

#include "data/addons/addon_url.hpp"

using namespace nuvio::data::addons;

TEST_CASE("canonicalize drops trailing slashes and /manifest.json in any case")
{
    CHECK(canonicalize("https://example.com/abc/manifest.json") == "https://example.com/abc");
    CHECK(canonicalize("https://example.com/abc/") == "https://example.com/abc");
    CHECK(canonicalize("  https://example.com/abc//  ") == "https://example.com/abc");
    CHECK(canonicalize("https://example.com/abc/Manifest.JSON") == "https://example.com/abc");
    CHECK(canonicalize("https://example.com/abc/manifest.json/") == "https://example.com/abc");
    CHECK(canonicalize("https://v3-cinemeta.strem.io") == "https://v3-cinemeta.strem.io");
}

TEST_CASE("canonicalize keeps the query string")
{
    CHECK(canonicalize("https://example.com/manifest.json?token=X") == "https://example.com?token=X");
    CHECK(canonicalize("https://example.com/abc/?token=X") == "https://example.com/abc?token=X");
    CHECK(canonicalize("https://example.com/abc?token=X/") == "https://example.com/abc?token=X");
}

TEST_CASE("manifestUrl puts /manifest.json before the query")
{
    CHECK(manifestUrl("https://example.com/abc") == "https://example.com/abc/manifest.json");
    CHECK(manifestUrl("https://example.com/abc/manifest.json") == "https://example.com/abc/manifest.json");
    CHECK(manifestUrl("https://example.com?token=X") == "https://example.com/manifest.json?token=X");
    CHECK(manifestUrl("https://example.com/manifest.json?token=X") == "https://example.com/manifest.json?token=X");
}

TEST_CASE("normalizeForCompare ignores case and the manifest suffix")
{
    CHECK(normalizeForCompare("https://Example.com/abc/") == normalizeForCompare("https://example.com/abc"));
    CHECK(normalizeForCompare("https://example.com/ABC/manifest.json") == "https://example.com/abc");
}

TEST_CASE("normalizeInstallInput accepts http, https and stremio")
{
    CHECK(normalizeInstallInput("https://example.com/manifest.json") == "https://example.com");
    CHECK(normalizeInstallInput(" http://example.com/abc/ ") == "http://example.com/abc");
    CHECK(normalizeInstallInput("stremio://example.com/manifest.json") == "https://example.com");
    CHECK(normalizeInstallInput("stremio://example.com/a/manifest.json?x=1") == "https://example.com/a?x=1");
}

TEST_CASE("normalizeInstallInput rejects text without a scheme or host")
{
    CHECK_FALSE(normalizeInstallInput("example.com"));
    CHECK_FALSE(normalizeInstallInput("ftp://example.com"));
    CHECK_FALSE(normalizeInstallInput(""));
    CHECK_FALSE(normalizeInstallInput("https://"));
}

TEST_CASE("hostOf keeps only scheme and host")
{
    CHECK(hostOf("https://example.com/secret-token/manifest.json") == "https://example.com");
    CHECK(hostOf("https://example.com?token=X") == "https://example.com");
    CHECK(hostOf("https://example.com") == "https://example.com");
}
