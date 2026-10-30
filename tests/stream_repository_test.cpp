#include <doctest.h>

#include <chrono>
#include <mutex>
#include <thread>

#include "data/streams/stream_repository.hpp"

using namespace nuvio;
using data::streams::StreamRepository;
using domain::Addon;
using domain::AddonResource;

namespace
{
Addon streamAddon(const std::string& baseUrl)
{
    Addon addon;
    addon.baseUrl     = baseUrl;
    addon.displayName = baseUrl;
    AddonResource resource;
    resource.name = "stream";
    addon.resources.push_back(resource);
    return addon;
}

struct Fixture
{
    std::mutex mutex;
    std::vector<std::function<void()>> ui;
    std::vector<std::string> requested;
    std::map<std::string, core::net::HttpResult> responses; // by URL; a missing URL is a 404
    std::unique_ptr<core::net::HttpClient> http;
    std::unique_ptr<StreamRepository> repo;

    Fixture()
    {
        core::net::HttpClient::Config config;
        config.threads   = 1;
        config.transport = [this](const core::net::Request& request, const std::atomic<bool>&)
        {
            std::lock_guard<std::mutex> lock(mutex);
            requested.push_back(request.url);
            auto it = responses.find(request.url);
            if (it != responses.end())
                return it->second;
            core::net::HttpResult notFound;
            notFound.status = 404;
            notFound.error  = core::net::HttpError::Http;
            return notFound;
        };
        config.post = [this](std::function<void()> fn)
        {
            std::lock_guard<std::mutex> lock(mutex);
            ui.push_back(std::move(fn));
        };
        http = std::make_unique<core::net::HttpClient>(config);
        repo = std::make_unique<StreamRepository>(StreamRepository::Config{http.get()});
    }

    void ok(const std::string& url, const std::string& body)
    {
        core::net::HttpResult result;
        result.ok      = true;
        result.status  = 200;
        result.body    = body;
        responses[url] = result;
    }

    template <class Done> bool await(Done done)
    {
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (!done() && std::chrono::steady_clock::now() < deadline)
        {
            std::vector<std::function<void()>> pending;
            {
                std::lock_guard<std::mutex> lock(mutex);
                pending.swap(ui);
            }
            for (auto& fn : pending)
                fn();
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        return done();
    }
};

struct Collected
{
    std::vector<StreamRepository::Group> groups;
    std::optional<StreamRepository::Summary> summary;
};

StreamRepository::Ticket run(Fixture& f, Collected& out, const std::vector<Addon>& addons,
    const std::string& videoId = "tt1")
{
    return f.repo->fetchAll("movie", videoId, addons, [&out](const StreamRepository::Group& g) { out.groups.push_back(g); },
        [&out](const StreamRepository::Summary& s) { out.summary = s; });
}

const char* const ONE = R"({"streams":[{"url":"https://cdn/1.mp4","name":"one"}]})";
} // namespace

TEST_CASE("StreamRepository reports each add-on's group with its installed position")
{
    Fixture f;
    f.ok("https://a/stream/movie/tt1.json", ONE);
    f.ok("https://c/stream/movie/tt1.json",
        R"({"streams":[{"url":"https://cdn/1.mp4"},{"url":"https://cdn/1.mp4"},{"infoHash":"x"}]})");
    Collected out;
    run(f, out, {streamAddon("https://a"), streamAddon("https://b"), streamAddon("https://c")});
    REQUIRE(f.await([&] { return out.summary.has_value(); }));

    REQUIRE(out.groups.size() == 2);
    for (const auto& g : out.groups)
    {
        if (g.addonBaseUrl == "https://a")
            CHECK(g.order == 0);
        else
        {
            CHECK(g.order == 2);
            CHECK(g.streams.size() == 2); // the duplicate is gone
        }
    }
    CHECK(out.summary->groups == 2);
    CHECK(out.summary->queried.size() == 3);
    CHECK(out.summary->failed == std::vector<std::string>{"https://b"});
}

TEST_CASE("StreamRepository falls back to the inline streams of the meta")
{
    Fixture f;
    f.ok("https://a/stream/series/tt1%3A1%3A5.json", R"({"streams":[]})");
    f.ok("https://a/meta/series/tt1.json",
        R"({"meta":{"id":"tt1","name":"Show","videos":[{"id":"tt1:1:4","streams":[{"url":"https://cdn/x"}]},
        {"id":"tt1:1:5","streams":[{"url":"https://cdn/a"},{"url":"https://cdn/b"}]}]}})");
    Collected out;
    f.repo->fetchAll("series", "tt1:1:5", {streamAddon("https://a")},
        [&out](const StreamRepository::Group& g) { out.groups.push_back(g); },
        [&out](const StreamRepository::Summary& s) { out.summary = s; });
    REQUIRE(f.await([&] { return out.summary.has_value(); }));

    REQUIRE(out.groups.size() == 1);
    CHECK(out.groups[0].streams.size() == 2);
    CHECK(*out.groups[0].streams[0].url == "https://cdn/a");
    CHECK(out.summary->failed.empty());
}

TEST_CASE("StreamRepository counts an add-on with no streams anywhere as failed")
{
    Fixture f;
    f.ok("https://a/stream/movie/tt1.json", R"({"streams":[]})");
    Collected out;
    run(f, out, {streamAddon("https://a")});
    REQUIRE(f.await([&] { return out.summary.has_value(); }));
    CHECK(out.groups.empty());
    CHECK(out.summary->failed == std::vector<std::string>{"https://a"});
}

TEST_CASE("StreamRepository with no qualifying add-on finishes at once")
{
    Fixture f;
    Collected out;
    run(f, out, {});
    REQUIRE(out.summary.has_value());
    CHECK(out.summary->queried.empty());
    CHECK(f.requested.empty());
}

TEST_CASE("StreamRepository drops callbacks after cancel")
{
    Fixture f;
    f.ok("https://a/stream/movie/tt1.json", ONE);
    Collected out;
    auto ticket = run(f, out, {streamAddon("https://a")});
    f.repo->cancel(ticket);
    f.await([] { return false; });
    CHECK(out.groups.empty());
    CHECK_FALSE(out.summary.has_value());
}
