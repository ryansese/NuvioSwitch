#include "data/meta/meta_repository.hpp"

#include <algorithm>
#include <cassert>

#include "core/log.hpp"
#include "data/addons/addon_url.hpp"
#include "data/meta/meta_parser.hpp"

namespace nuvio::data::meta
{

namespace
{
MetaRepository* sharedRepository = nullptr;
}

MetaRepository& MetaRepository::shared()
{
    assert(sharedRepository && "MetaRepository::setShared was not called");
    return *sharedRepository;
}

void MetaRepository::setShared(MetaRepository* repository)
{
    sharedRepository = repository;
}

bool MetaRepository::hasShared()
{
    return sharedRepository != nullptr;
}

std::string MetaRepository::cacheKey(const std::string& type, const std::string& id)
{
    return type + "|" + id;
}

MetaRepository::Ticket MetaRepository::lookup(const std::string& type, const std::string& id,
    const std::string& sourceAddonBaseUrl, const std::vector<domain::Addon>& addons, Callback callback)
{
    Ticket ticket   = nextTicket++;
    std::string key = cacheKey(type, id);

    if (auto cached = cache.get(key, config.now()))
    {
        Result result;
        result.ok   = true;
        result.meta = std::move(*cached);
        callback(std::move(result));
        return ticket;
    }

    if (auto running = inFlight.find(key); running != inFlight.end())
    {
        running->second.waiters.push_back({ticket, std::move(callback)});
        return ticket;
    }

    Fetch fetch;
    for (const auto& candidate : candidates(addons, type, id))
        fetch.attempts.push_back(
            {buildMetaUrl(candidate.addon.baseUrl, candidate.type, id), candidate.type, candidate.addon.baseUrl});

    // the add-on the title came from, when it is installed and enabled and wasn't tried
    if (!sourceAddonBaseUrl.empty())
    {
        std::string source = addons::normalizeForCompare(sourceAddonBaseUrl);
        auto installed     = std::find_if(addons.begin(), addons.end(), [&source](const domain::Addon& a)
            { return a.enabled && addons::normalizeForCompare(a.baseUrl) == source; });
        bool tried = std::any_of(fetch.attempts.begin(), fetch.attempts.end(), [&](const Attempt& a)
            { return addons::normalizeForCompare(a.addonBaseUrl) == source && a.type == type; });
        if (installed != addons.end() && !tried)
            fetch.attempts.push_back({buildMetaUrl(installed->baseUrl, type, id), type, installed->baseUrl});
    }

    fetch.waiters.push_back({ticket, std::move(callback)});
    inFlight.emplace(key, std::move(fetch));
    tryNext(key);
    return ticket;
}

void MetaRepository::cancel(Ticket ticket)
{
    for (auto& [key, fetch] : inFlight)
        fetch.waiters.erase(std::remove_if(fetch.waiters.begin(), fetch.waiters.end(),
                                [ticket](const Waiter& w) { return w.ticket == ticket; }),
            fetch.waiters.end());
}

void MetaRepository::tryNext(const std::string& key)
{
    auto it = inFlight.find(key);
    if (it == inFlight.end())
        return;
    Fetch& fetch = it->second;

    if (fetch.next >= fetch.attempts.size())
    {
        Result result;
        result.error = "not found";
        finish(key, std::move(result));
        return;
    }

    const Attempt attempt = fetch.attempts[fetch.next++];
    net::Request request;
    request.url           = attempt.url;
    request.permissiveTls = true;

    config.http->sendPrepared(std::move(request),
        [this, key, attempt](net::HttpResult response)
        {
            // on the worker: meta with a long video list can be large
            std::optional<domain::Meta> parsed;
            std::string error;
            if (!response.ok)
                error = response.error == net::HttpError::Http ? "HTTP " + std::to_string(response.status)
                                                                : response.errorMessage;
            else if (!(parsed = parseMeta(response.body, attempt.type, attempt.addonBaseUrl)))
                error = "no meta";

            auto ttl = metaTtl(response.headers);
            return std::function<void()>(
                [this, key, attempt, parsed = std::move(parsed), error, ttl]() mutable
                {
                    if (!parsed)
                    {
                        core::log::info("Meta " + key + " from " + addons::hostOf(attempt.addonBaseUrl) +
                                        " failed: " + error);
                        tryNext(key);
                        return;
                    }
                    cache.put(key, *parsed, ttl, config.now());
                    Result result;
                    result.ok   = true;
                    result.meta = std::move(*parsed);
                    finish(key, std::move(result));
                });
        });
}

void MetaRepository::finish(const std::string& key, Result result)
{
    auto it = inFlight.find(key);
    if (it == inFlight.end())
        return;
    std::vector<Waiter> waiters = std::move(it->second.waiters);
    inFlight.erase(it);
    for (auto& waiter : waiters)
        waiter.callback(result);
}

} // namespace nuvio::data::meta
