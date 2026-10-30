#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "data/player/player_rules.hpp"

struct mpv_handle;
struct mpv_render_context;

namespace nuvio::ui::player
{

// A thin wrapper around one libmpv instance (NuvioTV NuvioMpvSurfaceView.kt): options, the OpenGL
// render context, and mpv's events turned into callbacks. Everything runs on the UI thread, where
// borealis's GL context lives: mpv's wakeup is posted to it with brls::sync, and callbacks fire
// from there.
//
// Teardown order matters (the usual crash of borealis + mpv apps): stop, free the render context,
// then destroy the handle. shutdown() does this and is called by the destructor.
class MpvCore
{
  public:
    struct Events
    {
        std::function<void(double seconds)> onPosition;
        std::function<void(double seconds)> onDuration;
        std::function<void(double seconds)> onBuffered; // how far ahead the demuxer has data
        std::function<void(bool paused)> onPaused;
        std::function<void(bool waiting)> onWaitingForCache;
        std::function<void(std::vector<data::player::Track>)> onTracks;
        std::function<void()> onPlaybackStarted; // the first frame (or audio) after a load
        std::function<void()> onEnded;           // played to the end
        std::function<void(std::string reason)> onError; // the file failed to load or playback broke
    };

    explicit MpvCore(Events events);
    ~MpvCore();

    MpvCore(const MpvCore&)            = delete;
    MpvCore& operator=(const MpvCore&) = delete;

    // False when the handle couldn't be created or initialised.
    bool ok() const { return handle != nullptr; }

    // Creates the render context. The borealis GL context must be current.
    bool createRenderContext();

    // Plays url with headers (http-header-fields is always set, empty clears the last stream's),
    // starting at startMs when given. Tracks are reset to automatic.
    void load(const std::string& url, const std::map<std::string, std::string>& headers,
        std::optional<int64_t> startMs);

    void stop();
    void setPaused(bool paused);
    void togglePause();
    void seekRelative(double seconds);
    void seekAbsolute(double seconds);

    void setAudioTrack(int64_t id);
    void setSubtitleTrack(std::optional<int64_t> id); // nullopt is off
    // sub-add <url> cached <title> <lang>, then selects it and makes subtitles visible.
    void addSubtitle(const std::string& url, const std::string& title, const std::string& lang);

    // Draws the next frame into fbo when there is one, or when force is set; returns whether it did.
    // The GL state is left as mpv wants it: the caller saves and restores what it cares about.
    bool render(unsigned fbo, int width, int height, bool force);

    std::string getString(const std::string& name);
    bool setOption(const std::string& name, const std::string& value);

    // Stops, frees the render context and destroys the handle. Safe to call twice.
    void shutdown();

  private:
    friend struct MpvCoreAccess;

    void drain();
    void handleEvent(void* event);
    void command(const std::vector<std::string>& args);
    void setProperty(const std::string& name, const std::string& value);
    void applyOptions();

    Events events;
    mpv_handle* handle         = nullptr;
    mpv_render_context* render_ = nullptr;
    std::atomic<bool> frameReady{false};
    bool started = false;
    std::string lastLogError;
    std::vector<std::string> addedSubtitles; // sub-add URLs, so each is added once
};

} // namespace nuvio::ui::player
