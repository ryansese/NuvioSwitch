#include "ui/player/mpv_core.hpp"

#include <borealis.hpp>
#include <mpv/client.h>
#include <mpv/render.h>
#include <mpv/render_gl.h>

#include <GLFW/glfw3.h>
#include <algorithm>
#include <mutex>
#include <set>

#include "core/log.hpp"
#include "version.hpp"

namespace nuvio::ui::player
{

namespace
{
constexpr uint64_t OBS_POSITION = 1;
constexpr uint64_t OBS_DURATION = 2;
constexpr uint64_t OBS_PAUSE    = 3;
constexpr uint64_t OBS_CACHING  = 4;
constexpr uint64_t OBS_BUFFERED = 5;
constexpr uint64_t OBS_TRACKS   = 6;

// NuvioTV's MPV_MAX_VOLUME_PERCENT
constexpr const char* VOLUME_MAX = "400";
constexpr int64_t DEMUXER_BYTES  = 64 * 1024 * 1024;

// The cores that exist, so a wakeup posted to the UI thread can tell that its core is gone
// (touched on the UI thread only).
std::set<MpvCore*>& live()
{
    static std::set<MpvCore*> cores;
    return cores;
}

void* glProc(void*, const char* name)
{
    return reinterpret_cast<void*>(glfwGetProcAddress(name));
}
} // namespace

// What the C callbacks may call: they run on mpv's threads and only post to the UI thread or set a flag.
struct MpvCoreAccess
{
    static void wakeup(void* context)
    {
        auto* core = static_cast<MpvCore*>(context);
        brls::sync([core]()
            {
                if (live().count(core))
                    core->drain();
            });
    }
    static void update(void* context) { static_cast<MpvCore*>(context)->frameReady = true; }
};

MpvCore::MpvCore(Events events) : events(std::move(events))
{
    handle = mpv_create();
    if (!handle)
    {
        core::log::error("mpv_create failed");
        return;
    }
    applyOptions();
    if (mpv_initialize(handle) < 0)
    {
        core::log::error("mpv_initialize failed");
        mpv_terminate_destroy(handle);
        handle = nullptr;
        return;
    }

    live().insert(this);
    mpv_observe_property(handle, OBS_POSITION, "time-pos", MPV_FORMAT_DOUBLE);
    mpv_observe_property(handle, OBS_DURATION, "duration", MPV_FORMAT_DOUBLE);
    mpv_observe_property(handle, OBS_PAUSE, "pause", MPV_FORMAT_FLAG);
    mpv_observe_property(handle, OBS_CACHING, "paused-for-cache", MPV_FORMAT_FLAG);
    mpv_observe_property(handle, OBS_BUFFERED, "demuxer-cache-time", MPV_FORMAT_DOUBLE);
    mpv_observe_property(handle, OBS_TRACKS, "track-list", MPV_FORMAT_STRING);
    mpv_request_log_messages(handle, "warn");
    mpv_set_wakeup_callback(handle, MpvCoreAccess::wakeup, this);
    core::log::info(std::string("mpv ") + getString("mpv-version") + ", ffmpeg " + getString("ffmpeg-version"));
}

MpvCore::~MpvCore()
{
    shutdown();
}

bool MpvCore::setOption(const std::string& name, const std::string& value)
{
    int result = mpv_set_option_string(handle, name.c_str(), value.c_str());
    if (result < 0)
        core::log::warning("mpv option " + name + "=" + value + " failed: " + mpv_error_string(result));
    return result >= 0;
}

// NuvioTV's initOptions where they apply outside Android (see the change's design).
void MpvCore::applyOptions()
{
    setOption("vo", "libmpv");
    setOption("user-agent", std::string("NuvioSwitch/") + NUVIO_APP_VERSION);
    setOption("keep-open", "yes");
    setOption("demuxer-max-bytes", std::to_string(DEMUXER_BYTES));
    setOption("demuxer-max-back-bytes", std::to_string(DEMUXER_BYTES));
    setOption("sub-ass-override", "no");
    setOption("sub-codepage", "auto:utf-8");
    setOption("sub-use-margins", "yes");
    setOption("sub-ass-force-margins", "yes");
    setOption("save-position-on-quit", "no");
    setOption("volume-max", VOLUME_MAX);
    setOption("input-default-bindings", "no");
    setOption("terminal", "no");
    setOption("hwdec", "auto-safe");
#ifdef __SWITCH__
    // software decoding on the console unless the build has a hardware decoder; fast skips the costly filters
    setOption("profile", "fast");
    setOption("ao", "hos");
#endif
    // media is verified like any other non-add-on request (nuvioswitch-networking). The console has no system CA
    // list for ffmpeg, so a bundled one is used when the app ships it.
#ifdef __SWITCH__
    if (FILE* bundle = std::fopen("romfs:/cacert.pem", "rb"))
    {
        std::fclose(bundle);
        setOption("tls-verify", "yes");
        setOption("tls-ca-file", "romfs:/cacert.pem");
    }
    else
    {
        core::log::warning("no CA bundle in romfs: media TLS is not verified");
        setOption("tls-verify", "no");
    }
#else
    setOption("tls-verify", "yes");
#endif
}

bool MpvCore::createRenderContext()
{
    if (!handle || render_)
        return render_ != nullptr;

    mpv_opengl_init_params gl{glProc, nullptr};
    int advanced = 1;
    mpv_render_param params[] = {
        {MPV_RENDER_PARAM_API_TYPE, const_cast<char*>(MPV_RENDER_API_TYPE_OPENGL)},
        {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &gl},
        {MPV_RENDER_PARAM_ADVANCED_CONTROL, &advanced},
        {MPV_RENDER_PARAM_INVALID, nullptr},
    };
    int result = mpv_render_context_create(&render_, handle, params);
    if (result < 0)
    {
        core::log::error(std::string("mpv_render_context_create failed: ") + mpv_error_string(result));
        render_ = nullptr;
        return false;
    }
    mpv_render_context_set_update_callback(render_, MpvCoreAccess::update, this);
    return true;
}

void MpvCore::command(const std::vector<std::string>& args)
{
    if (!handle)
        return;
    std::vector<const char*> argv;
    for (const auto& a : args)
        argv.push_back(a.c_str());
    argv.push_back(nullptr);
    int result = mpv_command_async(handle, 0, argv.data());
    if (result < 0)
        core::log::warning("mpv command " + args.front() + " failed: " + mpv_error_string(result));
}

void MpvCore::setProperty(const std::string& name, const std::string& value)
{
    if (handle)
        mpv_set_property_string(handle, name.c_str(), value.c_str());
}

std::string MpvCore::getString(const std::string& name)
{
    if (!handle)
        return "";
    char* value = mpv_get_property_string(handle, name.c_str());
    if (!value)
        return "";
    std::string out = value;
    mpv_free(value);
    return out;
}

void MpvCore::load(const std::string& url, const std::map<std::string, std::string>& headers,
    std::optional<int64_t> startMs)
{
    if (!handle)
        return;
    started = false;
    lastLogError.clear();
    addedSubtitles.clear();
    setProperty("http-header-fields", data::player::headerFields(headers));
    // each file starts on automatic tracks
    setProperty("aid", "auto");
    setProperty("sid", "auto");
    command(data::player::loadfileArgs(url, startMs));
}

void MpvCore::stop()
{
    command({"stop"});
}

void MpvCore::setPaused(bool paused)
{
    setProperty("pause", paused ? "yes" : "no");
}

void MpvCore::togglePause()
{
    command({"cycle", "pause"});
}

void MpvCore::seekRelative(double seconds)
{
    command({"seek", std::to_string(seconds), "relative"});
}

void MpvCore::seekAbsolute(double seconds)
{
    command({"seek", std::to_string(seconds), "absolute"});
}

void MpvCore::setAudioTrack(int64_t id)
{
    setProperty("aid", std::to_string(id));
}

void MpvCore::setSubtitleTrack(std::optional<int64_t> id)
{
    setProperty("sid", id ? std::to_string(*id) : "no");
}

void MpvCore::addSubtitle(const std::string& url, const std::string& title, const std::string& lang)
{
    // "cached" makes mpv reuse a subtitle it already loaded; the list keeps us from relying on that alone
    if (std::find(addedSubtitles.begin(), addedSubtitles.end(), url) == addedSubtitles.end())
        addedSubtitles.push_back(url);
    command({"sub-add", url, "cached", title, lang});
    setProperty("sub-visibility", "yes");
}

bool MpvCore::render(unsigned fbo, int width, int height, bool force)
{
    if (!render_ || width <= 0 || height <= 0)
        return false;
    // the update flags must be read to let mpv know we are drawing
    frameReady = false;
    uint64_t flags = mpv_render_context_update(render_);
    if (!(flags & MPV_RENDER_UPDATE_FRAME) && !force)
        return false;

    mpv_opengl_fbo target{static_cast<int>(fbo), width, height, 0};
    // NanoVG samples the texture top row first, so mpv draws without its own flip
    int flipY = 0;
    mpv_render_param params[] = {
        {MPV_RENDER_PARAM_OPENGL_FBO, &target},
        {MPV_RENDER_PARAM_FLIP_Y, &flipY},
        {MPV_RENDER_PARAM_INVALID, nullptr},
    };
    mpv_render_context_render(render_, params);
    mpv_render_context_report_swap(render_);
    return true;
}

void MpvCore::drain()
{
    while (handle)
    {
        mpv_event* event = mpv_wait_event(handle, 0);
        if (event->event_id == MPV_EVENT_NONE)
            return;
        if (event->event_id == MPV_EVENT_SHUTDOWN)
            return;
        handleEvent(event);
    }
}

void MpvCore::handleEvent(void* raw)
{
    auto* event = static_cast<mpv_event*>(raw);
    switch (event->event_id)
    {
        case MPV_EVENT_PROPERTY_CHANGE:
        {
            auto* property = static_cast<mpv_event_property*>(event->data);
            if (property->format == MPV_FORMAT_NONE)
                return; // unavailable, e.g. time-pos between files
            switch (event->reply_userdata)
            {
                case OBS_POSITION:
                    if (events.onPosition && property->format == MPV_FORMAT_DOUBLE)
                        events.onPosition(*static_cast<double*>(property->data));
                    break;
                case OBS_DURATION:
                    if (events.onDuration && property->format == MPV_FORMAT_DOUBLE)
                        events.onDuration(*static_cast<double*>(property->data));
                    break;
                case OBS_BUFFERED:
                    if (events.onBuffered && property->format == MPV_FORMAT_DOUBLE)
                        events.onBuffered(*static_cast<double*>(property->data));
                    break;
                case OBS_PAUSE:
                    if (events.onPaused && property->format == MPV_FORMAT_FLAG)
                        events.onPaused(*static_cast<int*>(property->data) != 0);
                    break;
                case OBS_CACHING:
                    if (events.onWaitingForCache && property->format == MPV_FORMAT_FLAG)
                        events.onWaitingForCache(*static_cast<int*>(property->data) != 0);
                    break;
                case OBS_TRACKS:
                    if (events.onTracks && property->format == MPV_FORMAT_STRING)
                        if (auto tracks = data::player::parseTrackList(*static_cast<char**>(property->data)))
                            events.onTracks(std::move(*tracks));
                    break;
            }
            break;
        }
        case MPV_EVENT_PLAYBACK_RESTART:
            // also fires after seeks; only the first one ends the loading state
            if (!started)
            {
                started = true;
                if (events.onPlaybackStarted)
                    events.onPlaybackStarted();
            }
            break;
        case MPV_EVENT_LOG_MESSAGE:
        {
            auto* message = static_cast<mpv_event_log_message*>(event->data);
            std::string text = message->text ? message->text : "";
            while (!text.empty() && (text.back() == '\n' || text.back() == '\r'))
                text.pop_back();
            if (!text.empty())
            {
                // warnings and errors from mpv and ffmpeg go to the app log; the last error is the failure reason
                core::log::info(std::string("mpv [") + (message->prefix ? message->prefix : "") + "] " + text);
                if (message->log_level <= MPV_LOG_LEVEL_ERROR)
                    lastLogError = text;
            }
            break;
        }
        case MPV_EVENT_END_FILE:
        {
            auto* end = static_cast<mpv_event_end_file*>(event->data);
            if (end->reason == MPV_END_FILE_REASON_ERROR)
            {
                std::string reason = mpv_error_string(end->error);
                if (!lastLogError.empty())
                    reason = lastLogError;
                core::log::info("mpv: playback failed: " + reason);
                if (events.onError)
                    events.onError(reason);
            }
            else if (end->reason == MPV_END_FILE_REASON_EOF)
            {
                if (events.onEnded)
                    events.onEnded();
            }
            break;
        }
        default: break;
    }
}

void MpvCore::shutdown()
{
    if (!handle)
        return;
    // no more callbacks into this object
    live().erase(this);
    mpv_set_wakeup_callback(handle, nullptr, nullptr);
    const char* stopCommand[] = {"stop", nullptr};
    mpv_command(handle, stopCommand);
    if (render_)
    {
        mpv_render_context_set_update_callback(render_, nullptr, nullptr);
        mpv_render_context_free(render_);
        render_ = nullptr;
    }
    mpv_terminate_destroy(handle);
    handle = nullptr;
}

} // namespace nuvio::ui::player
