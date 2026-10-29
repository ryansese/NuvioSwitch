#include <doctest.h>

#include <chrono>
#include <mutex>
#include <thread>

#include "data/catalog/catalog_repository.hpp"
#include "test_util.hpp"

using namespace nuvio;
using data::catalog::CatalogRepository;

namespace
{
struct Fixture
{
    std::mutex mutex;
    std::vector<std::function<void()>> ui;
    std::vector<core::net::Request> requests;
    core::net::HttpResult response;
    std::unique_ptr<core::net::HttpClient> http;

    Fixture()
    {
        core::net::HttpClient::Config config;
        config.threads   = 1;
        config.transport = [this](const core::net::Request& request, const std::atomic<bool>&)
        {
            std::lock_guard<std::mutex> lock(mutex);
            requests.push_back(request);
            return response;
        };
        config.post = [this](std::function<void()> fn)
        {
            std::lock_guard<std::mutex> lock(mutex);
            ui.push_back(std::move(fn));
        };
        http = std::make_unique<core::net::HttpClient>(config);
    }

    // Runs posted callbacks until one has produced a result.
    std::optional<CatalogRepository::Result> await(std::optional<CatalogRepository::Result>& result)
    {
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (!result && std::chrono::steady_clock::now() < deadline)
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
        return result;
    }
};

domain::CatalogSource cinemetaTop()
{
    domain::CatalogSource source;
    source.addonId              = "com.linvo.cinemeta";
    source.addonBaseUrl         = "https://v3-cinemeta.strem.io";
    source.addonName            = "Cinemeta";
    source.catalog.type         = "movie";
    source.catalog.id           = "top";
    source.catalog.name         = "Popular";
    source.catalog.extraSupported = {"search", "genre", "skip"};
    return source;
}
} // namespace

TEST_CASE("CatalogRepository loads, parses and pages a catalog")
{
    Fixture f;
    f.response.ok     = true;
    f.response.status = 200;
    f.response.body   = readFixture("catalog/cinemeta_top_movie.json");
    CatalogRepository repo(*f.http);

    std::optional<CatalogRepository::Result> result;
    repo.load(cinemetaTop(), 100, {}, [&result](CatalogRepository::Result r) { result = r; });
    REQUIRE(f.await(result));
    CHECK(result->ok);
    CHECK(result->page.items.size() == 12);
    CHECK(result->page.hasMore);
    CHECK(result->page.nextSkip == 112);

    REQUIRE(f.requests.size() == 1);
    CHECK(f.requests[0].url == "https://v3-cinemeta.strem.io/catalog/movie/top/skip=100.json");
    CHECK(f.requests[0].permissiveTls);
    CHECK(f.requests[0].cachePolicy == core::net::CachePolicy::Default);
}

TEST_CASE("CatalogRepository reports HTTP errors and bad bodies")
{
    Fixture f;
    f.response.status = 500;
    f.response.error  = core::net::HttpError::Http;
    CatalogRepository repo(*f.http);

    std::optional<CatalogRepository::Result> result;
    repo.load(cinemetaTop(), 0, {}, [&result](CatalogRepository::Result r) { result = r; });
    REQUIRE(f.await(result));
    CHECK_FALSE(result->ok);
    CHECK(result->error == "HTTP 500");

    f.response        = {};
    f.response.ok     = true;
    f.response.status = 200;
    f.response.body   = "<html>";
    result.reset();
    repo.load(cinemetaTop(), 0, {}, [&result](CatalogRepository::Result r) { result = r; });
    REQUIRE(f.await(result));
    CHECK_FALSE(result->ok);
    CHECK(result->error == "invalid response");
}
