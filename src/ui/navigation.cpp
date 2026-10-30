#include "ui/navigation.hpp"

#include <borealis.hpp>

#include "core/log.hpp"
#include "ui/catalog/see_all_activity.hpp"
#include "ui/details/details_activity.hpp"

using namespace brls::literals; // for _i18n

namespace nuvio::ui
{

void openDetails(const domain::MetaPreview& item, const std::string& sourceAddonName)
{
    brls::Application::pushActivity(new DetailsActivity(item, sourceAddonName));
}

void openStreams(const domain::StreamRequest& request)
{
    core::log::info("Streams requested for " + request.contentType + " " + request.videoId);
    brls::Application::notify("app/details/streams_soon"_i18n);
}

void openSeeAll(const domain::CatalogRow& row)
{
    brls::Application::pushActivity(new CatalogSeeAllActivity(row));
}

} // namespace nuvio::ui
