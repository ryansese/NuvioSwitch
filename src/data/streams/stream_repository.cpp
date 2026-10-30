#include "data/streams/stream_repository.hpp"

#include <algorithm>
#include <cassert>

#include "core/log.hpp"
#include "data/addons/addon_url.hpp"
#include "data/meta/meta_candidates.hpp"
#include "data/meta/meta_parser.hpp"
#include "data/streams/stream_parser.hpp"
#include "data/streams/stream_rules.hpp"

namespace nuvio::data::streams
{

namespace
{
StreamRepository* sharedRepository = nullptr;

std::string describe(const net::HttpResult& response)
{
    return response.error == net::HttpError::Http ? "HTTP " + std::to_string(response.status) : response.errorMessage;
}
} // namespace

StreamRepository& StreamRepository::shared()
{
    assert(sharedRepository && "StreamRepository::setShared was not called");
    return *sharedRepository;
}

void StreamRepository::setShared(StreamRepository* repository)
{
    sharedRepository = repository;
}

bool StreamRepository::hasShared()
{
    return sharedRepository != nullptr;
}

StreamRepository::Ticket StreamRepository::fetchAll(const std::string& type, const std::string& videoId,
    const std::vector<domain::Addon>& addons, GroupCallback onGroup, DoneCallback onDone)
{
    Ticket ticket = nextTicket++;
    auto selected = streamAddons(addons, type, videoId);

    auto run      = std::make_shared<Run>();
    run->ticket   = ticket;
    run->pending  = selected.size();
    run->onGroup  = std::move(onGroup);
    run->onDone   = std::move(onDone);
    for (const auto& addon : selected)
        run->summary.queried.push_back(addon.displayName);

    if (selected.empty())
    {
        run->onDone(run->summary);
        return ticket;
    }

    runs.emplace(ticket, run);
    for (size_t i = 0; i < selected.size(); i++)
        ask(run, i, selected[i], type, videoId);
    return ticket;
}

void StreamRepository::cancel(Ticket ticket)
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

void StreamRepository::ask(const std::shared_ptr<Run>& run, size_t order, const domain::Addon& addon,
    const std::string& type, const std::string& videoId)
{
    net::Request request;
    request.url           = buildStreamUrl(addon.baseUrl, type, videoId);
    request.permissiveTls = true;

    run->handles.push_back(config.http->sendPrepared(std::move(request),
        [this, run, order, addon, type, videoId](net::HttpResult response)
        {
            // on the worker: a large response is parsed here
            std::optional<std::vector<domain::Stream>> parsed;
            std::string error;
            if (!response.ok)
                error = describe(response);
            else if (!(parsed = parseStreamResponse(response.body, addon.displayName)))
                error = "invalid response";
            if (parsed)
                *parsed = dedup(std::move(*parsed));

            return std::function<void()>(
                [this, run, order, addon, type, videoId, parsed = std::move(parsed), error]() mutable
                {
                    if (run->cancelled)
                        return;
                    if (!parsed)
                    {
                        fail(run, addon, error);
                        return;
                    }
                    if (parsed->empty())
                    {
                        askMeta(run, order, addon, type, videoId);
                        return;
                    }
                    deliver(run, Group{order, addon.displayName, addon.baseUrl, std::move(*parsed)});
                });
        }));
}

void StreamRepository::askMeta(const std::shared_ptr<Run>& run, size_t order, const domain::Addon& addon,
    const std::string& type, const std::string& videoId)
{
    net::Request request;
    request.url           = meta::buildMetaUrl(addon.baseUrl, type, contentIdFromVideoId(videoId));
    request.permissiveTls = true;

    run->handles.push_back(config.http->sendPrepared(std::move(request),
        [this, run, order, addon, type, videoId](net::HttpResult response)
        {
            std::vector<domain::Stream> streams;
            std::string error = response.ok ? "no streams" : describe(response);
            if (response.ok)
                if (auto meta = meta::parseMeta(response.body, type, addon.baseUrl))
                    for (const auto& video : meta->videos)
                        if (video.id == videoId)
                        {
                            streams = dedup(parseInlineStreams(video.streamsJson, addon.displayName));
                            break;
                        }

            return std::function<void()>([this, run, order, addon, streams = std::move(streams), error]() mutable
                {
                    if (run->cancelled)
                        return;
                    if (streams.empty())
                        fail(run, addon, error);
                    else
                        deliver(run, Group{order, addon.displayName, addon.baseUrl, std::move(streams)});
                });
        }));
}

void StreamRepository::deliver(const std::shared_ptr<Run>& run, Group group)
{
    run->summary.groups++;
    if (run->onGroup)
        run->onGroup(group);
    finishOne(run);
}

void StreamRepository::fail(const std::shared_ptr<Run>& run, const domain::Addon& addon, const std::string& reason)
{
    core::log::info("Streams from " + addons::hostOf(addon.baseUrl) + " failed: " + reason);
    run->summary.failed.push_back(addon.displayName);
    finishOne(run);
}

void StreamRepository::finishOne(const std::shared_ptr<Run>& run)
{
    if (run->cancelled || --run->pending > 0)
        return;
    runs.erase(run->ticket);
    if (run->onDone)
        run->onDone(run->summary);
}

} // namespace nuvio::data::streams
