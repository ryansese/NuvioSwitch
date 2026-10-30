#include "ui/details/episode_cell.hpp"

#include "core/image/image_loader.hpp"
#include "domain/meta_format.hpp"

using nuvio::core::image::ImageLoader;

// roughly two lines at the cell's width
static constexpr size_t OVERVIEW_LIMIT = 130;

EpisodeCell::EpisodeCell()
{
    this->inflateFromXMLRes("xml/cells/episode_cell.xml");
}

EpisodeCell::~EpisodeCell()
{
    if (ImageLoader::hasShared())
        ImageLoader::shared().cancel(thumbnail);
}

void EpisodeCell::setEpisode(const nuvio::domain::Video& episode, const std::string& fallbackPoster)
{
    title->setText(nuvio::domain::episodeHeading(episode));
    date->setText(nuvio::domain::releaseDate(episode.released));
    date->setVisibility(date->getFullText().empty() ? brls::Visibility::GONE : brls::Visibility::VISIBLE);
    overview->setText(nuvio::domain::shorten(episode.overview.value_or(""), OVERVIEW_LIMIT));

    const std::string& url = episode.thumbnail && !episode.thumbnail->empty() ? *episode.thumbnail : fallbackPoster;
    ImageLoader::shared().load(thumbnail, url);
}

void EpisodeCell::prepareForReuse()
{
    ImageLoader::shared().cancel(thumbnail);
}

brls::RecyclerCell* EpisodeCell::create()
{
    return new EpisodeCell();
}
