#include <doctest.h>

#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>
#include <vector>

#include "core/async/worker_pool.hpp"
#include "core/net/http_client.hpp"
#include "test_util.hpp"

using namespace nuvio::core;
using namespace std::chrono_literals;

namespace
{
// Collects posted callbacks so the test can run them, standing in for the UI thread.
struct FakeUi
{
    std::mutex mutex;
    std::vector<std::function<void()>> queue;

    net::HttpClient::Poster poster()
    {
        return [this](std::function<void()> fn)
        {
            std::lock_guard<std::mutex> lock(mutex);
            queue.push_back(std::move(fn));
        };
    }

    void drain()
    {
        std::vector<std::function<void()>> pending;
        {
            std::lock_guard<std::mutex> lock(mutex);
            pending.swap(queue);
        }
        for (auto& fn : pending)
            fn();
    }
};

net::HttpResult okResult(const std::string& body, const std::string& cacheControl = "")
{
    net::HttpResult result;
    result.ok     = true;
    result.status = 200;
    result.body   = body;
    if (!cacheControl.empty())
        result.headers["Cache-Control"] = cacheControl;
    return result;
}
} // namespace

TEST_CASE("WorkerPool runs every job and drains on shutdown")
{
    std::atomic<int> done{0};
    {
        async::WorkerPool pool(3);
        for (int i = 0; i < 50; i++)
            pool.post([&done] { done++; });
        pool.waitIdle();
        CHECK(done == 50);
        for (int i = 0; i < 10; i++)
            pool.post([&done] { done++; });
    } // destructor: shutdown runs what's queued
    CHECK(done == 60);
}

TEST_CASE("HttpClient runs at most pool-size requests at once, one callback each")
{
    FakeUi ui;
    std::atomic<int> running{0};
    std::atomic<int> peak{0};

    net::HttpClient::Config config;
    config.threads   = 4;
    config.post      = ui.poster();
    config.transport = [&](const net::Request& request, const std::atomic<bool>&)
    {
        int now = ++running;
        int old = peak.load();
        while (now > old && !peak.compare_exchange_weak(old, now))
            ;
        std::this_thread::sleep_for(20ms);
        running--;
        return okResult(request.url);
    };
    net::HttpClient client(config);

    std::vector<int> calls(10, 0);
    for (int i = 0; i < 10; i++)
    {
        net::Request request;
        request.url = "https://example/" + std::to_string(i);
        client.send(request, [&calls, i](net::HttpResult result)
            {
                CHECK(result.body == "https://example/" + std::to_string(i));
                calls[i]++;
            });
    }

    int delivered = 0;
    for (int tries = 0; tries < 400 && delivered < 10; tries++)
    {
        std::this_thread::sleep_for(5ms);
        ui.drain();
        delivered = 0;
        for (int count : calls)
            delivered += count;
    }

    CHECK(calls == std::vector<int>(10, 1));
    CHECK(peak <= 4);
    CHECK(peak > 1);
}

TEST_CASE("HttpClient merges default headers and reports HTTP errors")
{
    FakeUi ui;
    net::Request seen;
    net::HttpClient::Config config;
    config.defaultHeaders = {{"User-Agent", "NuvioSwitch/0.1.0"}, {"Accept-Language", "it-IT,en;q=0.7"}};
    config.post           = ui.poster();
    config.transport      = [&seen](const net::Request& request, const std::atomic<bool>&)
    {
        seen = request;
        net::HttpResult result;
        result.status = 404;
        result.body   = "not found";
        result.error  = net::HttpError::Http;
        return result;
    };
    net::HttpClient client(config);

    std::atomic<bool> never{false};
    net::Request request;
    request.url                 = "https://example/missing";
    request.headers["X-Custom"] = "1";
    auto result                 = client.execute(request, never);

    CHECK_FALSE(result.ok);
    CHECK(result.error == net::HttpError::Http);
    CHECK(result.status == 404);
    CHECK(result.body == "not found");
    CHECK(seen.headers["User-Agent"] == "NuvioSwitch/0.1.0");
    CHECK(seen.headers["Accept-Language"] == "it-IT,en;q=0.7");
    CHECK(seen.headers["X-Custom"] == "1");
}

