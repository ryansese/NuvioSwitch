include(FetchContent)

FetchContent_Declare(doctest
    URL https://github.com/doctest/doctest/archive/refs/tags/v2.4.11.tar.gz
    URL_HASH SHA256=632ed2c05a7f53fa961381497bf8069093f0d6628c5f26286161fbd32a560186
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    # header-only: point at a folder with no CMakeLists so doctest's own build is skipped
    SOURCE_SUBDIR header-only
)
FetchContent_MakeAvailable(doctest)

# Pure logic only: src/core, src/domain and src/data are compiled without borealis or curl.
# The glue that needs them (curl_*.cpp, *_brls.cpp) is left out.
file(GLOB TEST_SRC ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
file(GLOB_RECURSE CORE_SRC
    ${PROJECT_SOURCE_DIR}/src/core/*.cpp
    ${PROJECT_SOURCE_DIR}/src/domain/*.cpp
    ${PROJECT_SOURCE_DIR}/src/data/*.cpp)
list(FILTER CORE_SRC EXCLUDE REGEX "/(curl_[^/]*|[^/]*_brls)\\.cpp$")

add_executable(NuvioSwitchTests ${TEST_SRC} ${CORE_SRC})
set_target_properties(NuvioSwitchTests PROPERTIES CXX_STANDARD 17)
target_include_directories(NuvioSwitchTests PRIVATE
    ${PROJECT_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}/generated
    ${NLOHMANN_INCLUDE}
    ${doctest_SOURCE_DIR}/doctest
)
find_package(Threads REQUIRED)
target_compile_definitions(NuvioSwitchTests PRIVATE NUVIO_TEST_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/")
target_link_libraries(NuvioSwitchTests PRIVATE Threads::Threads PkgConfig::WEBP)

add_test(NAME NuvioSwitchTests COMMAND NuvioSwitchTests)
