#include <borealis.hpp>
#include <cstdlib>
#include <cstring>
#include <curl/curl.h>
#include <memory>

#include "activity/main_activity.hpp"
#include "core/async/ui_thread.hpp"
#include "core/image/image_loader.hpp"
#include "core/log.hpp"
#include "core/net/curl_executor.hpp"
#include "core/net/headers.hpp"
#include "core/net/http_client.hpp"
#include "core/storage/paths.hpp"
#include "core/storage/store.hpp"
#include "data/addons/addon_repository.hpp"
#include "data/catalog/catalog_repository.hpp"
#include "data/meta/meta_repository.hpp"
#include "tabs/placeholder_tab.hpp"
#include "ui/addons/addons_tab.hpp"
#include "ui/home/home_tab.hpp"
#include "ui/search/search_tab.hpp"
#include "version.hpp"

using namespace nuvio::core;
using nuvio::data::addons::AddonRepository;

using namespace brls::literals; // for _i18n

// Nuvio's default dark theme (NuvioTV ui/theme/PrimitiveTokens.kt).
static void applyNuvioTheme()
{
    brls::Theme& dark = brls::Theme::getDarkTheme();
    dark.addColor("brls/background", nvgRGB(0x0D, 0x0D, 0x0D));
    dark.addColor("brls/backdrop", nvgRGBA(0x0D, 0x0D, 0x0D, 0xB2));
    dark.addColor("brls/sidebar/background", nvgRGB(0x1A, 0x1A, 0x1A));
    dark.addColor("brls/sidebar/active_item", nvgRGB(0xFF, 0xFF, 0xFF));
    dark.addColor("brls/text", nvgRGB(0xFF, 0xFF, 0xFF));
    dark.addColor("brls/text_disabled", nvgRGB(0xB3, 0xB3, 0xB3));
    dark.addColor("brls/accent", nvgRGB(0xFF, 0xFF, 0xFF));
    dark.addColor("brls/highlight/color1", nvgRGB(0xFF, 0xFF, 0xFF));
    dark.addColor("brls/highlight/color2", nvgRGB(0xFF, 0xFF, 0xFF));
    dark.addColor("brls/highlight/background", nvgRGB(0x30, 0x30, 0x30));
    dark.addColor("nuvio/card", nvgRGB(0x24, 0x24, 0x24));
}

static void routeCoreLogging()
{
    log::setSink([](log::Level level, const std::string& message)
        {
            switch (level)
            {
                case log::Level::Debug: brls::Logger::debug("{}", message); break;
                case log::Level::Info: brls::Logger::info("{}", message); break;
                case log::Level::Warning: brls::Logger::warning("{}", message); break;
                case log::Level::Error: brls::Logger::error("{}", message); break;
            }
        });
}

#ifdef NUVIO_DEBUG_NETWORK
// Checks that verified TLS works on this platform (switch-curl may need a CA bundle).
static void probeHttps()
{
    net::Request request;
    request.url         = "https://v3-cinemeta.strem.io/manifest.json";
    request.cachePolicy = net::CachePolicy::NoCache;
    net::HttpClient::shared().send(request, [](net::HttpResult result)
        {
            if (result.ok)
                brls::Logger::info("HTTPS check: {} {} ({} bytes)", result.status, result.finalUrl, result.body.size());
            else
                brls::Logger::error("HTTPS check failed: {}", result.errorMessage);
        });
}
#endif

int main(int argc, char* argv[])
{
    for (int i = 1; i < argc; i++)
    {
        if (std::strcmp(argv[i], "-d") == 0)
            brls::Logger::setLogLevel(brls::LogLevel::LOG_DEBUG);
        else if (std::strcmp(argv[i], "-v") == 0)
            brls::Application::enableDebuggingView(true);
    }

    brls::Platform::APP_LOCALE_DEFAULT = brls::LOCALE_AUTO;

    if (!brls::Application::init())
    {
        brls::Logger::error("Unable to init borealis");
        return EXIT_FAILURE;
    }

    brls::Application::createWindow(NUVIO_APP_NAME);

    routeCoreLogging();
    if (!storage::Paths::ensureCreated())
        brls::Logger::error("Unable to create the data folder {}", storage::Paths::dataDir());

    // services: HTTP on a 4-thread pool, images on their own 4 threads, both over one curl share
    curl_global_init(CURL_GLOBAL_DEFAULT);
    net::Headers headers = net::defaultHeaders(NUVIO_APP_VERSION, brls::Application::getLocale());
    auto executor        = std::make_unique<net::CurlExecutor>();
    auto transport       = [&executor](const net::Request& request, const std::atomic<bool>& cancelled)
    { return executor->perform(request, cancelled); };

    auto http = std::make_unique<net::HttpClient>(net::HttpClient::Config{
        4, headers, std::make_shared<net::HttpCache>(storage::Paths::httpCacheDir()), transport, async::postToUi});
    net::HttpClient::setShared(http.get());

    // applets (album mode) get much less memory than a full application
    uint64_t imageMemory = image::ImageCache::MEMORY_BYTES;
    if (!brls::Application::getPlatform()->isApplicationMode())
        imageMemory /= 2;
    auto images = std::make_unique<image::ImageLoader>(transport, headers, storage::Paths::imageCacheDir(), imageMemory);
    image::ImageLoader::setShared(images.get());

    // add-ons: cached manifests are shown at once, missing and stale ones fetched in the background
    auto addons = std::make_unique<AddonRepository>(AddonRepository::Config{http.get(),
        storage::Store::open("addons"), storage::Store::open("addon_manifest_cache", storage::Scope::Global)});
    AddonRepository::setShared(addons.get());
    addons->start();

    auto catalogs = std::make_unique<nuvio::data::catalog::CatalogRepository>(*http);
    nuvio::data::catalog::CatalogRepository::setShared(catalogs.get());

    auto metas = std::make_unique<nuvio::data::meta::MetaRepository>(nuvio::data::meta::MetaRepository::Config{http.get()});
    nuvio::data::meta::MetaRepository::setShared(metas.get());

    applyNuvioTheme();
    brls::Application::getPlatform()->setThemeVariant(brls::ThemeVariant::DARK);

    // + (Start) quits from any activity; borealis leaves this off by default
    brls::Application::setGlobalQuit(true);

    brls::Application::registerXMLView("PlaceholderTab", PlaceholderTab::create);
    brls::Application::registerXMLView("AddonsTab", AddonsTab::create);
    brls::Application::registerXMLView("HomeTab", HomeTab::create);
    brls::Application::registerXMLView("SearchTab", SearchTab::create);

    brls::Application::pushActivity(new MainActivity());

#ifdef NUVIO_DEBUG_NETWORK
    probeHttps();
#endif

    while (brls::Application::mainLoop())
        ;

    // stop workers before the curl share and global state they use go away
    nuvio::data::meta::MetaRepository::setShared(nullptr);
    metas.reset();
    nuvio::data::catalog::CatalogRepository::setShared(nullptr);
    catalogs.reset();
    AddonRepository::setShared(nullptr);
    addons.reset();
    image::ImageLoader::setShared(nullptr);
    images.reset();
    net::HttpClient::setShared(nullptr);
    http.reset();
    executor.reset();
    storage::Store::shutdown();
    curl_global_cleanup();

    return EXIT_SUCCESS;
}
