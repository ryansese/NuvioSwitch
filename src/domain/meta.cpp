#include "domain/meta.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <map>

namespace nuvio::domain
{

namespace
{
bool hasAnySeason(const std::vector<Video>& videos)
{
    return std::any_of(videos.begin(), videos.end(), [](const Video& v) { return v.season.has_value(); });
}

bool isBlank(const std::string& s)
{
    return std::all_of(s.begin(), s.end(), [](unsigned char c) { return std::isspace(c); });
}
} // namespace

std::vector<Season> groupSeasons(const std::vector<Video>& videos)
{
    std::vector<Season> out;
    if (videos.empty())
        return out;

    if (!hasAnySeason(videos))
    {
        Season virtualSeason;
        virtualSeason.episodes = videos;
        out.push_back(std::move(virtualSeason));
        return out;
    }

    std::map<int, Season> bySeason;
    for (const auto& video : videos)
    {
        int number    = video.season.value_or(0);
        Season& season = bySeason[number];
        season.number  = number;
        season.episodes.push_back(video);
    }
    for (auto& [number, season] : bySeason)
    {
        // stable: episodes without a number keep their place after the numbered ones
        std::stable_sort(season.episodes.begin(), season.episodes.end(), [](const Video& a, const Video& b)
            { return a.episode.value_or(INT32_MAX) < b.episode.value_or(INT32_MAX); });
        out.push_back(std::move(season));
    }
    // specials (season 0) come last
    std::stable_partition(out.begin(), out.end(), [](const Season& s) { return s.number != 0; });
    return out;
}

size_t initialSeasonIndex(const std::vector<Season>& seasons, const std::optional<std::string>& defaultVideoId)
{
    if (seasons.empty())
        return 0;

    if (defaultVideoId && !isBlank(*defaultVideoId))
    {
        for (size_t i = 0; i < seasons.size(); i++)
            for (const auto& episode : seasons[i].episodes)
                if (episode.id == *defaultVideoId && episode.available != false)
                    return i;
    }
    for (size_t i = 0; i < seasons.size(); i++)
        if (seasons[i].number && *seasons[i].number > 0)
            return i;
    return 0;
}

std::string playVideoId(const Meta& meta, const Season* selected)
{
    bool isSeries = !meta.videos.empty() && meta.type != "movie";
    if (isSeries && selected && !selected->episodes.empty())
        return selected->episodes.front().id;
    if (meta.defaultVideoId && !isBlank(*meta.defaultVideoId))
        return *meta.defaultVideoId;
    return meta.id;
}

} // namespace nuvio::domain
