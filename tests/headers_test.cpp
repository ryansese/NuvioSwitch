#include <doctest.h>

#include "core/net/headers.hpp"

using namespace nuvio::core::net;

TEST_CASE("Accept-Language adds English as a fallback for other locales")
{
    CHECK(buildAcceptLanguage("it-IT") == "it-IT,en;q=0.7");
    CHECK(buildAcceptLanguage("pt-BR") == "pt-BR,en;q=0.7");
}

TEST_CASE("Accept-Language for English locales is the locale alone")
{
    CHECK(buildAcceptLanguage("en-US") == "en-US");
    CHECK(buildAcceptLanguage("en") == "en");
    CHECK(buildAcceptLanguage("EN-gb") == "EN-gb");
}

TEST_CASE("Accept-Language for an unknown locale is en")
{
    CHECK(buildAcceptLanguage("") == "en");
    CHECK(buildAcceptLanguage("und") == "en");
}

TEST_CASE("default headers carry the app version and locale")
{
    Headers headers = defaultHeaders("0.1.0", "it-IT");
    CHECK(headers["User-Agent"] == "NuvioSwitch/0.1.0");
    CHECK(headers["Accept-Language"] == "it-IT,en;q=0.7");
}

TEST_CASE("request headers override defaults of the same name, ignoring case")
{
    Headers merged = mergeHeaders(defaultHeaders("0.1.0", "en-US"), {{"user-agent", "Custom"}, {"X-Extra", "1"}});
    CHECK(merged.size() == 3);
    CHECK(merged["User-Agent"] == "Custom");
    CHECK(merged["x-extra"] == "1");
    CHECK(merged["Accept-Language"] == "en-US");
}
