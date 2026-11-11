#include "ui/details/details_activity.hpp"

#include <algorithm>

#include "core/image/image_loader.hpp"
#include "data/addons/addon_repository.hpp"
#include "data/library/library_repository.hpp"
#include "data/progress/continue_watching.hpp"
#include "data/progress/progress_repository.hpp"
#include "domain/meta_format.hpp"
#include "domain/stream_request.hpp"
#include "ui/common/options_menu.hpp"
#include "ui/common/scrim.hpp"
#include "ui/common/today.hpp"
#include "ui/details/episode_cell.hpp"
#include "ui/navigation.hpp"

using namespace brls::literals; // for _i18n
using nuvio::core::image::ImageLoader;
using nuvio::data::addons::AddonRepository;
using nuvio::data::library::LibraryRepository;
using nuvio::data::progress::ProgressRepository;
namespace progress = nuvio::data::progress;
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
        const Video& episode = owner->seasons[owner->selectedSeason].episodes[index.row];
        cell->setEpisode(episode, owner->meta.poster.value_or(""));
        owner->applyWatchState(cell, episode);
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
    if (ProgressRepository::hasShared())
        ProgressRepository::shared().unsubscribe(progressSubscription);
    if (LibraryRepository::hasShared())
        LibraryRepository::shared().unsubscribe(librarySubscription);
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
    libraryButton = static_cast<brls::Button*>(content->getView("details/library"));
    watchedButton = static_cast<brls::Button*>(content->getView("details/watched"));
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

    libraryButton->registerClickAction([this](brls::View*)
        {
            toggleLibrary();
            return true;
        });
    watchedButton->registerClickAction([this](brls::View*)
        {
            toggleWatched();
            return true;
        });

    recycler->estimatedRowHeight = EpisodeCell::HEIGHT;
    recycler->registerCell("Episode", [this]()
        {
            auto* cell = static_cast<EpisodeCell*>(EpisodeCell::create());
            cell->registerAction("app/details/options"_i18n, brls::ControllerButton::BUTTON_X, [this, cell](brls::View*)
                {
                    episodeOptions(cell->getIndexPath().row);
                    return true;
                });
            episodeCells.push_back(cell);
            return cell;
        });
    recycler->setDataSource(new Episodes(this));

    // a save from the player, or a change made here, shows at once
    progressSubscription = ProgressRepository::shared().subscribe([this]
        {
            updateActions();
            refreshEpisodes();
        });
    librarySubscription = LibraryRepository::shared().subscribe([this] { updateActions(); });

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
    updateActions();
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
    // a series plays its in-progress or next episode; the stream picker resumes in-progress ones
    if (target.episode)
        return playEpisode(*target.episode);
    const Season* selected = selectedSeason < seasons.size() ? &seasons[selectedSeason] : nullptr;
    nuvio::ui::openStreams(streamRequestForPlay(meta, selected));
}

// The play button's label and the library and watched toggles, from the repositories.
void DetailsActivity::updateActions()
{
    if (!loaded)
        return;
    auto& repo = ProgressRepository::shared();
    target     = progress::playTarget(meta, repo.entries(), repo.watched(), nuvio::ui::today());

    if (target.resume)
        playButton->setText(brls::getStr("app/details/resume", progress::shortDuration(progress::remainingMs(*target.resume))));
    else if (target.episode && target.episode->season && target.episode->episode)
        playButton->setText(brls::getStr("app/details/play_episode", *target.episode->season, *target.episode->episode));
    else
        playButton->setText("app/details/play"_i18n);

    libraryButton->setText(LibraryRepository::shared().contains(meta.id, meta.type) ? "app/details/remove_library"_i18n
                                                                                     : "app/details/add_library"_i18n);

    // a series is marked episode by episode
    bool movie = seasons.empty();
    watchedButton->setVisibility(movie ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
    if (movie)
        watchedButton->setText(repo.isWatched(meta.id, std::nullopt, std::nullopt) ? "app/details/mark_unwatched"_i18n
                                                                                   : "app/details/mark_watched"_i18n);
}

void DetailsActivity::applyWatchState(EpisodeCell* cell, const Video& episode)
{
    auto& repo   = ProgressRepository::shared();
    bool watched = repo.isWatched(meta.id, episode.season, episode.episode);
    float fraction = 0;
    if (auto entry = repo.get(progress::progressKey(meta.id, episode.season, episode.episode)))
        if (progress::isInProgress(*entry))
            fraction = progress::progressPercentage(*entry);
    cell->setWatchState(watched, fraction);
}

// Updates the episode cells on screen without reloading the list, which would move focus.
void DetailsActivity::refreshEpisodes()
{
    if (selectedSeason >= seasons.size())
        return;
    const auto& list = seasons[selectedSeason].episodes;
    for (auto* cell : episodeCells)
    {
        size_t row = cell->getIndexPath().row;
        if (cell->getParent() && row < list.size())
            applyWatchState(cell, list[row]);
    }
}

void DetailsActivity::toggleLibrary()
{
    if (!loaded)
        return;
    auto& library = LibraryRepository::shared();
    if (library.contains(meta.id, meta.type))
        library.remove(meta.id, meta.type);
    else
        library.add(nuvio::data::library::itemFromMeta(meta, library.now()));
}

void DetailsActivity::toggleWatched()
{
    if (!loaded)
        return;
    auto& repo = ProgressRepository::shared();
    if (repo.isWatched(meta.id, std::nullopt, std::nullopt))
        repo.markUnwatched(meta.id, std::nullopt, std::nullopt);
    else
        repo.markWatched({meta.id, meta.type, meta.name, std::nullopt, std::nullopt, repo.now(), meta.poster});
}

void DetailsActivity::episodeOptions(size_t row)
{
    if (selectedSeason >= seasons.size() || row >= seasons[selectedSeason].episodes.size())
        return;
    Video episode = seasons[selectedSeason].episodes[row];
    bool watched  = ProgressRepository::shared().isWatched(meta.id, episode.season, episode.episode);
    std::vector<std::string> options{watched ? "app/details/mark_unwatched"_i18n : "app/details/mark_watched"_i18n};
    nuvio::ui::showOptions(nuvio::domain::episodeHeading(episode), options, 0,
        [this, episode, watched](int)
        {
            auto& repo = ProgressRepository::shared();
            if (watched)
                repo.markUnwatched(meta.id, episode.season, episode.episode);
            else
                repo.markWatched({meta.id, meta.type, meta.name, episode.season, episode.episode, repo.now(), meta.poster});
        });
}

void DetailsActivity::playEpisode(const Video& episode)
{
    nuvio::ui::openStreams(streamRequestForEpisode(meta, episode));
}
