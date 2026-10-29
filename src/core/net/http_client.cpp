#include "core/net/http_client.hpp"

#include <algorithm>
#include <cassert>

#include "core/net/headers.hpp"

namespace nuvio::core::net
{

namespace
{
HttpClient* sharedClient = nullptr;
}

HttpClient& HttpClient::shared()
{
    assert(sharedClient && "HttpClient::setShared was not called");
    return *sharedClient;
}

void HttpClient::setShared(HttpClient* client)
{
    sharedClient = client;
}

HttpClient::HttpClient(Config cfg) : config(std::move(cfg)), pool(config.threads)
{
}

HttpClient::~HttpClient()
{
    shutdown();
}

void HttpClient::shutdown()
{
    stopped.store(true);
    {
        std::lock_guard<std::mutex> lock(liveMutex);
        for (auto& weak : live)
            if (auto flag = weak.lock())
                flag->store(true);
        live.clear();
    }
    pool.shutdown();
    if (config.cache)
        config.cache->flush();
}

RequestHandle HttpClient::send(Request request, Callback callback)
{
    auto cancelled = std::make_shared<std::atomic<bool>>(stopped.load());
    {
        std::lock_guard<std::mutex> lock(liveMutex);
        live.erase(std::remove_if(live.begin(), live.end(), [](const auto& weak) { return weak.expired(); }), live.end());
        live.push_back(cancelled);
    }

    pool.post([this, request = std::move(request), callback = std::move(callback), cancelled]
        {
            if (cancelled->load())
                return;
            HttpResult result = execute(request, *cancelled);
            if (cancelled->load())
                return;
            config.post([callback, cancelled, result = std::move(result)]() mutable
                {
                    // cancelled between the worker finishing and the UI thread running this
                    if (!cancelled->load())
                        callback(std::move(result));
                });
        });
    return RequestHandle(cancelled);
}

HttpResult HttpClient::execute(const Request& original, const std::atomic<bool>& cancelled)
{
    Request request = original;
    request.headers = mergeHeaders(config.defaultHeaders, original.headers);

    bool useCache = config.cache && request.method == "GET";
    std::optional<HttpCache::Lookup> cached;
    if (useCache && request.cachePolicy != CachePolicy::NoCache)
    {
        cached = config.cache->lookup(request.method, request.url);
        if (cached && cached->fresh)
            return cached->result;
    }

    HttpResult result = config.transport(request, cancelled);
    if (cancelled.load())
    {
        result.ok    = false;
        result.error = HttpError::Cancelled;
        return result;
    }

    if (useCache && result.ok)
        config.cache->store(request.method, request.url, result);
    else if (!result.ok && request.cachePolicy == CachePolicy::StaleIfError && cached &&
             isNetworkFailure(result.error))
        return cached->result; // flagged stale by lookup

    return result;
}

} // namespace nuvio::core::net
