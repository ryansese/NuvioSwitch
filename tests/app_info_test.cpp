#include <doctest.h>

#include "core/app_info.hpp"
#include "version.hpp"

TEST_CASE("formatVersionText joins name and version")
{
    CHECK(nuvio::formatVersionText("NuvioSwitch", "0.1.0") == "NuvioSwitch 0.1.0");
}

TEST_CASE("formatVersionText without a version is just the name")
{
    CHECK(nuvio::formatVersionText("NuvioSwitch", "") == "NuvioSwitch");
}

TEST_CASE("appVersionText uses the version from CMakeLists.txt")
{
    CHECK(nuvio::appVersionText() == std::string("NuvioSwitch ") + NUVIO_APP_VERSION);
}
