# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

NuvioSwitch is a Nintendo Switch homebrew app that streams media. It is a port of NuvioTV, an Android TV app (fork: https://github.com/ryansese/NuvioTV, upstream: NuvioMedia/NuvioTV).

The planned stack is C++ with devkitPro/libnx, the borealis UI library and libmpv for playback. The repo has no code yet, so there are no build, lint or test commands. Add them here once the project is scaffolded.

## Reference source: NuvioTV

A local checkout of the fork is at `../nuvio-specs/repos/NuvioTV`, a submodule of the `nuvio-specs` OpenSpec workspace. Use it to look up behaviour. It is Kotlin/Jetpack Compose and does not run on the Switch, so port behaviour and data flow rather than code. Treat it as read-only from this repo. Its own guidance is in `../nuvio-specs/.claude/rules/NuvioTV.md`.

The parts of NuvioTV that matter most for the port, under `app/src/main/java/com/nuvio/tv/`:

- **Content comes from Stremio-compatible add-ons.** Catalogs, metadata and streams are fetched over HTTP from add-on manifests (`core/plugin`, `core/streams`, `data/remote`). The app aggregates them into a library and has no content of its own.
- **Layers:** `domain/` holds the models and repository interfaces, `data/` the API clients, repositories and DataStore preferences, `core/` the feature services (player, streams, sync, auth, profile, trakt, tmdb, debrid, torrent), and `ui/` the screens and navigation.
- **Startup flow** (`MainActivity.kt`): account QR sign-in → profile selection (PIN) → layout choice → main shell (Home, Search, Library, Add-ons, Settings).
- **Integrations:** Supabase-backed account sync, plus Trakt, Simkl, MDBList and TMDB. API keys are read from `local.properties` into `BuildConfig` (template in `local.example.properties`).
- **Playback** uses Media3, with libmpv as a second player backend (`libmpv-android/`).
- **Navigation** is built around a TV remote and focus handling, which maps naturally to a gamepad.

Specs for Nuvio ecosystem changes are written in `../nuvio-specs/openspec/`.
