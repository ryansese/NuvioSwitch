#include "domain/stream_request.hpp"

namespace nuvio::domain
{

namespace
{
StreamRequest base(const Meta& meta)
{
    StreamRequest request;
    request.contentType = meta.type;
    request.contentId   = meta.id;
    request.videoId     = meta.id;
    request.title       = meta.name;
    request.poster      = meta.poster;
    request.backdrop    = meta.background;
    request.logo        = meta.logo;
    return request;
}
} // namespace

StreamRequest streamRequestForEpisode(const Meta& meta, const Video& episode)
{
    StreamRequest request = base(meta);
    request.videoId       = episode.id;
    request.season        = episode.season;
    request.episode       = episode.episode;
    if (!episode.title.empty())
        request.episodeTitle = episode.title;
    return request;
}

StreamRequest streamRequestForPlay(const Meta& meta, const Season* selected)
{
    std::string videoId = playVideoId(meta, selected);
    if (selected && !selected->episodes.empty() && selected->episodes.front().id == videoId)
        return streamRequestForEpisode(meta, selected->episodes.front());

    StreamRequest request = base(meta);
    request.videoId       = videoId;
    return request;
}

} // namespace nuvio::domain
