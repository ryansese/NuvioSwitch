#include "tabs/debug_poster_row.hpp"

#include "core/image/image_loader.hpp"

using nuvio::core::image::ImageLoader;

static const char* const POSTER_URLS[] = {
    "https://images.metahub.space/poster/medium/tt0111161/img",
    "https://images.metahub.space/poster/medium/tt0068646/img",
    "https://images.metahub.space/poster/medium/tt0468569/img",
    "https://images.metahub.space/poster/medium/tt0071562/img",
    "https://images.metahub.space/poster/medium/tt0050083/img",
};
static constexpr size_t POSTER_COUNT = sizeof(POSTER_URLS) / sizeof(POSTER_URLS[0]);

DebugPosterRow::DebugPosterRow()
{
    this->setAxis(brls::Axis::ROW);
    this->setMarginTop(32);

    for (size_t i = 0; i < POSTER_COUNT; i++)
    {
        auto* cell = new brls::Box();
        cell->setFocusable(true);
        cell->setCornerRadius(8);
        cell->setMarginLeft(i == 0 ? 0 : 16);
        cell->registerClickAction([this](brls::View*)
            {
                offset = (offset + 1) % POSTER_COUNT;
                loadAll();
                return true;
            });

        auto* image = new brls::Image();
        image->setDimensions(126, 189);
        image->setScalingType(brls::ImageScalingType::FILL);
        image->setCornerRadius(8);
        cell->addView(image);

        this->addView(cell);
        posters.push_back(image);
    }

    loadAll();
}

DebugPosterRow::~DebugPosterRow()
{
    if (!ImageLoader::hasShared())
        return;
    for (auto* image : posters)
        ImageLoader::shared().cancel(image);
}

void DebugPosterRow::loadAll()
{
    for (size_t i = 0; i < posters.size(); i++)
        ImageLoader::shared().load(posters[i], POSTER_URLS[(i + offset) % POSTER_COUNT]);
}
