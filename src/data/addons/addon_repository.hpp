#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "core/net/http_client.hpp"
#include "core/storage/store.hpp"
#include "domain/addon.hpp"

namespace nuvio::data::addons
{

namespace net = core::net;

// The installed add-ons, in priority order, with their manifests
// (NuvioTV AddonRepositoryImpl.kt and AddonPreferences.kt).
//
// Install state lives in the per-profile store "addons" under NuvioTV's key names;
// raw manifests live in the global store "addon_manifest_cache". Manifests are
// fetched with permissive TLS, parsed on the HTTP workers and applied on the UI
// thread. Every method must be called on the UI thread.
class AddonRepository
{
  public:
    using Json     = nlohmann::json;
    using Clock    = std::function<int64_t()>; // unix seconds
    using Listener = std::function<void()>;

    static constexpr int64_t REFRESH_INTERVAL_SECONDS = 6 * 60 * 60;

    // Cinemeta and OpenSubtitles v3, installed when no list was ever stored.
    static const std::vector<std::string>& defaultUrls();

    struct Config
    {
        net::HttpClient* http = nullptr;
        std::shared_ptr<core::storage::Store> addons;        // per profile
        std::shared_ptr<core::storage::Store> manifestCache; // global
        Clock now;                                           // defaults to cache::unixNow
        // Installed at the end of the list on every start when missing (a local development aid:
        // main.cpp fills it from the git-ignored resources/local_addons.txt).
        std::vector<std::string> extraUrls;
    };

    enum class InstallStatus
    {
        Installed,
        InvalidUrl,       // not http(s) or stremio
        AlreadyInstalled, // same canonical URL, ignoring case
        FetchFailed,      // detail holds the reason
        InvalidManifest,
    };

    struct InstallResult
    {
        InstallStatus status = InstallStatus::Installed;
        std::string name;   // display name, when installed
        std::string detail; // "HTTP 404", a curl message...
    };

    explicit AddonRepository(Config config);
    ~AddonRepository(); // cancels pending fetches

    // Loads the stored list and cached manifests, then fetches missing manifests
    // and starts the background refresh when the last one started over 6 hours ago.
    void start();

    // Every installed add-on in priority order, disabled ones and placeholders included.
    const std::vector<domain::Addon>& addons() const { return list; }

    // The enabled add-ons serving resource for type and id, in priority order.
    std::vector<domain::Addon> addonsFor(const std::string& resource, const std::string& type,
        const std::string& id) const;

    // A manifest request for url is in flight.
    bool isLoading(const std::string& url) const;

    // Validates input, fetches the manifest and appends the add-on, enabled.
    void install(const std::string& input, std::function<void(InstallResult)> done);

    // Each returns false when nothing changed (unknown URL, same value, edge of the list).
    bool remove(const std::string& url);
    bool setEnabled(const std::string& url, bool enabled);
    bool move(size_t index, int delta);

    // Re-fetches every enabled add-on now; done gets the number of failures.
    // Returns false, without calling done, while a forced refresh is still running.
    bool refreshAll(std::function<void(int failed)> done = nullptr);
    bool isRefreshing() const { return refreshing; }

    // Listeners run on the UI thread after the list or a manifest changes.
    int subscribe(Listener listener);
    void unsubscribe(int token);

    // The app-wide instance, set up in main.cpp.
    static AddonRepository& shared();
    static void setShared(AddonRepository* repository);
    static bool hasShared();

  private:
    struct Cached
    {
        Json raw;
        int64_t fetchedAt = 0;
        domain::Addon addon;
    };

    // A manifest response, parsed on the worker.
    struct Fetched
    {
        Json raw;
        std::optional<domain::Addon> addon;
        std::string error; // empty on success
        bool invalidManifest = false;
    };

    void loadInstallState();
    void loadManifestCache();
    void saveInstallState();
    void saveManifestCache();

    // Index in urls of the add-on with this URL, compared canonically and ignoring case.
    std::optional<size_t> indexOf(const std::string& url) const;
    domain::Addon placeholder(const std::string& url) const;
    void rebuild();
    void notify();

    net::RequestHandle request(const std::string& url, std::function<void(Fetched)> onResult);
    void fetch(const std::string& url, std::function<void(bool ok)> done = nullptr);
    bool storeManifest(const std::string& url, Fetched& fetched);

    Config config;

    std::vector<std::string> urls; // canonical, in priority order
    std::map<std::string, bool> enabledStates;
    std::map<std::string, std::string> userNames;
    std::map<std::string, Cached> manifests;
    int64_t lastRefreshAttemptAt = 0;

    std::map<std::string, net::RequestHandle> inFlight; // manifest fetches by URL
    std::map<int, net::RequestHandle> installs;
    int nextInstall = 1;
    bool refreshing = false;

    std::vector<domain::Addon> list;
    std::map<int, Listener> listeners;
    int nextListener = 1;
};

} // namespace nuvio::data::addons
