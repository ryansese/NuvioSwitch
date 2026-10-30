#include "ui/navigation.hpp"

#include <borealis.hpp>

#include "core/log.hpp"
#include "ui/catalog/see_all_activity.hpp"
#include "ui/details/details_activity.hpp"
#include "ui/player/player_activity.hpp"
#include "ui/streams/stream_activity.hpp"

using namespace brls::literals; // for _i18n

namespace nuvio::ui
{

void openDetails(const domain::MetaPreview& item, const std::string& sourceAddonName)
{
    brls::Application::pushActivity(new DetailsActivity(item, sourceAddonName));
}

void openStreams(const domain::StreamRequest& request)
{
    brls::Application::pushActivity(new StreamActivity(request));
}

void openPlayer(const domain::PlaybackRequest& request)
{
    brls::Application::pushActivity(new PlayerActivity(request));
}

void openSeeAll(const domain::CatalogRow& row)
{
    brls::Application::pushActivity(new CatalogSeeAllActivity(row));
}

} // namespace nuvio::ui
