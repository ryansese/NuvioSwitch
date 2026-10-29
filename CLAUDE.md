# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

NuvioSwitch is a Nintendo Switch homebrew app that streams media. It is a port of NuvioTV, an Android TV app (fork: https://github.com/ryansese/NuvioTV, upstream: NuvioMedia/NuvioTV).

The stack is C++17 with devkitPro/libnx, the borealis UI library and (from NuvioSwitch-0008) libmpv for playback. Work is planned as numbered OpenSpec changes in the workspace (`../../openspec/changes/nuvioswitch-NNNN-*`); commit subjects use the change title, e.g. `NuvioSwitch-0001 Scaffold CMake build and app shell`.

## Commands

The version is set once, in `CMakeLists.txt` (`VERSION_MAJOR/MINOR/ALTER`), and reaches the NACP and `version.hpp`.

```bash
# Desktop (macOS): build, run from the build dir (borealis loads ./resources/), test
cmake -B build_desktop -DPLATFORM_DESKTOP=ON -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build_desktop -j
(cd build_desktop && ./NuvioSwitch)          # -d debug logging, -v debug view
ctest --test-dir build_desktop --output-on-failure

# Switch, without a local devkitPro install (same image CI pins)
docker run --rm -v "$PWD":/src -w /src devkitpro/devkita64:20260219 bash -c \
  'cmake -B build_switch -DPLATFORM_SWITCH=ON -DCMAKE_BUILD_TYPE=Release && cmake --build build_switch --target NuvioSwitch.nro -j2'

# Send to a console running the homebrew menu's netloader
nxlink -s build_switch/NuvioSwitch.nro
```

- Keep Switch builds at `-j2` in Docker: higher parallelism runs the compiler out of memory on borealis sources.
- CMake 4 rejects projects declaring a minimum below 3.5, so fetched dependencies whose CMakeLists are old (doctest) are used header-only via `SOURCE_SUBDIR`.
- On desktop, Escape is B and Enter is A (borealis keyboard mapping). + (Start) quits because `main.cpp` enables borealis's global quit, which is off by default.
- CI is `.github/workflows/build.yml`: a Switch job in `devkitpro/devkita64:<dated tag>` that uploads the `.nro`, and a macOS job that builds and runs `ctest`.

## Layout

- `src/main.cpp`: init, theme colour overrides (NuvioTV dark tokens), XML view registration, root activity.
- `src/activity/`: activities (screens pushed on the stack). `src/tabs/`: tab content views; `PlaceholderTab` stands in for tabs whose feature hasn't landed.
- `src/core/`: pure logic with no borealis dependency. It is compiled into the test target too, so keep UI code out of it.
- `resources/`: `xml/` layouts, `i18n/en-US/app.json` (strings are read as `app/...`), `img/` (`icon.jpg` is the 256×256 `.nro` icon), `icon/` (NuvioTV sidebar SVGs), plus `font/`, `material/` and `i18n/en-US/hints.json` copied from borealis because it needs them at runtime.
- `tests/`: doctest cases; `test_main.cpp` provides `main`.

## borealis (`library/borealis`)

This is a git submodule of the maintained fork `xfangfang/borealis`, not `natinusala/borealis`. Run `git submodule update --init --recursive` after cloning. Don't edit files inside it. To move to a newer borealis version, check out a new commit in the submodule and commit the updated pointer in this repo.

The fork's top-level `CMakeLists.txt` builds its demo app. It is the template for this app's own CMake setup:

- It includes `library/cmake/commonOption.cmake` (build options) and `library/cmake/toolchain.cmake` (per-platform toolchain) before `project()`, then calls `add_subdirectory(library)` and links against `borealis`.
- The platform is chosen with a CMake flag: `PLATFORM_DESKTOP` for testing the UI on macOS, `PLATFORM_SWITCH` for the console.
- On Switch, a `<project>.nro` custom target packages the `.elf` with `nacptool` and `elf2nro`, and puts `resources/` into romfs. `USE_DEKO3D` additionally compiles shaders.

Platform build instructions are in the fork's wiki (https://github.com/xfangfang/borealis/wiki/NintendoSwitch), not in the repo. Switchfin and wiliwili are borealis + libmpv apps worth using as references for the player integration.

## Reference source: NuvioTV

This repo and NuvioTV are both submodules of the `nuvio-specs` OpenSpec workspace, so a local checkout of the fork sits next to this one at `../NuvioTV`. Use it to look up behaviour. It is Kotlin/Jetpack Compose and does not run on the Switch, so port behaviour and data flow rather than code. Treat it as read-only from this repo. Its own guidance is in the workspace at `../../.claude/rules/nuviotv.md`.

The parts of NuvioTV that matter most for the port, under `app/src/main/java/com/nuvio/tv/`:

- **Content comes from Stremio-compatible add-ons.** Catalogs, metadata and streams are fetched over HTTP from add-on manifests (`core/plugin`, `core/streams`, `data/remote`). The app aggregates them into a library and has no content of its own.
- **Layers:** `domain/` holds the models and repository interfaces, `data/` the API clients, repositories and DataStore preferences, `core/` the feature services (player, streams, sync, auth, profile, trakt, tmdb, debrid, torrent), and `ui/` the screens and navigation.
- **Startup flow** (`MainActivity.kt`): account QR sign-in → profile selection (PIN) → layout choice → main shell (Home, Search, Library, Add-ons, Settings).
- **Integrations:** Supabase-backed account sync, plus Trakt, Simkl, MDBList and TMDB. API keys are read from `local.properties` into `BuildConfig` (template in `local.example.properties`).
- **Playback** uses Media3, with libmpv as a second player backend (`libmpv-android/`).
- **Navigation** is built around a TV remote and focus handling, which maps naturally to a gamepad.

Specs for Nuvio ecosystem changes are written in the workspace at `../../openspec/`.
