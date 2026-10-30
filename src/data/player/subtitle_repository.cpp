#include "data/player/subtitle_repository.hpp"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <functional>

#include "core/json/json.hpp"
#include "core/log.hpp"
#include "data/addons/addon_url.hpp"
#include "data/player/player_rules.hpp"

namespace nuvio::data::player
{

using core::json::Json;
using domain::Subtitle;

namespace
{
SubtitleRepository* sharedRepository = nullptr;

std::string lower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

bool blank(const std::string& s)
{
    return std::all_of(s.begin(), s.end(), [](unsigned char c) { return std::isspace(c); });
}

bool isSubtitleResource(const std::string& name)
{
    return lower(name) == "subtitles" || lower(name) == "subtitle";
}

bool supports(const domain::Addon& addon, const domain::AddonResource& resource, const std::string& type,
    const std::string& id)
{
    if (!resource.types.empty())
    {
        bool listed = std::any_of(resource.types.begin(), resource.types.end(),
            [&type](const std::string& t) { return subtitleType(t) == type; });
        if (!listed)
            return false;
    }
    const auto& prefixes = !resource.idPrefixes.empty() ? resource.idPrefixes : addon.idPrefixes;
    if (prefixes.empty())
        return true;
    return std::any_of(prefixes.begin(), prefixes.end(),
        [&id](const std::string& prefix) { return id.compare(0, prefix.size(), prefix) == 0; });
}
} // namespace

SubtitleRepository& SubtitleRepository::shared()
{
    assert(sharedRepository && "SubtitleRepository::setShared was not called");
    return *sharedRepository;
}

void SubtitleRepository::setShared(SubtitleRepository* repository)
{
    sharedRepository = repository;
}

bool SubtitleRepository::hasShared()
{
    return sharedRepository != nullptr;
}

std::optional<std::vector<Subtitle>> parseSubtitleResponse(const std::string& body, const std::string& addonName)
{
    Json json = Json::parse(body, nullptr, false);
    if (!json.is_object())
        return std::nullopt;
    std::vector<Subtitle> out;
    auto it = json.find("subtitles");
    if (it == json.end() || !it->is_array())
        return out;
    for (const auto& entry : *it)
    {
        if (!entry.is_object())
            continue;
        std::string url = core::json::str(entry, "url");
        if (blank(url))
            continue;
        std::string lang = core::json::str(entry, "lang");
        if (blank(lang))
            lang = core::json::str(entry, "language");
        if (blank(lang))
            lang = "und";
        Subtitle subtitle;
        subtitle.url       = url;
        subtitle.lang      = lang;
        subtitle.addonName = addonName;
        std::string id     = core::json::str(entry, "id");
        subtitle.id        = !blank(id) ? id : lang + "-" + std::to_string(std::hash<std::string>{}(url));
        if (auto headers = entry.find("headers"); headers != entry.end() && headers->is_object())
            for (auto h = headers->begin(); h != headers->end(); ++h)
                if (h.value().is_string())
                    subtitle.headers[h.key()] = h.value().get<std::string>();
        out.push_back(std::move(subtitle));
    }
    return out;
}

std::vector<domain::Addon> subtitleAddons(const std::vector<domain::Addon>& addons, const std::string& type,
    const std::string& id)
{
    std::string wanted = subtitleType(type);
    std::vector<domain::Addon> out;
    for (const auto& addon : addons)
    {
        if (!addon.enabled)
            continue;
        for (const auto& resource : addon.resources)
            if (isSubtitleResource(resource.name) && supports(addon, resource, wanted, id))
            {
                out.push_back(addon);
                break;
            }
    }
    return out;
}

SubtitleRepository::Ticket SubtitleRepository::fetchAll(const domain::PlaybackRequest& request,
    const std::vector<domain::Addon>& addons, UpdateCallback onUpdate, DoneCallback onDone)
{
    Ticket ticket = nextTicket++;
    auto selected = subtitleAddons(addons, request.contentType, request.videoId);

    auto run      = std::make_shared<Run>();
    run->pending  = selected.size();
    run->onUpdate = std::move(onUpdate);
    run->onDone   = std::move(onDone);

    for (const auto& own : request.subtitles)
    {
        Subtitle subtitle;
        subtitle.id             = own.id;
        subtitle.url            = own.url;
        subtitle.lang           = own.lang;
        subtitle.addonName      = request.addonName;
        subtitle.streamProvided = true;
        subtitle.headers        = own.headers;
        run->all.push_back(std::move(subtitle));
    }
    if (!run->all.empty() && run->onUpdate)
        run->onUpdate(run->all);

    if (selected.empty())
    {
        if (run->onDone)
            run->onDone();
        return ticket;
    }

    runs.emplace(ticket, run);
    for (const auto& addon : selected)
    {
        net::Request http;
        http.url = subtitleUrl(addon.baseUrl, request.contentType, request.contentId, request.videoId,
            request.videoHash, request.videoSize, request.filename);
        http.permissiveTls          = true;
        http.connectTimeoutSeconds  = TIMEOUT_SECONDS;
        http.readTimeoutSeconds     = TIMEOUT_SECONDS;

        run->handles.push_back(config.http->sendPrepared(std::move(http),
            [this, run, ticket, name = addon.displayName, host = addons::hostOf(addon.baseUrl)](net::HttpResult response)
            {
                std::optional<std::vector<Subtitle>> parsed;
                if (response.ok)
                    parsed = parseSubtitleResponse(response.body, name);
                std::string error = response.ok ? "invalid response" : response.errorMessage;
                return std::function<void()>([this, run, ticket, name, host, parsed = std::move(parsed), error]() mutable
                    {
                        if (run->cancelled)
                            return;
                        if (!parsed)
                            core::log::info("Subtitles from " + host + " failed: " + error);
                        else if (!parsed->empty())
                        {
                            for (auto& s : *parsed)
                                run->all.push_back(std::move(s));
                            if (run->onUpdate)
                                run->onUpdate(run->all);
                        }
                        if (run->cancelled || --run->pending > 0)
                            return;
                        runs.erase(ticket);
                        if (run->onDone)
                            run->onDone();
                    });
            }));
    }
    return ticket;
}

void SubtitleRepository::cancel(Ticket ticket)
{
    auto it = runs.find(ticket);
    if (it == runs.end())
        return;
    auto run       = it->second;
    run->cancelled = true;
    for (auto& handle : run->handles)
        handle.cancel();
    runs.erase(it);
}

} // namespace nuvio::data::player
