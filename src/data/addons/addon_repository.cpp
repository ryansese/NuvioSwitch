#include "data/addons/addon_repository.hpp"

#include <cassert>
#include <cctype>

#include "core/cache/disk_cache.hpp"
#include "core/log.hpp"
#include "data/addons/addon_display.hpp"
#include "data/addons/addon_query.hpp"
#include "data/addons/addon_url.hpp"
#include "data/addons/manifest_parser.hpp"

namespace nuvio::data::addons
{

using domain::Addon;

namespace
{
AddonRepository* sharedRepository = nullptr;

// NuvioTV's AddonPreferences keys, so account sync (0014) maps them one to one.
const char* const KEY_URLS    = "installed_addon_urls_ordered";
const char* const KEY_ENABLED = "installed_addon_enabled_states";
const char* const KEY_NAMES   = "addon_user_set_names";

const char* const KEY_MANIFESTS    = "manifests_v2";
const char* const KEY_LAST_REFRESH = "lastRefreshAttemptAt";

bool isBlank(const std::string& s)
{
    for (unsigned char c : s)
        if (!std::isspace(c))
            return false;
    return true;
}

std::string describe(const net::HttpResult& result)
{
    if (result.error == net::HttpError::Http)
        return "HTTP " + std::to_string(result.status);
    if (!result.errorMessage.empty())
        return result.errorMessage;
    return "network error";
}
} // namespace

const std::vector<std::string>& AddonRepository::defaultUrls()
{
    static const std::vector<std::string> urls{"https://v3-cinemeta.strem.io", "https://opensubtitles-v3.strem.io"};
    return urls;
}

AddonRepository& AddonRepository::shared()
{
    assert(sharedRepository && "AddonRepository::setShared was not called");
    return *sharedRepository;
}

void AddonRepository::setShared(AddonRepository* repository)
{
    sharedRepository = repository;
}

bool AddonRepository::hasShared()
{
    return sharedRepository != nullptr;
}

AddonRepository::AddonRepository(Config cfg) : config(std::move(cfg))
{
    if (!config.now)
        config.now = core::cache::unixNow;
}

AddonRepository::~AddonRepository()
{
    for (auto& [url, handle] : inFlight)
        handle.cancel();
    for (auto& [id, handle] : installs)
        handle.cancel();
}

void AddonRepository::start()
{
    loadInstallState();
    loadManifestCache();
    rebuild();
    notify();

    std::vector<std::string> cached;
    for (const auto& url : urls)
    {
        if (!enabledStates[url])
            continue;
        if (manifests.count(url))
            cached.push_back(url);
        else
            fetch(url);
    }

    // recorded when the refresh starts, so a run where every fetch fails still waits 6 hours
    if (!cached.empty() && config.now() - lastRefreshAttemptAt > REFRESH_INTERVAL_SECONDS)
    {
        lastRefreshAttemptAt = config.now();
        saveManifestCache();
        core::log::info("Refreshing " + std::to_string(cached.size()) + " add-on manifests");
        for (const auto& url : cached)
            fetch(url);
    }
}

// --- persistence ---

void AddonRepository::loadInstallState()
{
    urls.clear();
    enabledStates.clear();
    userNames.clear();

    Json stored = config.addons->get(KEY_URLS);
    if (stored.is_array())
    {
        for (const auto& entry : stored)
            if (entry.is_string() && !indexOf(entry.get<std::string>()))
                urls.push_back(canonicalize(entry.get<std::string>()));
    }
    else
    {
        // never stored (or unreadable): the defaults, saved so that removing them sticks
        urls = defaultUrls();
    }

    Json enabled = config.addons->get(KEY_ENABLED);
    if (enabled.is_object())
        for (const auto& [url, value] : enabled.items())
            if (value.is_boolean())
                enabledStates[canonicalize(url)] = value.get<bool>();

    Json names = config.addons->get(KEY_NAMES);
    if (names.is_object())
        for (const auto& [url, value] : names.items())
            if (value.is_string())
                userNames[canonicalize(url)] = value.get<std::string>();

    for (const auto& url : urls)
        enabledStates.emplace(url, true);

    if (!stored.is_array())
        saveInstallState();
}

void AddonRepository::saveInstallState()
{
    Json enabled = Json::object();
    for (const auto& url : urls)
        enabled[url] = enabledStates[url];

    Json names = Json::object();
    for (const auto& [url, name] : userNames)
        names[url] = name;

    config.addons->set(KEY_URLS, urls);
    config.addons->set(KEY_ENABLED, std::move(enabled));
    config.addons->set(KEY_NAMES, std::move(names));
    config.addons->save();
}

void AddonRepository::loadManifestCache()
{
    manifests.clear();
    lastRefreshAttemptAt = config.manifestCache->get<int64_t>(KEY_LAST_REFRESH, 0);

    Json stored = config.manifestCache->get(KEY_MANIFESTS);
    if (!stored.is_object())
        return;
    for (const auto& [key, entry] : stored.items())
    {
        // drop manifests of add-ons that are no longer installed
        auto index = indexOf(key);
        if (!index || !entry.is_object())
            continue;
        const std::string& url = urls[*index];
        Json raw               = entry.value("manifest", Json());
        auto addon             = parseManifest(raw, url);
        if (!addon)
            continue;
        manifests[url] = Cached{std::move(raw), entry.value("fetchedAt", int64_t(0)), std::move(*addon)};
    }
}

void AddonRepository::saveManifestCache()
{
    Json stored = Json::object();
    for (const auto& [url, cached] : manifests)
        stored[url] = {{"manifest", cached.raw}, {"fetchedAt", cached.fetchedAt}};
    config.manifestCache->set(KEY_MANIFESTS, std::move(stored));
    config.manifestCache->set(KEY_LAST_REFRESH, lastRefreshAttemptAt);
    config.manifestCache->save();
}

// --- list ---

std::optional<size_t> AddonRepository::indexOf(const std::string& url) const
{
    const std::string key = normalizeForCompare(url);
    for (size_t i = 0; i < urls.size(); i++)
        if (normalizeForCompare(urls[i]) == key)
            return i;
    return std::nullopt;
}

Addon AddonRepository::placeholder(const std::string& url) const
{
    std::string name;
    auto custom = userNames.find(url);
    if (custom != userNames.end() && !isBlank(custom->second))
        name = custom->second;
    else
    {
        std::string path = url.substr(0, url.find('?'));
        name             = path.substr(path.rfind('/') + 1);
        if (name.empty())
            name = url;
    }

    Addon addon;
    addon.id          = url;
    addon.name        = name;
    addon.displayName = name;
    addon.baseUrl     = url;
    addon.placeholder = true;
    return addon;
}

void AddonRepository::rebuild()
{
    list.clear();
    for (const auto& url : urls)
    {
        auto cached = manifests.find(url);
        Addon addon = cached != manifests.end() ? cached->second.addon : placeholder(url);
        addon.baseUrl = url;
        addon.enabled = enabledStates[url];
        list.push_back(std::move(addon));
    }
    applyDisplayNames(list, userNames);
}

void AddonRepository::notify()
{
    // a listener may unsubscribe while we iterate
    auto current = listeners;
    for (auto& [token, listener] : current)
        listener();
}

std::vector<Addon> AddonRepository::addonsFor(const std::string& resource, const std::string& type,
    const std::string& id) const
{
    return addons::addonsFor(list, resource, type, id);
}

bool AddonRepository::isLoading(const std::string& url) const
{
    auto index = indexOf(url);
    return index && inFlight.count(urls[*index]);
}

int AddonRepository::subscribe(Listener listener)
{
    int token = nextListener++;
    listeners.emplace(token, std::move(listener));
    return token;
}

void AddonRepository::unsubscribe(int token)
{
    listeners.erase(token);
}

// --- fetching ---

net::RequestHandle AddonRepository::request(const std::string& url, std::function<void(Fetched)> onResult)
{
    net::Request req;
    req.url           = manifestUrl(url);
    req.permissiveTls = true;
    // the manifest cache decides freshness, not Cache-Control
    req.cachePolicy = net::CachePolicy::NoCache;

    return config.http->sendPrepared(std::move(req), [url, onResult = std::move(onResult)](net::HttpResult result)
        {
            // on the worker: big manifests are parsed off the UI thread
            Fetched fetched;
            if (!result.ok)
                fetched.error = describe(result);
            else
            {
                fetched.raw = Json::parse(result.body, nullptr, false);
                if (!fetched.raw.is_discarded())
                    fetched.addon = parseManifest(fetched.raw, url);
                if (!fetched.addon)
                {
                    fetched.error           = "invalid manifest";
                    fetched.invalidManifest = true;
                }
            }
            return std::function<void()>([onResult, fetched = std::move(fetched)]() mutable { onResult(std::move(fetched)); });
        });
}

bool AddonRepository::storeManifest(const std::string& url, Fetched& fetched)
{
    auto existing = manifests.find(url);
    if (existing != manifests.end() && existing->second.raw == fetched.raw)
        return false;
    manifests[url] = Cached{std::move(fetched.raw), config.now(), std::move(*fetched.addon)};
    saveManifestCache();
    return true;
}

void AddonRepository::fetch(const std::string& url, std::function<void(bool ok)> done)
{
    if (auto running = inFlight.find(url); running != inFlight.end())
        running->second.cancel();

    inFlight[url] = request(url, [this, url, done](Fetched fetched)
        {
            inFlight.erase(url);
            // removed while the request was running; still report back so a refresh batch completes
            if (!indexOf(url))
            {
                if (done)
                    done(true);
                return;
            }

            bool wasMissing = !manifests.count(url);
            bool changed    = false;
            if (fetched.error.empty())
                changed = storeManifest(url, fetched);
            else
                core::log::warning("Add-on manifest from " + hostOf(url) + " failed: " + fetched.error);

            rebuild();
            if (changed || wasMissing)
                notify();
            if (done)
                done(fetched.error.empty());
        });
}

void AddonRepository::install(const std::string& input, std::function<void(InstallResult)> done)
{
    auto url = normalizeInstallInput(input);
    if (!url)
        return done({InstallStatus::InvalidUrl, "", ""});
    if (indexOf(*url))
        return done({InstallStatus::AlreadyInstalled, "", ""});

    int id      = nextInstall++;
    installs[id] = request(*url, [this, id, url = *url, done](Fetched fetched)
        {
            installs.erase(id);
            if (!fetched.error.empty())
            {
                core::log::warning("Installing add-on from " + hostOf(url) + " failed: " + fetched.error);
                auto status = fetched.invalidManifest ? InstallStatus::InvalidManifest : InstallStatus::FetchFailed;
                return done({status, "", fetched.error});
            }
            // the same add-on finished installing while this request ran
            if (indexOf(url))
                return done({InstallStatus::AlreadyInstalled, "", ""});

            urls.push_back(url);
            enabledStates[url] = true;
            storeManifest(url, fetched);
            saveInstallState();
            rebuild();
            notify();
            done({InstallStatus::Installed, list.back().displayName, ""});
        });
}

// --- edits ---

bool AddonRepository::remove(const std::string& input)
{
    auto index = indexOf(input);
    if (!index)
        return false;
    std::string url = urls[*index];

    urls.erase(urls.begin() + *index);
    enabledStates.erase(url);
    userNames.erase(url);
    // a fetch still running for it finishes and is ignored (see fetch)
    if (manifests.erase(url))
        saveManifestCache();

    saveInstallState();
    rebuild();
    notify();
    return true;
}

bool AddonRepository::setEnabled(const std::string& input, bool enabled)
{
    auto index = indexOf(input);
    if (!index)
        return false;
    const std::string& url = urls[*index];
    if (enabledStates[url] == enabled)
        return false;

    enabledStates[url] = enabled;
    saveInstallState();
    if (enabled && !manifests.count(url) && !inFlight.count(url))
        fetch(url);
    rebuild();
    notify();
    return true;
}

bool AddonRepository::move(size_t index, int delta)
{
    long target = static_cast<long>(index) + delta;
    if (index >= urls.size() || target < 0 || target >= static_cast<long>(urls.size()) || delta == 0)
        return false;

    std::string url = urls[index];
    urls.erase(urls.begin() + index);
    urls.insert(urls.begin() + target, url);
    saveInstallState();
    rebuild();
    notify();
    return true;
}

bool AddonRepository::refreshAll(std::function<void(int failed)> done)
{
    if (refreshing)
        return false;

    std::vector<std::string> enabled;
    for (const auto& url : urls)
        if (enabledStates[url])
            enabled.push_back(url);

    lastRefreshAttemptAt = config.now();
    saveManifestCache();
    if (enabled.empty())
    {
        if (done)
            done(0);
        return true;
    }

    struct Batch
    {
        size_t remaining;
        int failed = 0;
        std::function<void(int)> done;
    };
    auto batch = std::make_shared<Batch>(Batch{enabled.size(), 0, std::move(done)});
    refreshing = true;
    for (const auto& url : enabled)
        fetch(url, [this, batch](bool ok)
            {
                if (!ok)
                    batch->failed++;
                if (--batch->remaining > 0)
                    return;
                refreshing = false;
                notify();
                if (batch->done)
                    batch->done(batch->failed);
            });
    return true;
}

} // namespace nuvio::data::addons
