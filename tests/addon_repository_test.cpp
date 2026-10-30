#include <doctest.h>

#include <chrono>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

#include "core/net/http_client.hpp"
#include "core/storage/store.hpp"
#include "data/addons/addon_repository.hpp"
#include "test_util.hpp"

using namespace nuvio;
using data::addons::AddonRepository;
using Status = AddonRepository::InstallStatus;
using Json   = nlohmann::json;

namespace
{
const std::string CINEMETA      = "https://v3-cinemeta.strem.io";
const std::string OPENSUBTITLES = "https://opensubtitles-v3.strem.io";

std::string manifest(const std::string& id, const std::string& name, const std::string& version = "1.0.0")
{
    return Json{{"id", id}, {"name", name}, {"version", version}, {"types", {"movie"}},
        {"resources", {Json{{"name", "stream"}, {"types", {"movie"}}, {"idPrefixes", {"tt"}}}}}}
        .dump();
}

// Serves canned responses by URL and records what was requested.
struct FakeServer
{
    std::mutex mutex;
    std::map<std::string, core::net::HttpResult> routes;
    std::vector<core::net::Request> requests;
    bool offline = false;

    void serve(const std::string& url, const std::string& body)
    {
        std::lock_guard<std::mutex> lock(mutex);
        core::net::HttpResult result;
        result.ok     = true;
        result.status = 200;
        result.body   = body;
        routes[url]   = result;
    }

    std::vector<std::string> requestedUrls()
    {
        std::lock_guard<std::mutex> lock(mutex);
        std::vector<std::string> out;
        for (const auto& request : requests)
            out.push_back(request.url);
        return out;
    }

    size_t requestCount()
    {
        std::lock_guard<std::mutex> lock(mutex);
        return requests.size();
    }

    core::net::HttpResult handle(const core::net::Request& request)
    {
        std::lock_guard<std::mutex> lock(mutex);
        requests.push_back(request);
        core::net::HttpResult result;
        if (offline)
        {
            result.error        = core::net::HttpError::Connect;
            result.errorMessage = "Could not connect";
            return result;
        }
        auto it = routes.find(request.url);
        if (it != routes.end())
            return it->second;
        result.status = 404;
        result.error  = core::net::HttpError::Http;
        return result;
    }
};

// The app as the repository sees it: stores in a temp folder, an HTTP client over
// FakeServer whose callbacks are run by pump(), and a settable clock.
struct Harness
{
    TempDir dir;
    FakeServer server;
    std::mutex uiMutex;
    std::vector<std::function<void()>> uiQueue;
    int64_t now = 1'700'000'000;
    std::unique_ptr<core::net::HttpClient> http;

    Harness()
    {
        server.serve(CINEMETA + "/manifest.json", readFixture("cinemeta.json"));
        server.serve(OPENSUBTITLES + "/manifest.json", readFixture("opensubtitles_v3.json"));

        core::net::HttpClient::Config config;
        config.threads   = 2;
        config.transport = [this](const core::net::Request& request, const std::atomic<bool>&)
        { return server.handle(request); };
        config.post = [this](std::function<void()> fn)
        {
            std::lock_guard<std::mutex> lock(uiMutex);
            uiQueue.push_back(std::move(fn));
        };
        http = std::make_unique<core::net::HttpClient>(config);
    }

    std::shared_ptr<core::storage::Store> store(const std::string& name)
    {
        core::storage::Store::flush();
        return std::make_shared<core::storage::Store>(dir.path() + name + ".json");
    }

    // A repository over the stores as they are on disk now, like a fresh app launch.
    std::unique_ptr<AddonRepository> launch(std::vector<std::string> extraUrls = {})
    {
        AddonRepository::Config config;
        config.extraUrls     = std::move(extraUrls);
        config.http          = http.get();
        config.addons        = store("addons");
        config.manifestCache = store("addon_manifest_cache");
        config.now           = [this] { return now; };
        return std::make_unique<AddonRepository>(config);
    }

    // Runs UI callbacks until done() holds, for at most timeout.
    template <typename Pred>
    bool pumpUntil(Pred done, std::chrono::milliseconds timeout = std::chrono::seconds(2))
    {
        auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline)
        {
            std::vector<std::function<void()>> pending;
            {
                std::lock_guard<std::mutex> lock(uiMutex);
                pending.swap(uiQueue);
            }
            for (auto& fn : pending)
                fn();
            if (done())
                return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        return false;
    }

