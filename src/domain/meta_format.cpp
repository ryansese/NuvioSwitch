#include "domain/meta_format.hpp"

#include <cctype>
#include <cstdio>

namespace nuvio::domain
{

std::string shorten(const std::string& text, size_t limit)
{
    if (text.size() <= limit)
        return text;
    size_t end = limit;
    while (end > 0 && (static_cast<unsigned char>(text[end]) & 0xC0) == 0x80)
        end--;
    while (end > 0 && text[end - 1] == ' ')
        end--;
    return text.substr(0, end) + "…";
}

std::string episodeHeading(const Video& episode)
{
    std::string code;
    if (episode.season)
        code += "S" + std::to_string(*episode.season);
    if (episode.episode)
        code += "E" + std::to_string(*episode.episode);

    if (code.empty())
        return episode.title;
    if (episode.title.empty())
        return code;
    return code + " · " + episode.title;
}

std::string releaseDate(const std::optional<std::string>& released)
{
    if (!released)
        return "";
    const std::string& s = *released;
    bool isoDate         = s.size() >= 10 && s[4] == '-' && s[7] == '-';
    for (size_t i : {0, 1, 2, 3, 5, 6, 8, 9})
        isoDate = isoDate && s.size() > i && std::isdigit(static_cast<unsigned char>(s[i]));
    return isoDate ? s.substr(0, 10) : s;
}

std::string infoLine(const Meta& meta)
{
    std::string line;
    auto add = [&line](const std::string& part)
    {
        if (part.empty())
            return;
        line += line.empty() ? part : "  ·  " + part;
    };
    add(meta.releaseInfo.value_or(""));
    add(meta.runtime.value_or(""));
    if (meta.imdbRating)
    {
        char rating[32];
        std::snprintf(rating, sizeof(rating), "IMDb %.1f", *meta.imdbRating);
        add(rating);
    }
    return line;
}

std::string joinNames(const std::vector<std::string>& names, size_t limit)
{
    std::string out;
    for (size_t i = 0; i < names.size() && i < limit; i++)
        out += (i ? ", " : "") + names[i];
    return out;
}

} // namespace nuvio::domain
