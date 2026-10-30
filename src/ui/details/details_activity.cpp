#include "ui/details/details_activity.hpp"

#include <algorithm>

#include "core/image/image_loader.hpp"
#include "data/addons/addon_repository.hpp"
#include "domain/meta_format.hpp"
#include "domain/stream_request.hpp"
#include "ui/common/scrim.hpp"
#include "ui/details/episode_cell.hpp"
#include "ui/navigation.hpp"

using namespace brls::literals; // for _i18n
using nuvio::core::image::ImageLoader;
using nuvio::data::addons::AddonRepository;
using nuvio::data::meta::MetaRepository;
using namespace nuvio::domain;

static const char* const TRANSPARENT = "img/transparent.png";

// The episodes of the selected season, one cell each.
class DetailsActivity::Episodes : public brls::RecyclerDataSource
{
  public:
    explicit Episodes(DetailsActivity* owner) : owner(owner) { }

    int numberOfRows(brls::RecyclerFrame*, int) override
    {
        if (owner->selectedSeason >= owner->seasons.size())
            return 0;
        return owner->seasons[owner->selectedSeason].episodes.size();
    }

    brls::RecyclerCell* cellForRow(brls::RecyclerFrame* recycler, brls::IndexPath index) override
    {
        auto* cell = static_cast<EpisodeCell*>(recycler->dequeueReusableCell("Episode"));
        cell->setEpisode(owner->seasons[owner->selectedSeason].episodes[index.row], owner->meta.poster.value_or(""));
        return cell;
    }

    float heightForRow(brls::RecyclerFrame*, brls::IndexPath) override { return EpisodeCell::HEIGHT; }

    void didSelectRowAt(brls::RecyclerFrame*, brls::IndexPath index) override
    {
        owner->playEpisode(owner->seasons[owner->selectedSeason].episodes[index.row]);
    }

  private:
    DetailsActivity* owner;
};

DetailsActivity::DetailsActivity(const MetaPreview& item, const std::string& sourceAddonName)
    : item(item)
    , sourceAddonName(sourceAddonName)
{
}

DetailsActivity::~DetailsActivity()
{
    if (ticket && MetaRepository::hasShared())
        MetaRepository::shared().cancel(ticket);
    if (ImageLoader::hasShared())
    {
        ImageLoader::shared().cancel(backdrop);
        ImageLoader::shared().cancel(logo);
    }
}

brls::View* DetailsActivity::createContentView()
{
    auto* content = static_cast<brls::Box*>(brls::View::createFromXMLResource("activity/details.xml"));

    backdrop     = static_cast<brls::Image*>(content->getView("details/backdrop"));
    status       = static_cast<brls::Box*>(content->getView("details/status"));
    spinner      = static_cast<brls::ProgressSpinner*>(content->getView("details/spinner"));
    message      = static_cast<brls::Label*>(content->getView("details/message"));
    retry        = static_cast<brls::Button*>(content->getView("details/retry"));
    heroScroll   = static_cast<brls::ScrollingFrame*>(content->getView("details/heroScroll"));
    logo         = static_cast<brls::Image*>(content->getView("details/logo"));
    title        = static_cast<brls::Label*>(content->getView("details/title"));
    info         = static_cast<brls::Label*>(content->getView("details/info"));
    genres       = static_cast<brls::Label*>(content->getView("details/genres"));
    playButton   = static_cast<brls::Button*>(content->getView("details/play"));
    description  = static_cast<brls::Label*>(content->getView("details/description"));
    director     = static_cast<brls::Label*>(content->getView("details/director"));
    cast         = static_cast<brls::Label*>(content->getView("details/cast"));
    episodes     = static_cast<brls::Box*>(content->getView("details/episodes"));
    seasonBox    = static_cast<brls::Box*>(content->getView("details/seasons"));
    seasonScroll = static_cast<brls::Box*>(content->getView("details/seasonScroll"));
    recycler     = static_cast<brls::RecyclerFrame*>(content->getView("details/recycler"));

    // the fade goes over the backdrop and under everything else
    content->addView(new Scrim(), 1);

    retry->registerClickAction([this](brls::View*)
        {
            load();
            return true;
        });
    playButton->registerClickAction([this](brls::View*)
        {
            play();
            return true;
        });

    recycler->estimatedRowHeight = EpisodeCell::HEIGHT;
    recycler->registerCell("Episode", EpisodeCell::create);
    recycler->setDataSource(new Episodes(this));

    auto* frame = new brls::AppletFrame(content);
    frame->setHeaderVisibility(brls::Visibility::GONE);
    return frame;
}

void DetailsActivity::onContentAvailable()
{
    load();
}

void DetailsActivity::showLoading()
{
    status->setVisibility(brls::Visibility::VISIBLE);
    spinner->setVisibility(brls::Visibility::VISIBLE);
    spinner->animate(true);
    message->setText(item.name);
    retry->setVisibility(brls::Visibility::GONE);
}

void DetailsActivity::showError()
{
    spinner->animate(false);
    spinner->setVisibility(brls::Visibility::GONE);
    message->setText("app/details/error"_i18n);
    retry->setVisibility(brls::Visibility::VISIBLE);
    brls::Application::giveFocus(retry);
}

