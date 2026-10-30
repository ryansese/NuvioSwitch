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
- Desktop needs Homebrew `webp`, `pkgconf` and `mpv` (`brew install webp pkgconf mpv`); curl is the macOS system one. The Switch image already has `switch-curl`, `switch-libwebp` and `switch-libmpv` (0.39, with ffmpeg 7.1; linked statically through its pkg-config file, which makes the `.nro` about 32 MB). Whether that stock build can decode 1080p smoothly on hardware is still to be benchmarked (NuvioSwitch-0008 task 1.1).
- `-DNUVIO_DEBUG_NETWORK=ON` logs an HTTPS check at startup. `NUVIOSWITCH_PLAY=<url or file>` opens the player on it at startup (desktop testing; a local file works).
- `resources/local_addons.txt` (git-ignored; one add-on URL per line) lists add-ons installed at the end of the list on every start when missing, for development add-ons whose URLs hold secrets (a debrid token). It is bundled into the `.nro`'s romfs, so don't share a `.nro` built with one. Removing such an add-on in the app doesn't stick: it comes back on the next start.
- borealis logs to stdout, which is fully buffered when redirected and lost if the app is killed; capture runs with `stdbuf -oL ./NuvioSwitch -d > run.log`.
- CI is `.github/workflows/build.yml`: a Switch job in `devkitpro/devkita64:<dated tag>` that uploads the `.nro`, and a macOS job that builds and runs `ctest`.

## Layout

- `src/main.cpp`: init, theme colour overrides (NuvioTV dark tokens), XML view registration, root activity; it creates the shared `HttpClient`, `ImageLoader`, `AddonRepository`, `CatalogRepository` and `MetaRepository` and tears them down after the main loop.
- `src/activity/`: activities (screens pushed on the stack). `src/tabs/`: tab content views; `PlaceholderTab` stands in for tabs whose feature hasn't landed.
- `src/domain/`: plain models (`Addon`, `MetaPreview`, `Meta`, `CatalogRow`, `StreamRequest`, `Stream`, `PlaybackRequest`) and catalog paging, ported from NuvioTV `domain/model`.
- `src/data/`: repositories and the pure logic behind them, one folder per feature. `addons/`: URL rules, manifest parser, display names, capability queries (`addonsFor`) and `AddonRepository`, which keeps the installed list in the `addons` store and raw manifests in `addon_manifest_cache`, and notifies subscribers on the UI thread. `catalog/`: catalog URLs, response parsing, which catalogs Home shows and the hero pick (`home_selection`), and `CatalogRepository`. `meta/`: meta parsing, which add-ons are asked for a title (`candidates`), the `Cache-Control` TTL with an LRU, and `MetaRepository` (tries candidates in order, then the source add-on; shares in-flight lookups). `player/`: the pure player rules (header fields, `loadfile` arguments, seek steps, subtitle URLs, `track-list` parsing, time formatting) and `SubtitleRepository`. `search/`: which catalogs a search asks (`searchTargets`), the search query URL extras, and the recent-searches list (`SearchHistory`). `streams/`: stream parsing, the picker's rules (add-on selection, URL, de-duplication, playability, headers without `Range`, the `PlaybackRequest` hand-off) and `StreamRepository` (asks every stream add-on in parallel, inline-meta fallback, one callback per add-on group).
- `src/ui/`: feature screens built on borealis, one folder per feature: `home/` (Home tab, hero, catalog rows), `catalog/` (See all), `details/` (Details screen and episode cell), `search/` (Search tab; its rows are `CatalogRowView` with first-page-only options), `streams/` (stream picker and `StreamCard`), `player/` (`PlayerActivity`; `MpvCore` wraps one libmpv handle and its OpenGL render context, `PlayerView` draws the mpv frame through an off-screen texture and NanoVG, `SeekBar`), `addons/` (Add-ons tab), `common/` (`PosterCell`, `Scrim`). `navigation.hpp` opens screens other features reach (details, See all).
- borealis rebuilds a tab's view each time the tab is selected, so state that should survive tab switches lives outside the view (Home keeps loaded catalog pages in a session cache).
- `src/core/`: services and logic. It is compiled into the test target with `src/domain/` and `src/data/`. Files in those three that need borealis or curl are named `*_brls.cpp` or `curl_*.cpp`; `tests/CMakeLists.txt` leaves them out, so keep every other file free of both (inject them instead, as `HttpClient` does with its transport and UI poster).
  - `async/`: `WorkerPool` and `postToUi` (`brls::sync`). `json/`: tolerant getters for loose add-on JSON.
  - `net/`: `HttpClient` (worker pool, `Cache-Control` disk cache, results on the UI thread), `CurlExecutor`, NuvioTV's default headers.
  - `storage/`: `Paths` (data folder: `sdmc:/switch/NuvioSwitch/`, `~/Library/Application Support/NuvioSwitch/`, or `NUVIOSWITCH_DATA_DIR`) and `Store`, the per-profile JSON key-value files with atomic writes.
  - `image/`: `ImageLoader` for borealis `Image` views, memory and disk caches, WebP decoding.
- `resources/`: `xml/` layouts, `i18n/en-US/app.json` (strings are read as `app/...`), `img/` (`icon.jpg` is the 256×256 `.nro` icon), `icon/` (NuvioTV sidebar SVGs), plus `font/`, `material/` and `i18n/en-US/hints.json` copied from borealis because it needs them at runtime.
- `tests/`: doctest cases; `test_main.cpp` provides `main`. `fixtures/` holds real add-on manifests, read with `readFixture()` from `test_util.hpp`.

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
