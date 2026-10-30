#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/net/http_client.hpp"
#include "domain/addon.hpp"
#include "domain/stream.hpp"

namespace nuvio::data::streams
{

namespace net = core::net;

// Asks every add-on that serves a video for its streams, all at once, and reports each
// add-on's group as soon as it answers (NuvioTV StreamRepositoryImpl.fetchStreamsFromAllSources).
// An add-on whose response has no streams is asked for its meta instead, and the streams
// listed inline for the video are used (fetchInlineStreamsFromMeta). Streams are
// de-duplicated within a group.
//
// Everything here runs on the UI thread: requests go through HttpClient and its callbacks
// arrive there. Responses are parsed on the HTTP workers.
class StreamRepository
{
  public:
    struct Group
    {
        size_t order = 0; // the add-on's place among those asked: installed order, whatever arrived first
        std::string addonName;
        std::string addonBaseUrl;
        std::vector<domain::Stream> streams; // never empty
    };

    struct Summary
    {
        std::vector<std::string> queried; // display names of every add-on asked, in installed order
        std::vector<std::string> failed;  // those that failed or had no streams, in the order they finished
        size_t groups = 0;                // add-ons that returned streams
    };

    using GroupCallback = std::function<void(const Group&)>;
    using DoneCallback  = std::function<void(const Summary&)>;
    using Ticket        = uint64_t;

    struct Config
    {
        net::HttpClient* http = nullptr;
    };

    explicit StreamRepository(Config config) : config(std::move(config)) { }

    // Starts the queries for type/videoId over the installed add-ons. onGroup runs once per add-on
    // with streams; onDone runs after every add-on has finished, unless cancel(ticket) comes first.
    // With no qualifying add-on, onDone runs before this returns.
    Ticket fetchAll(const std::string& type, const std::string& videoId, const std::vector<domain::Addon>& addons,
        GroupCallback onGroup, DoneCallback onDone);

    // Stops the transfers and drops the callbacks.
    void cancel(Ticket ticket);

    // The app-wide instance, set up in main.cpp.
    static StreamRepository& shared();
    static void setShared(StreamRepository* repository);
    // False before setup and after teardown; views check it before cancelling in their destructors.
    static bool hasShared();

  private:
    struct Run
    {
        Ticket ticket = 0;
        bool cancelled = false;
        size_t pending = 0;
        Summary summary;
        GroupCallback onGroup;
        DoneCallback onDone;
        std::vector<net::RequestHandle> handles;
    };

    void ask(const std::shared_ptr<Run>& run, size_t order, const domain::Addon& addon, const std::string& type,
        const std::string& videoId);
    void askMeta(const std::shared_ptr<Run>& run, size_t order, const domain::Addon& addon, const std::string& type,
        const std::string& videoId);
    void deliver(const std::shared_ptr<Run>& run, Group group);
    void fail(const std::shared_ptr<Run>& run, const domain::Addon& addon, const std::string& reason);
    void finishOne(const std::shared_ptr<Run>& run);

    Config config;
    std::unordered_map<Ticket, std::shared_ptr<Run>> runs;
    Ticket nextTicket = 1;
};

} // namespace nuvio::data::streams
