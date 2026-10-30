cmake_minimum_required(VERSION 3.10)

# borealis is a git submodule; fail early with a useful message if it is missing
set(BOREALIS_LIBRARY ${CMAKE_CURRENT_SOURCE_DIR}/library/borealis/library)
if (NOT EXISTS "${BOREALIS_LIBRARY}/CMakeLists.txt")
    message(FATAL_ERROR
        "borealis is not initialised (library/borealis is empty). "
        "Run: git submodule update --init --recursive")
endif ()

# build options (PLATFORM_DESKTOP, PLATFORM_SWITCH, ...)
include(${BOREALIS_LIBRARY}/cmake/commonOption.cmake)

option(USE_SHARED_LIB "Whether to use shared libs provided by system" OFF)
cmake_dependent_option(USE_SYSTEM_FMT "" OFF "NOT USE_SHARED_LIB" ON)
cmake_dependent_option(USE_SYSTEM_TINYXML2 "" OFF "NOT USE_SHARED_LIB" ON)
cmake_dependent_option(USE_SYSTEM_TWEENY "" OFF "NOT USE_SHARED_LIB" ON)

if (APPLE AND PLATFORM_DESKTOP)
    if (NOT CMAKE_OSX_ARCHITECTURES)
        set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
    endif ()
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "" FORCE)
endif ()

# toolchain (devkitA64 on Switch)
include(${BOREALIS_LIBRARY}/cmake/toolchain.cmake)

# project info: the version is defined here only
project(NuvioSwitch)
set(VERSION_MAJOR "0")
set(VERSION_MINOR "1")
set(VERSION_ALTER "0")
set(APP_VERSION "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_ALTER}")
set(PROJECT_AUTHOR "Ryan Sese")
set(PROJECT_ICON ${CMAKE_CURRENT_SOURCE_DIR}/resources/img/icon.jpg)
set(PROJECT_RESOURCES ${CMAKE_CURRENT_SOURCE_DIR}/resources)

add_subdirectory(${BOREALIS_LIBRARY} ${CMAKE_BINARY_DIR}/borealis)

find_package(Threads REQUIRED)
list(APPEND APP_PLATFORM_LIB ${CMAKE_THREAD_LIBS_INIT})

find_package(PkgConfig REQUIRED)

# libcurl: the system/Homebrew curl on desktop, the switch-curl portlib on Switch (TLS through libnx)
if (PLATFORM_SWITCH)
    pkg_check_modules(CURL REQUIRED IMPORTED_TARGET libcurl)
    list(APPEND APP_PLATFORM_LIB PkgConfig::CURL z)
else ()
    find_package(CURL REQUIRED)
    list(APPEND APP_PLATFORM_LIB CURL::libcurl)
endif ()

# libwebp: metahub posters are WebP only. switch-libwebp on Switch, Homebrew webp on macOS
pkg_check_modules(WEBP REQUIRED IMPORTED_TARGET libwebp)
list(APPEND APP_PLATFORM_LIB PkgConfig::WEBP)

# libmpv: Homebrew mpv on macOS, the switch-libmpv portlib (with its static dependencies) on Switch
pkg_check_modules(MPV IMPORTED_TARGET mpv)
if (NOT MPV_FOUND)
    message(FATAL_ERROR
        "libmpv not found. On macOS run: brew install mpv. "
        "On Switch it comes with the devkitPro portlibs: dkp-pacman -S switch-libmpv")
endif ()
if (PLATFORM_SWITCH)
    list(APPEND APP_PLATFORM_LIB ${MPV_STATIC_LDFLAGS})
    list(APPEND APP_PLATFORM_INCLUDE ${MPV_STATIC_INCLUDE_DIRS})
else ()
    list(APPEND APP_PLATFORM_LIB PkgConfig::MPV)
endif ()

# nlohmann/json, bundled with borealis
set(NLOHMANN_INCLUDE ${BOREALIS_LIBRARY}/include/borealis/extern)

# temporary network checks: a row of Cinemeta posters on Home and an HTTPS request at startup
option(NUVIO_DEBUG_NETWORK "Log an HTTPS check at startup" OFF)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/version.hpp.in ${CMAKE_BINARY_DIR}/generated/version.hpp @ONLY)

file(GLOB_RECURSE MAIN_SRC ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

if (PLATFORM_SWITCH)
    list(APPEND MAIN_SRC ${BOREALIS_LIBRARY}/lib/platforms/switch/switch_wrapper.c)
endif ()

program_target(${PROJECT_NAME} "${MAIN_SRC}")
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 17)

if (PLATFORM_DESKTOP)
    # borealis reads ./resources/ relative to the working directory
    add_custom_target(${PROJECT_NAME}.data
        COMMAND "${CMAKE_COMMAND}" -E copy_directory ${PROJECT_RESOURCES} ${CMAKE_CURRENT_BINARY_DIR}/resources
    )
    add_dependencies(${PROJECT_NAME} ${PROJECT_NAME}.data)
elseif (PLATFORM_SWITCH)
    if (USE_DEKO3D)
        gen_dksh("${PROJECT_RESOURCES}/shaders")
    endif ()
    # the console provides the system font, so resources/font is left out of romfs
    add_custom_target(${PROJECT_NAME}.nro DEPENDS ${PROJECT_NAME}
        COMMAND ${NX_NACPTOOL_EXE} --create "${PROJECT_NAME}" "${PROJECT_AUTHOR}" "${APP_VERSION}" ${PROJECT_NAME}.nacp
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_RESOURCES} ${CMAKE_BINARY_DIR}/resources
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/resources/font
        COMMAND ${NX_ELF2NRO_EXE} ${PROJECT_NAME}.elf ${PROJECT_NAME}.nro --icon=${PROJECT_ICON} --nacp=${PROJECT_NAME}.nacp --romfsdir=${CMAKE_BINARY_DIR}/resources
    )
endif ()

target_include_directories(${PROJECT_NAME} PRIVATE src ${CMAKE_BINARY_DIR}/generated ${NLOHMANN_INCLUDE} ${APP_PLATFORM_INCLUDE})
target_compile_options(${PROJECT_NAME} PRIVATE -ffunction-sections -fdata-sections ${APP_PLATFORM_OPTION})
if (NUVIO_DEBUG_NETWORK)
    target_compile_definitions(${PROJECT_NAME} PRIVATE NUVIO_DEBUG_NETWORK)
endif ()
target_link_libraries(${PROJECT_NAME} PRIVATE borealis ${APP_PLATFORM_LIB})

# unit tests: desktop only
if (PLATFORM_DESKTOP)
    enable_testing()
    add_subdirectory(tests)
endif ()
