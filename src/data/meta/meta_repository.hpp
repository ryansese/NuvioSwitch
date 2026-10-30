#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/net/http_client.hpp"
#include "data/meta/meta_cache.hpp"
#include "data/meta/meta_candidates.hpp"
#include "domain/addon.hpp"
#include "domain/meta.hpp"

namespace nuvio::data::meta
{

namespace net = core::net;

// Loads a title's metadata the way NuvioTV does by default
// (MetaRepositoryImpl.getMetaFromAllAddons): the meta add-on candidates one at a
// time in priority order, then the add-on the title came from. Results are cached
// per (type, id) and lookups for the same title share one fetch.
//
// Everything here runs on the UI thread: requests go through HttpClient and its
// callbacks arrive there, so the cache and the in-flight table need no lock.
// Responses are parsed on the HTTP workers.
class MetaRepository
{
  public:
    struct Result
    {
        bool ok = false;
        domain::Meta meta;
        std::string error; // when !ok
    };
    using Callback = std::function<void(Result)>;
    using Ticket   = uint64_t;

    struct Config
    {
        net::HttpClient* http = nullptr;
        std::function<Clock::time_point()> now = [] { return Clock::now(); };
    };

    explicit MetaRepository(Config config) : config(std::move(config)) { }

    // Finds the meta of type/id. sourceAddonBaseUrl is the add-on whose catalog listed the
    // title ("" when unknown); addons is the installed list. A cached result calls back
    // before lookup returns; otherwise the callback runs once on the UI thread, unless
    // cancel(ticket) comes first.
    Ticket lookup(const std::string& type, const std::string& id, const std::string& sourceAddonBaseUrl,
        const std::vector<domain::Addon>& addons, Callback callback);

    // Drops the callback; the fetch itself carries on and fills the cache.
    void cancel(Ticket ticket);

    // Forgets every cached meta.
    void clear() { cache = MetaCache(); }

    static std::string cacheKey(const std::string& type, const std::string& id);

    // The app-wide instance, set up in main.cpp.
    static MetaRepository& shared();
    static void setShared(MetaRepository* repository);
    // False before setup and after teardown; views check it before cancelling in their destructors.
    static bool hasShared();

  private:
    struct Attempt
    {
        std::string url;
        std::string type;
        std::string addonBaseUrl;
    };
    struct Waiter
    {
        Ticket ticket;
        Callback callback;
    };
    struct Fetch
    {
        std::vector<Attempt> attempts;
        size_t next = 0;
        std::vector<Waiter> waiters;
    };

    void tryNext(const std::string& key);
    void finish(const std::string& key, Result result);

    Config config;
    MetaCache cache;
    std::unordered_map<std::string, Fetch> inFlight;
    Ticket nextTicket = 1;
};

} // namespace nuvio::data::meta
