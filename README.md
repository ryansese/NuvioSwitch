# NuvioSwitch

A Nintendo Switch homebrew port of [NuvioTV](https://github.com/NuvioMedia/NuvioTV), the Android TV media app built around Stremio-compatible add-ons. It is written in C++ with devkitPro/libnx, the [borealis](https://github.com/xfangfang/borealis) UI library and libmpv.

## Status

Early development. This version is the app shell only: the main window with Home, Search, Library, Add-ons and Settings tabs, each a placeholder for now. Browsing, playback and accounts arrive in later versions.

## Building

Clone with submodules:

```bash
git clone --recurse-submodules https://github.com/ryansese/NuvioSwitch.git
```

**Switch** (needs devkitPro, or Docker):

```bash
docker run --rm -v "$PWD":/src -w /src devkitpro/devkita64:20260219 bash -c \
  'cmake -B build_switch -DPLATFORM_SWITCH=ON -DCMAKE_BUILD_TYPE=Release && cmake --build build_switch --target NuvioSwitch.nro -j2'
```

Copy `build_switch/NuvioSwitch.nro` to `sdmc:/switch/` and open it from the homebrew menu, or send it with `nxlink -s build_switch/NuvioSwitch.nro`.

**Desktop (macOS)**, for testing the UI:

```bash
cmake -B build_desktop -DPLATFORM_DESKTOP=ON -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build_desktop -j
cd build_desktop && ./NuvioSwitch
```

Run the unit tests with `ctest --test-dir build_desktop --output-on-failure`.

## Controls

| Switch | Desktop keyboard | Action |
|---|---|---|
| D-pad / left stick | Arrow keys | Move focus |
| A | Enter | Select |
| B | Escape | Back |
| + | (gamepad Start, or close the window) | Exit |