TEST_CASE("a cancelled request never runs its callback")
{
    FakeUi ui;
    std::atomic<bool> release{false};
    net::HttpClient::Config config;
    config.threads   = 1;
    config.post      = ui.poster();
    config.transport = [&release](const net::Request&, const std::atomic<bool>& cancelled)
    {
        while (!release && !cancelled)
            std::this_thread::sleep_for(1ms);
        return okResult("late");
    };
    net::HttpClient client(config);

    bool firstCalled = false, secondCalled = false, thirdCalled = false;
    net::Request request;
    request.url = "https://example/";
    auto first  = client.send(request, [&](net::HttpResult) { firstCalled = true; });  // running
    auto second = client.send(request, [&](net::HttpResult) { secondCalled = true; }); // queued
    auto third  = client.send(request, [&](net::HttpResult) { thirdCalled = true; });

    first.cancel();
    second.cancel();
    release = true;

    for (int tries = 0; tries < 200 && !thirdCalled; tries++)
    {
        std::this_thread::sleep_for(5ms);
        ui.drain();
    }
    CHECK(thirdCalled);
    CHECK_FALSE(firstCalled);
    CHECK_FALSE(secondCalled);
    CHECK(first.isCancelled());
}

TEST_CASE("HttpClient serves fresh responses from the cache")
{
    TempDir tmp;
    int networkCalls = 0;
    int64_t clock    = 0;
    net::HttpResult next = okResult("v1", "max-age=600");

    net::HttpClient::Config config;
    config.cache     = std::make_shared<net::HttpCache>(tmp.path(), net::HttpCache::DEFAULT_MAX_BYTES, [&clock] { return clock; });
    config.post      = [](std::function<void()> fn) { fn(); };
    config.transport = [&](const net::Request&, const std::atomic<bool>&)
    {
        networkCalls++;
        return next;
    };
    net::HttpClient client(config);
    std::atomic<bool> never{false};
    net::Request request;
    request.url = "https://addon/catalog.json";

    CHECK(client.execute(request, never).body == "v1");
    clock = 300;
    auto cached = client.execute(request, never);
    CHECK(cached.body == "v1");
    CHECK(cached.fromCache);
    CHECK(networkCalls == 1);

    SUBCASE("expired entries go to the network and are replaced")
    {
        clock = 660;
        next  = okResult("v2", "max-age=600");
        CHECK(client.execute(request, never).body == "v2");
        CHECK(networkCalls == 2);
        clock = 700;
        CHECK(client.execute(request, never).body == "v2");
        CHECK(networkCalls == 2);
    }

    SUBCASE("NoCache bypasses a fresh entry")
    {
        request.cachePolicy = net::CachePolicy::NoCache;
        client.execute(request, never);
        CHECK(networkCalls == 2);
    }

    SUBCASE("StaleIfError returns the expired body when offline")
    {
        clock = 660;
        next  = net::HttpResult();
        next.error = net::HttpError::Connect;

        CHECK(client.execute(request, never).error == net::HttpError::Connect);

        request.cachePolicy = net::CachePolicy::StaleIfError;
        auto stale          = client.execute(request, never);
        CHECK(stale.ok);
        CHECK(stale.stale);
        CHECK(stale.body == "v1");
    }
}

TEST_CASE("HttpClient doesn't cache server errors")
{
    TempDir tmp;
    int networkCalls = 0;
    net::HttpClient::Config config;
    config.cache     = std::make_shared<net::HttpCache>(tmp.path());
    config.post      = [](std::function<void()> fn) { fn(); };
    config.transport = [&](const net::Request&, const std::atomic<bool>&)
    {
        networkCalls++;
        net::HttpResult result;
        result.status = 500;
        result.error  = net::HttpError::Http;
        return result;
    };
    net::HttpClient client(config);
    std::atomic<bool> never{false};
    net::Request request;
    request.url = "https://addon/broken";

    client.execute(request, never);
    client.execute(request, never);
    CHECK(networkCalls == 2);
}