void DetailsActivity::load()
{
    showLoading();
    ImageLoader::shared().load(backdrop, item.background.value_or(item.poster.value_or("")), TRANSPARENT);
    if (ticket)
        MetaRepository::shared().cancel(ticket);

    // a cached result calls back before lookup returns, so the ticket is cleared first
    ticket = 0;
    auto self = this;
    ticket = MetaRepository::shared().lookup(item.type, item.id, item.sourceAddonBaseUrl,
        AddonRepository::shared().addons(),
        [self](MetaRepository::Result result)
        {
            self->ticket = 0;
            if (result.ok)
                self->showMeta(result.meta);
            else
                self->showError();
        });
}

void DetailsActivity::showMeta(const Meta& value)
{
    meta   = value;
    loaded = true;
    status->setVisibility(brls::Visibility::GONE);
    spinner->animate(false);
    heroScroll->setVisibility(brls::Visibility::VISIBLE);

    // the backdrop falls back to the poster
    ImageLoader::shared().load(backdrop, meta.background.value_or(meta.poster.value_or("")), TRANSPARENT);

    bool hasLogo = meta.logo && !meta.logo->empty();
    logo->setVisibility(hasLogo ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
    title->setVisibility(hasLogo ? brls::Visibility::GONE : brls::Visibility::VISIBLE);
    if (hasLogo)
        ImageLoader::shared().load(logo, *meta.logo, TRANSPARENT);
    title->setText(meta.name);

    auto setText = [](brls::Label* label, const std::string& text)
    {
        label->setText(text);
        label->setVisibility(text.empty() ? brls::Visibility::GONE : brls::Visibility::VISIBLE);
    };
    setText(info, infoLine(meta));
    setText(genres, joinNames(meta.genres, 4));
    setText(description, meta.description.value_or(""));
    std::string directors = joinNames(meta.director, 3);
    setText(director, directors.empty() ? "" : brls::getStr("app/details/director", directors));
    std::string actors = joinNames(meta.cast, 6);
    setText(cast, actors.empty() ? "" : brls::getStr("app/details/cast", actors));

    seasons = groupSeasons(meta.videos);
    // a title with no videos is a movie; a series with none still plays by its meta ID
    bool hasEpisodes = !seasons.empty();
    episodes->setVisibility(hasEpisodes ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
    heroScroll->setWidth(hasEpisodes ? 500 : brls::View::AUTO);
    heroScroll->setGrow(hasEpisodes ? 0 : 1);

    seasonBox->clearViews();
    seasonButtons.clear();
    tabOffset = 0;
    seasonBox->setTranslationX(0);
    // a single virtual season has no tabs
    bool tabs = seasons.size() > 1 || (seasons.size() == 1 && seasons[0].number);
    seasonScroll->setVisibility(tabs ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
    if (tabs)
    {
        for (size_t i = 0; i < seasons.size(); i++)
        {
            auto* button = new brls::Button();
            button->setText(*seasons[i].number == 0 ? "app/details/specials"_i18n
                                                    : brls::getStr("app/details/season", *seasons[i].number));
            button->setMarginRight(10);
            button->setShrink(0); // a long row must not squeeze the labels away
            button->registerClickAction([this, i](brls::View*)
                {
                    selectSeason(i);
                    return true;
                });
            button->getFocusEvent()->subscribe([this, i](brls::View*) { revealSeason(i); });
            seasonBox->addView(button);
            seasonButtons.push_back(button);
        }
    }

    if (hasEpisodes)
        selectSeason(initialSeasonIndex(seasons, meta.defaultVideoId));
    brls::Application::giveFocus(playButton);
}

void DetailsActivity::selectSeason(size_t index)
{
    selectedSeason = index;
    for (size_t i = 0; i < seasonButtons.size(); i++)
        seasonButtons[i]->setStyle(i == index ? &brls::BUTTONSTYLE_PRIMARY : &brls::BUTTONSTYLE_DEFAULT);
    recycler->reloadData();
    // the tab may be off screen (a default season far along); wait for layout
    brls::sync([this, index]() { revealSeason(index); });
}

// Slides the season tabs so that tab index is fully inside the strip.
void DetailsActivity::revealSeason(size_t index)
{
    if (index >= seasonButtons.size())
        return;
    constexpr float MARGIN = 12;
    float left             = seasonButtons[index]->getLocalX();
    float right            = left + seasonButtons[index]->getWidth();
    float visible          = seasonScroll->getWidth();
    if (visible <= 0)
        return;
    if (left - tabOffset < MARGIN)
        tabOffset = std::max(0.0f, left - MARGIN);
    else if (right - tabOffset > visible - MARGIN)
        tabOffset = right - visible + MARGIN;
    seasonBox->setTranslationX(-tabOffset);
}

void DetailsActivity::play()
{
    if (!loaded)
        return;
    const Season* selected = selectedSeason < seasons.size() ? &seasons[selectedSeason] : nullptr;
    nuvio::ui::openStreams(streamRequestForPlay(meta, selected));
}

void DetailsActivity::playEpisode(const Video& episode)
{
    nuvio::ui::openStreams(streamRequestForEpisode(meta, episode));
}
