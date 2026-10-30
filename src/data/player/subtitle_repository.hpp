#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/net/http_client.hpp"
#include "domain/addon.hpp"
#include "domain/playback_request.hpp"
#include "domain/subtitle.hpp"

namespace nuvio::data::player
{

namespace net = core::net;

// The `{"subtitles": [...]}` response of a subtitles add-on: entries without a URL are
// skipped, a missing language is "und". nullopt when the body isn't a JSON object.
std::optional<std::vector<domain::Subtitle>> parseSubtitleResponse(const std::string& body,
    const std::string& addonName);

// The enabled add-ons with a "subtitles" (or "subtitle") resource for the type ("tv" counts as
// "series") and ID, in installed order (NuvioTV SubtitleRepositoryImpl.supportsType).
std::vector<domain::Addon> subtitleAddons(const std::vector<domain::Addon>& addons, const std::string& type,
    const std::string& id);

// Collects the subtitles for what is playing: the stream's own, then those of every subtitles
// add-on, asked in parallel with a 20 second limit each (SubtitleRepositoryImpl.getSubtitles).
//
// Runs on the UI thread, like the other repositories; responses are parsed on the HTTP workers.
class SubtitleRepository
{
  public:
    static constexpr long TIMEOUT_SECONDS = 20;

    using UpdateCallback = std::function<void(const std::vector<domain::Subtitle>& all)>;
    using DoneCallback   = std::function<void()>;
    using Ticket         = uint64_t;

    struct Config
    {
        net::HttpClient* http = nullptr;
    };

    explicit SubtitleRepository(Config config) : config(std::move(config)) { }

    // onUpdate runs with everything known so far: at once with the stream's own subtitles (when it has
    // any) and again whenever an add-on answers with some. onDone runs when every add-on has answered,
    // failed or timed out. Neither runs after cancel(ticket).
    Ticket fetchAll(const domain::PlaybackRequest& request, const std::vector<domain::Addon>& addons,
        UpdateCallback onUpdate, DoneCallback onDone);

    void cancel(Ticket ticket);

    // The app-wide instance, set up in main.cpp.
    static SubtitleRepository& shared();
    static void setShared(SubtitleRepository* repository);
    static bool hasShared();

  private:
    struct Run
    {
        bool cancelled = false;
        size_t pending = 0;
        std::vector<domain::Subtitle> all;
        UpdateCallback onUpdate;
        DoneCallback onDone;
        std::vector<net::RequestHandle> handles;
    };

    Config config;
    std::unordered_map<Ticket, std::shared_ptr<Run>> runs;
    Ticket nextTicket = 1;
};

} // namespace nuvio::data::player