    // Lets requests in flight finish and runs their callbacks.
    void settle(const AddonRepository& repo)
    {
        pumpUntil([&repo]
            {
                for (const auto& addon : repo.addons())
                    if (repo.isLoading(addon.baseUrl))
                        return false;
                return true;
            });
        pumpUntil([] { return false; }, std::chrono::milliseconds(20)); // callbacks queued just after

    }
};

std::vector<std::string> names(const AddonRepository& repo)
{
    std::vector<std::string> out;
    for (const auto& addon : repo.addons())
        out.push_back(addon.displayName);
    return out;
}

using Strings = std::vector<std::string>;
} // namespace

TEST_CASE("First launch installs Cinemeta then OpenSubtitles, enabled, and saves them")
{
    Harness h;
    auto repo = h.launch();
    repo->start();

    // before the manifests arrive both are placeholders named after their host
    REQUIRE(repo->addons().size() == 2);
    CHECK(repo->addons()[0].placeholder);
    CHECK(repo->addons()[0].displayName == "v3-cinemeta.strem.io");
    CHECK(repo->isLoading(CINEMETA));

    REQUIRE(h.pumpUntil([&] { return !repo->addons()[0].placeholder && !repo->addons()[1].placeholder; }));
    CHECK(names(*repo) == Strings{"Cinemeta", "OpenSubtitles v3"});
    CHECK(repo->addons()[0].enabled);
    CHECK(repo->addons()[1].enabled);

    auto stored = h.store("addons");
    CHECK(stored->get("installed_addon_urls_ordered") == Json{CINEMETA, OPENSUBTITLES});
    CHECK(stored->get("installed_addon_enabled_states") == Json{{CINEMETA, true}, {OPENSUBTITLES, true}});

    // manifest requests skip certificate checks and the HTTP cache
    for (const auto& request : h.server.requests)
    {
        CHECK(request.permissiveTls);
        CHECK(request.cachePolicy == core::net::CachePolicy::NoCache);
    }
}

TEST_CASE("Extra URLs are added at the end when missing, once, and saved")
{
    Harness h;
    const std::string extra = "https://extra.example/cfg";
    {
        auto repo = h.launch();
        repo->start();
        h.settle(*repo);
    }
    // an existing install gets it on the next start
    {
        auto repo = h.launch({extra + "/manifest.json", CINEMETA});
        repo->start();
        REQUIRE(repo->addons().size() == 3);
        CHECK(repo->addons()[2].baseUrl == extra);
        CHECK(repo->addons()[2].enabled);
    }
    auto stored = h.store("addons");
    CHECK(stored->get("installed_addon_urls_ordered") == Json{CINEMETA, OPENSUBTITLES, extra});
    // starting again does not duplicate it
    auto repo = h.launch({extra});
    repo->start();
    CHECK(repo->addons().size() == 3);
}

TEST_CASE("Removing every add-on survives a restart; defaults are not re-added")
{
    Harness h;
    {
        auto repo = h.launch();
        repo->start();
        h.settle(*repo);
        CHECK(repo->remove(CINEMETA));
        CHECK(repo->remove("https://OPENSUBTITLES-v3.strem.io/manifest.json"));
        CHECK(repo->addons().empty());
    }
    auto requestsBefore = h.server.requestCount();
    auto repo           = h.launch();
    repo->start();
    CHECK(repo->addons().empty());
    CHECK(h.server.requestCount() == requestsBefore);
}

TEST_CASE("Offline start shows cached manifests straight away")
{
    Harness h;
    {
        auto repo = h.launch();
        repo->start();
        h.settle(*repo);
    }
    h.server.offline = true;
    h.now += 60; // well inside the refresh interval

    auto repo = h.launch();
    repo->start();
    CHECK(names(*repo) == Strings{"Cinemeta", "OpenSubtitles v3"});
    CHECK_FALSE(repo->addons()[0].placeholder);
    CHECK_FALSE(repo->addons()[0].catalogs.empty());

    // the background refresh that starts fails offline and leaves the cache in place
    h.settle(*repo);
    CHECK(names(*repo) == Strings{"Cinemeta", "OpenSubtitles v3"});
    CHECK_FALSE(repo->addons()[0].placeholder);
}

