#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

#include "core/async/worker_pool.hpp"
#include "core/net/http_cache.hpp"
#include "core/net/http_types.hpp"

namespace nuvio::core::net
{

// The one HTTP API for the app: requests run on a worker pool, go through the
// disk cache, and deliver their result once on the UI thread.
// The transport (curl) and the UI poster (brls::sync) are injected, so this
// class has no curl or borealis dependency and is unit-tested with fakes.
class HttpClient
{
  public:
    // Performs one request synchronously. Must return promptly once cancelled is set.
    using Transport = std::function<HttpResult(const Request&, const std::atomic<bool>& cancelled)>;
    // Runs a function on the UI thread.
    using Poster   = std::function<void(std::function<void()>)>;
    using Callback = std::function<void(HttpResult)>;
    // Runs on the worker with the result; the function it returns runs on the UI thread.
    using Prepare = std::function<std::function<void()>(HttpResult)>;

    struct Config
    {
        size_t threads = 4;
        Headers defaultHeaders;
        std::shared_ptr<HttpCache> cache; // optional
        Transport transport;
        Poster post;
    };

    explicit HttpClient(Config config);
    ~HttpClient(); // shutdown()

    // Queues request; callback runs on the UI thread exactly once, unless the
    // returned handle is cancelled first, in which case it never runs.
    RequestHandle send(Request request, Callback callback);

    // Like send, but prepare runs on the worker first, so large bodies can be
    // parsed off the UI thread. The function it returns follows send's rules.
    RequestHandle sendPrepared(Request request, Prepare prepare);

    // Cache lookup, transport and cache store, on the calling thread.
    HttpResult execute(const Request& request, const std::atomic<bool>& cancelled);

    // Cancels every pending request and waits for the workers; no callbacks run after this.
    void shutdown();

    const Headers& defaultHeaders() const { return config.defaultHeaders; }

    // The app-wide instance, set up in main.cpp.
    static HttpClient& shared();
    static void setShared(HttpClient* client);

  private:
    Config config;
    std::mutex liveMutex;
    std::vector<std::weak_ptr<std::atomic<bool>>> live; // cancel flags of requests not yet finished
    std::atomic<bool> stopped{false};
    async::WorkerPool pool; // last, so its threads are joined before the members above go away
};

} // namespace nuvio::core::net
