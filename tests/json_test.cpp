#include <doctest.h>

#include "core/json/json.hpp"

using namespace nuvio::core::json;

TEST_CASE("str and optStr read strings only")
{
    Json j = Json::parse(R"({"name": "Cinemeta", "year": 1994, "empty": null})");
    CHECK(str(j, "name") == "Cinemeta");
    CHECK(str(j, "year", "?") == "?");
    CHECK(str(j, "missing") == "");
    CHECK(optStr(j, "name") == std::optional<std::string>("Cinemeta"));
    CHECK_FALSE(optStr(j, "empty").has_value());
    CHECK_FALSE(optStr(j, "year").has_value());
    CHECK_FALSE(optStr(Json::array(), "name").has_value());
}

// NuvioTV MetadataFieldMappers.coerceStringList
TEST_CASE("strList accepts a string, an array or an object with a name")
{
    Json j = Json::parse(R"({
        "single": "Drama",
        "list": ["Drama", " Crime ", "", 3, null, {"name": "Thriller"}, {"id": 1}],
        "object": {"name": "Morgan Freeman"},
        "blankObject": {"name": "  "},
        "number": 5
    })");
    CHECK(strList(j, "single") == std::vector<std::string>{"Drama"});
    CHECK(strList(j, "list") == std::vector<std::string>{"Drama", "Crime", "Thriller"});
    CHECK(strList(j, "object") == std::vector<std::string>{"Morgan Freeman"});
    CHECK(strList(j, "blankObject").empty());
    CHECK(strList(j, "number").empty());
    CHECK(strList(j, "missing").empty());
}

// NuvioTV AddonMapper parseCatalogExtras optionsLimit
TEST_CASE("optInt accepts numbers and numeric strings")
{
    Json j = Json::parse(R"({"n": 20, "f": 2.9, "s": "15", "spaced": " 7 ", "bad": "15abc", "word": "ten", "b": true})");
    CHECK(optInt(j, "n") == std::optional<int64_t>(20));
    CHECK(optInt(j, "f") == std::optional<int64_t>(2));
    CHECK(optInt(j, "s") == std::optional<int64_t>(15));
    CHECK(optInt(j, "spaced") == std::optional<int64_t>(7));
    CHECK_FALSE(optInt(j, "bad").has_value());
    CHECK_FALSE(optInt(j, "word").has_value());
    CHECK_FALSE(optInt(j, "b").has_value());
    CHECK_FALSE(optInt(j, "missing").has_value());
}

// NuvioTV AddonMapper parseCatalogExtras isRequired
TEST_CASE("optBool accepts bools, true/false strings and numbers")
{
    Json j = Json::parse(R"({"t": true, "f": false, "s": "TRUE", "sf": "false", "one": 1, "zero": 0, "other": "yes"})");
    CHECK(optBool(j, "t") == std::optional<bool>(true));
    CHECK(optBool(j, "f") == std::optional<bool>(false));
    CHECK(optBool(j, "s") == std::optional<bool>(true));
    CHECK(optBool(j, "sf") == std::optional<bool>(false));
    CHECK(optBool(j, "one") == std::optional<bool>(true));
    CHECK(optBool(j, "zero") == std::optional<bool>(false));
    CHECK_FALSE(optBool(j, "other").has_value());
    CHECK_FALSE(optBool(j, "missing").has_value());
}