TEST_CASE("An unreachable add-on stays listed as a removable placeholder that serves nothing")
{
    Harness h;
    {
        auto addons = h.store("addons");
        addons->set("installed_addon_urls_ordered", Json{"https://down.example/abc/def?token=X"});
        addons->save();
    }

    auto repo = h.launch();
    repo->start();
    h.settle(*repo);
    REQUIRE(repo->addons().size() == 1);
    const auto& addon = repo->addons()[0];
    CHECK(addon.placeholder);
    CHECK(addon.displayName == "def");
    CHECK(addon.resources.empty());
    CHECK(addon.catalogs.empty());
    CHECK(repo->addonsFor("stream", "movie", "tt1").empty());
    CHECK(repo->remove(addon.baseUrl));
    CHECK(repo->addons().empty());
}

TEST_CASE("Background refresh runs at most every 6 hours, counted from when it starts")
{
    Harness h;
    {
        auto repo = h.launch();
        repo->start(); // cache misses only: fetched, but no refresh is recorded
        h.settle(*repo);
    }
    CHECK(h.server.requestCount() == 2);

    // the next launch finds a stale (never attempted) refresh and starts one
    h.server.offline = true; // every fetch fails, but the attempt still counts
    {
        auto repo = h.launch();
        repo->start();
        h.settle(*repo);
        CHECK(names(*repo) == Strings{"Cinemeta", "OpenSubtitles v3"}); // the old cache survives
    }
    CHECK(h.server.requestCount() == 4);

    h.now += 2 * 60 * 60;
    {
        auto repo = h.launch();
        repo->start();
        h.settle(*repo);
    }
    CHECK(h.server.requestCount() == 4);

    h.now += 4 * 60 * 60 + 1;
    {
        auto repo = h.launch();
        repo->start();
        h.settle(*repo);
    }
    CHECK(h.server.requestCount() == 6);
}

TEST_CASE("Disabled add-ons are never fetched and are left out of queries")
{
    Harness h;
    {
        auto addons = h.store("addons");
        addons->set("installed_addon_urls_ordered", Json{CINEMETA, "https://torrentio.example"});
        addons->set("installed_addon_enabled_states", Json{{"https://torrentio.example/", false}});
        addons->save();
    }
    h.server.serve("https://torrentio.example/manifest.json", manifest("torrentio", "Torrentio"));

    auto repo = h.launch();
    repo->start();
    h.settle(*repo);
    CHECK(h.server.requestedUrls() == Strings{CINEMETA + "/manifest.json"});
    CHECK_FALSE(repo->addons()[1].enabled);
    CHECK(repo->addons()[1].placeholder);

    // enabling it fetches the manifest it never had
    CHECK(repo->setEnabled("https://torrentio.example", true));
    REQUIRE(h.pumpUntil([&] { return !repo->addons()[1].placeholder; }));
    CHECK(repo->addonsFor("stream", "movie", "tt0111161").size() == 1);

    CHECK(repo->setEnabled("https://torrentio.example", false));
    CHECK(repo->addonsFor("stream", "movie", "tt0111161").empty());
    CHECK_FALSE(repo->setEnabled("https://torrentio.example", false));
}

