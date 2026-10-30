#include "ui/navigation.hpp"

#include <borealis.hpp>

#include "core/log.hpp"
#include "ui/catalog/see_all_activity.hpp"
#include "ui/details/details_activity.hpp"
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
    std::string line = "Play " + request.contentType + " " + request.videoId + " from " + request.addonName + ": " +
                       request.url + " (" + std::to_string(request.headers.size()) + " headers, " +
                       std::to_string(request.subtitles.size()) + " subtitles";
    if (request.bingeGroup)
        line += ", bingeGroup " + *request.bingeGroup;
    core::log::info(line + ")");
    for (const auto& [name, value] : request.headers)
        core::log::debug("  header " + name);
    brls::Application::notify("app/streams/player_soon"_i18n);
}

void openSeeAll(const domain::CatalogRow& row)
{
    brls::Application::pushActivity(new CatalogSeeAllActivity(row));
}

} // namespace nuvio::ui