TEST_CASE("Install: stremio:// URL, duplicate, invalid URL and failed fetch")
{
    Harness h;
    h.server.serve("https://example.com/manifest.json", manifest("example", "Example"));
    auto repo = h.launch();
    repo->start();
    h.settle(*repo);

    std::optional<AddonRepository::InstallResult> result;
    auto capture = [&result](AddonRepository::InstallResult r) { result = r; };

    repo->install("stremio://example.com/manifest.json", capture);
    REQUIRE(h.pumpUntil([&] { return result.has_value(); }));
    CHECK(result->status == Status::Installed);
    CHECK(result->name == "Example");
    CHECK(repo->addons().back().baseUrl == "https://example.com");
    CHECK(repo->addons().back().enabled);
    CHECK(h.server.requestedUrls().back() == "https://example.com/manifest.json");

    // same add-on, different case and suffix: refused without a request
    result.reset();
    auto requests = h.server.requestCount();
    repo->install("https://Example.com/", capture);
    REQUIRE(result);
    CHECK(result->status == Status::AlreadyInstalled);
    CHECK(h.server.requestCount() == requests);

    result.reset();
    repo->install("example.com", capture);
    REQUIRE(result);
    CHECK(result->status == Status::InvalidUrl);

    result.reset();
    repo->install("https://missing.example/manifest.json", capture);
    REQUIRE(h.pumpUntil([&] { return result.has_value(); }));
    CHECK(result->status == Status::FetchFailed);
    CHECK(result->detail == "HTTP 404");
    CHECK(repo->addons().size() == 3);

    h.server.serve("https://html.example/manifest.json", "<html>hi</html>");
    result.reset();
    repo->install("https://html.example", capture);
    REQUIRE(h.pumpUntil([&] { return result.has_value(); }));
    CHECK(result->status == Status::InvalidManifest);
    CHECK(repo->addons().size() == 3);

    // installed add-ons survive a restart
    auto again = h.launch();
    again->start();
    CHECK(names(*again) == Strings{"Cinemeta", "OpenSubtitles v3", "Example"});
}

TEST_CASE("Move reorders and saves; remove drops cache, enabled state and custom name")
{
    Harness h;
    {
        auto addons = h.store("addons");
        addons->set("installed_addon_urls_ordered", Json{CINEMETA, OPENSUBTITLES});
        addons->set("addon_user_set_names", Json{{OPENSUBTITLES, "Subs"}});
        addons->save();
    }
    auto repo = h.launch();
    repo->start();
    h.settle(*repo);
    CHECK(names(*repo) == Strings{"Cinemeta", "Subs"});

    int notified = 0;
    int token    = repo->subscribe([&notified] { notified++; });
    CHECK(repo->move(1, -1));
    CHECK(notified == 1);
    CHECK(names(*repo) == Strings{"Subs", "Cinemeta"});
    CHECK_FALSE(repo->move(0, -1));
    CHECK_FALSE(repo->move(1, 1));
    CHECK(h.store("addons")->get("installed_addon_urls_ordered") == Json{OPENSUBTITLES, CINEMETA});

    CHECK(repo->remove(OPENSUBTITLES));
    CHECK(notified == 2);
    repo->unsubscribe(token);
    auto addons = h.store("addons");
    CHECK(addons->get("installed_addon_enabled_states") == Json{{CINEMETA, true}});
    CHECK(addons->get("addon_user_set_names") == Json::object());
    auto cache = h.store("addon_manifest_cache")->get("manifests_v2");
    CHECK(cache.contains(CINEMETA));
    CHECK_FALSE(cache.contains(OPENSUBTITLES));
    CHECK_FALSE(repo->remove(OPENSUBTITLES));
}

TEST_CASE("Forced refresh re-fetches enabled add-ons and reports failures")
{
    Harness h;
    auto repo = h.launch();
    repo->start();
    h.settle(*repo);

    int notified = 0;
    repo->subscribe([&notified] { notified++; });

    std::optional<int> failed;
    h.now += 60;
    CHECK(repo->refreshAll([&failed](int n) { failed = n; }));
    CHECK(repo->isRefreshing());
    CHECK_FALSE(repo->refreshAll());
    REQUIRE(h.pumpUntil([&] { return failed.has_value(); }));
    CHECK(*failed == 0);
    CHECK_FALSE(repo->isRefreshing());
    CHECK(notified == 1); // unchanged manifests: only the end of the refresh is announced
    CHECK(h.store("addon_manifest_cache")->get<int64_t>("lastRefreshAttemptAt", 0) == h.now);

    // a changed manifest replaces the cached one
    h.server.serve(CINEMETA + "/manifest.json", manifest("com.linvo.cinemeta", "Cinemeta", "9.9.9"));
    h.server.offline = false;
    failed.reset();
    repo->refreshAll([&failed](int n) { failed = n; });
    REQUIRE(h.pumpUntil([&] { return failed.has_value(); }));
    CHECK(repo->addons()[0].version == "9.9.9");

    h.server.offline = true;
    failed.reset();
    repo->refreshAll([&failed](int n) { failed = n; });
    REQUIRE(h.pumpUntil([&] { return failed.has_value(); }));
    CHECK(*failed == 2);
    CHECK(repo->addons()[0].version == "9.9.9");
}
