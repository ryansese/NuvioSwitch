#include "ui/player/player_activity.hpp"

#include <algorithm>
#include <cmath>

#include "core/log.hpp"
#include "data/addons/addon_repository.hpp"
#include "data/addons/addon_url.hpp"
#include "domain/meta.hpp"
#include "domain/meta_format.hpp"
#include "ui/player/player_view.hpp"
#include "ui/player/seek_bar.hpp"

using namespace brls::literals; // for _i18n
using nuvio::data::addons::AddonRepository;
using nuvio::data::player::SubtitleRepository;
using nuvio::data::player::Track;
using namespace nuvio::ui::player;

namespace
{
// presses further apart than this are separate: the next one starts the step over
constexpr auto RELEASE_GAP = std::chrono::milliseconds(250);
// the controls hide after this long without input while playing
constexpr long HIDE_AFTER_MS = 4000;
// ZL and ZR
constexpr double SHOULDER_SEEK_SECONDS = 10;

std::string upper(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::toupper(c); });
    return s;
}

// "English · AC3 · 5.1"-style line of a track
std::string describe(const Track& track, const std::string& fallback)
{
    std::string text = track.title;
    auto add         = [&text](const std::string& part)
    {
        if (!part.empty())
            text += (text.empty() ? "" : " · ") + part;
    };
    if (!track.lang.empty())
        add(upper(track.lang));
    add(track.codec);
    if (track.type == "audio" && track.channels > 0)
        add(track.channels == 6 ? "5.1" : track.channels == 8 ? "7.1" : track.channels == 2 ? "2.0" : std::to_string(track.channels) + "ch");
    return text.empty() ? fallback : text;
}
} // namespace

PlayerActivity::PlayerActivity(const nuvio::domain::PlaybackRequest& request) : request(request) { }

PlayerActivity::~PlayerActivity()
{
    teardown();
}

// Stops everything that talks to mpv or the GL context while both are still alive.
void PlayerActivity::teardown()
{
    closing = true;
    if (subtitleTicket && SubtitleRepository::hasShared())
        SubtitleRepository::shared().cancel(subtitleTicket);
    subtitleTicket = 0;
    if (video)
        video->releaseGl();
    mpv.reset();
}

brls::View* PlayerActivity::createContentView()
{
    root = static_cast<brls::Box*>(brls::View::createFromXMLResource("activity/player.xml"));

    loading        = static_cast<brls::Box*>(root->getView("player/loading"));
    spinner        = static_cast<brls::ProgressSpinner*>(root->getView("player/spinner"));
    pausedLabel    = static_cast<brls::Label*>(root->getView("player/pausedLabel"));
    controls       = static_cast<brls::Box*>(root->getView("player/controls"));
    title          = static_cast<brls::Label*>(root->getView("player/title"));
    episode        = static_cast<brls::Label*>(root->getView("player/episode"));
    elapsed        = static_cast<brls::Label*>(root->getView("player/elapsed"));
    remaining      = static_cast<brls::Label*>(root->getView("player/remaining"));
    playButton     = static_cast<brls::Button*>(root->getView("player/play"));
    subtitlesButton = static_cast<brls::Button*>(root->getView("player/subtitles"));
    audioButton    = static_cast<brls::Button*>(root->getView("player/audio"));
    sourcesButton  = static_cast<brls::Button*>(root->getView("player/sources"));
    panelBox       = static_cast<brls::Box*>(root->getView("player/panel"));
    panelTitle     = static_cast<brls::Label*>(root->getView("player/panelTitle"));
    panelList      = static_cast<brls::Box*>(root->getView("player/panelList"));
    errorBox       = static_cast<brls::Box*>(root->getView("player/error"));
    errorReason    = static_cast<brls::Label*>(root->getView("player/errorReason"));
    errorOther     = static_cast<brls::Button*>(root->getView("player/errorOther"));
    errorBack      = static_cast<brls::Button*>(root->getView("player/errorBack"));
    auto* seekHolder = static_cast<brls::Box*>(root->getView("player/seekHolder"));

    MpvCore::Events events;
    events.onPosition = [this](double seconds)
    {
        position = seconds;
        updateTimes();
    };
    events.onDuration = [this](double seconds)
    {
        duration = seconds;
        updateTimes();
    };
    events.onBuffered = [this](double seconds)
    {
        buffered = seconds;
    };
    events.onPaused = [this](bool value) { updatePaused(value); };
    events.onWaitingForCache = [this](bool value)
    {
        waiting = value;
        updateSpinner();
    };
    events.onTracks = [this](std::vector<Track> list)
    {
        tracks = std::move(list);
        if (panel != Panel::None)
            fillPanel();
    };
    events.onPlaybackStarted = [this]()
    {
        started = true;
        nuvio::core::log::info("Playback started at " + nuvio::data::player::formatTime(position));
        updateSpinner();
    };
    // keep-open leaves the last frame up; the controls show so the user can pick something else
    events.onEnded = [this]()
    {
        if (!closing)
            showControls();
    };
    events.onError = [this](std::string reason) { showError(reason); };
    mpv = std::make_unique<MpvCore>(std::move(events));

    video = new PlayerView(*mpv);
    video->onContextReady = [this](bool ok)
    {
        if (!ok || !mpv || !mpv->ok())
            return showError("app/player/no_video"_i18n);
        start();
    };
    root->addView(video, 0);

    seekBar = new SeekBar();
    seekHolder->addView(seekBar);
    seekBar->registerAction("", brls::ControllerButton::BUTTON_NAV_LEFT, [this](brls::View*) { seek(false); return true; }, true, true);
    seekBar->registerAction("", brls::ControllerButton::BUTTON_NAV_RIGHT, [this](brls::View*) { seek(true); return true; }, true, true);
    seekBar->registerAction("", brls::ControllerButton::BUTTON_A, [this](brls::View*) { togglePause(); return true; }, true);

    title->setText(request.title);
    nuvio::domain::Video heading;
    heading.season  = request.season;
    heading.episode = request.episode;
    heading.title   = request.episodeTitle.value_or("");
    std::string line = request.season || request.episode || request.episodeTitle ? nuvio::domain::episodeHeading(heading) : "";
    episode->setText(line);
    episode->setVisibility(line.empty() ? brls::Visibility::GONE : brls::Visibility::VISIBLE);

    spinner->animate(true);
    updateTimes();

    playButton->registerClickAction([this](brls::View*)
        {
            togglePause();
            scheduleHide();
            return true;
        });
    subtitlesButton->registerClickAction([this](brls::View* v)
        {
            panelOpener = v;
            openPanel(Panel::Subtitles);
            return true;
        });
    audioButton->registerClickAction([this](brls::View* v)
        {
            panelOpener = v;
            openPanel(Panel::Audio);
            return true;
        });
    sourcesButton->registerClickAction([this](brls::View*)
        {
            closeToPicker();
            return true;
        });
    errorOther->registerClickAction([this](brls::View*)
        {
            closeToPicker();
            return true;
        });
    errorBack->registerClickAction([this](brls::View*)
        {
            closeToDetails();
            return true;
        });

    registerInput(root);
    return root;
}

void PlayerActivity::onContentAvailable()
{
    brls::Application::giveFocus(root);
}

// Loads the stream, and starts collecting subtitles so the panel is ready.
void PlayerActivity::start()
{
    mpv->load(request.url, request.headers, request.startPositionMs);
    nuvio::core::log::info("Playing " + nuvio::data::addons::hostOf(request.url));

    subtitleTicket = SubtitleRepository::shared().fetchAll(request, AddonRepository::shared().addons(),
        [this](const std::vector<nuvio::domain::Subtitle>& all)
        {
            subtitles = all;
            nuvio::core::log::info("Subtitles available: " + std::to_string(all.size()));
            if (panel == Panel::Subtitles)
                fillPanel();
        },
        [this]() { subtitleTicket = 0; });
}

void PlayerActivity::registerInput(brls::Box* surface)
{
    using brls::ControllerButton;
    // While the controls are up the d-pad moves focus, so these decline and the focus system takes over.
    surface->registerAction("", ControllerButton::BUTTON_NAV_LEFT, [this](brls::View*)
        {
            if (controlsVisible() || panel != Panel::None)
                return false;
            seek(false);
            return true;
        }, true, true);
    surface->registerAction("", ControllerButton::BUTTON_NAV_RIGHT, [this](brls::View*)
        {
            if (controlsVisible() || panel != Panel::None)
                return false;
            seek(true);
            return true;
        }, true, true);
    for (auto button : {ControllerButton::BUTTON_NAV_UP, ControllerButton::BUTTON_NAV_DOWN})
        surface->registerAction("", button, [this](brls::View*)
            {
                if (controlsVisible() || panel != Panel::None)
                    return false;
                showControls();
                return true;
            }, true);
    surface->registerAction("", ControllerButton::BUTTON_X, [this](brls::View*)
        {
            if (!controlsVisible())
                showControls();
            return true;
        }, true);
    surface->registerAction("", ControllerButton::BUTTON_A, [this](brls::View*)
        {
            if (controlsVisible() || panel != Panel::None)
                return false;
            togglePause();
            return true;
        }, true);
    surface->registerAction("", ControllerButton::BUTTON_LT, [this](brls::View*)
        {
            seekBySeconds(-SHOULDER_SEEK_SECONDS);
            return true;
        }, true, true);
    surface->registerAction("", ControllerButton::BUTTON_RT, [this](brls::View*)
        {
            seekBySeconds(SHOULDER_SEEK_SECONDS);
            return true;
        }, true, true);
    surface->registerAction("", ControllerButton::BUTTON_B, [this](brls::View*)
        {
            back();
            return true;
        }, true);
}

// B: a panel closes first, then the controls, then the player.
void PlayerActivity::back()
{
    if (failed)
        return closeToPicker();
    if (panel != Panel::None)
        return closePanel();
    if (controlsVisible())
        return hideControls();
    closeToPicker();
}

void PlayerActivity::showControls(bool focusSeekBar)
{
    controls->setVisibility(brls::Visibility::VISIBLE);
    updateTimes();
    if (focusSeekBar)
        brls::Application::giveFocus(seekBar);
    scheduleHide();
}

void PlayerActivity::hideControls()
{
    hideToken++;
    controls->setVisibility(brls::Visibility::GONE);
    brls::Application::giveFocus(root);
}

// Hides the controls after HIDE_AFTER_MS without input, while playing; any later input starts it over.
void PlayerActivity::scheduleHide()
{
    int token = ++hideToken;
    brls::delay(HIDE_AFTER_MS, [this, token]()
        {
            if (closing || token != hideToken || !controlsVisible() || panel != Panel::None || paused)
                return;
            hideControls();
        });
}

void PlayerActivity::seek(bool forward)
{
    auto now = std::chrono::steady_clock::now();
    seekRepeat = now - lastSeek < RELEASE_GAP ? seekRepeat + 1 : 0;
    lastSeek   = now;
    double step = nuvio::data::player::seekStepMs(seekRepeat) / 1000.0;
    seekBySeconds(forward ? step : -step);
}

void PlayerActivity::seekBySeconds(double seconds)
{
    if (!mpv || failed)
        return;
    mpv->seekRelative(seconds);
    // show where we are going until mpv reports it
    position = std::max(0.0, duration > 0 ? std::min(duration, position + seconds) : position + seconds);
    updateTimes();
    if (controlsVisible())
        scheduleHide();
}

void PlayerActivity::togglePause()
{
    if (mpv && !failed)
        mpv->togglePause();
}

void PlayerActivity::updatePaused(bool value)
{
    paused = value;
    playButton->setText(paused ? "app/player/play"_i18n : "app/player/pause"_i18n);
    pausedLabel->setVisibility(paused ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
    if (!paused && controlsVisible())
        scheduleHide();
}

void PlayerActivity::updateSpinner()
{
    bool show = !failed && (!started || waiting);
    loading->setVisibility(show ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
    spinner->animate(show);
}

void PlayerActivity::updateTimes()
{
    long long second = static_cast<long long>(std::floor(position));
    if (second == shownSecond && !controlsVisible())
        return;
    shownSecond = second;
    if (!controlsVisible())
        return;
    seekBar->setProgress(position, buffered, duration);
    elapsed->setText(nuvio::data::player::formatTime(position));
    remaining->setText(duration > 0 ? nuvio::data::player::formatTime(position - duration) : "");
}

void PlayerActivity::showError(const std::string& reason)
{
    if (closing || failed)
        return;
    failed = true;
    updateSpinner();
    controls->setVisibility(brls::Visibility::GONE);
    panelBox->setVisibility(brls::Visibility::GONE);
    panel = Panel::None;
    errorReason->setText(reason);
    errorBox->setVisibility(brls::Visibility::VISIBLE);
    brls::Application::giveFocus(errorOther);
}

// Stops playback and shows the stream picker again (it is the activity underneath).
void PlayerActivity::closeToPicker()
{
    if (closing)
        return;
    closing = true;
    if (mpv)
        mpv->stop();
    brls::Application::popActivity();
}

// From the error overlay: past the picker to the details screen.
void PlayerActivity::closeToDetails()
{
    if (closing)
        return;
    closing = true;
    if (mpv)
        mpv->stop();
    brls::Application::popActivity(brls::TransitionAnimation::FADE, []() { brls::Application::popActivity(); });
}

void PlayerActivity::openPanel(Panel which)
{
    panel = which;
    hideToken++; // the controls stay while a panel is open
    panelBox->setVisibility(brls::Visibility::VISIBLE);
    fillPanel();
}

void PlayerActivity::closePanel()
{
    panel = Panel::None;
    panelBox->setVisibility(brls::Visibility::GONE);
    panelList->clearViews();
    if (panelOpener && controlsVisible())
    {
        brls::Application::giveFocus(panelOpener);
        scheduleHide();
    }
    else
        brls::Application::giveFocus(root);
}

brls::Button* PlayerActivity::addPanelButton(const std::string& text, bool active, std::function<void()> onClick)
{
    auto* button = new brls::Button();
    button->setText((active ? "● " : "    ") + text);
    button->setWidth(brls::View::AUTO);
    button->setMarginBottom(6);
    button->registerClickAction([onClick](brls::View*)
        {
            onClick();
            return true;
        });
    panelList->addView(button);
    return button;
}

// Rebuilds the list of the open panel from the tracks mpv reported and the subtitles collected.
void PlayerActivity::fillPanel()
{
    // keep the focused entry across a rebuild caused by a late subtitle answer
    size_t focusedIndex = 0;
    auto children = panelList->getChildren();
    for (size_t i = 0; i < children.size(); i++)
        if (children[i]->isFocused())
            focusedIndex = i;
    bool hadFocus = std::any_of(children.begin(), children.end(), [](brls::View* v) { return v->isFocused(); });

    panelList->clearViews();
    brls::View* first = nullptr;
    auto remember     = [&first](brls::View* v)
    {
        if (!first)
            first = v;
    };

    if (panel == Panel::Audio)
    {
        panelTitle->setText("app/player/audio"_i18n);
        for (const auto& track : tracks)
            if (track.type == "audio")
                remember(addPanelButton(describe(track, brls::getStr("app/player/track", track.id)), track.selected,
                    [this, id = track.id]()
                    {
                        mpv->setAudioTrack(id);
                        closePanel();
                    }));
    }
    else if (panel == Panel::Subtitles)
    {
        panelTitle->setText("app/player/subtitles"_i18n);
        bool anySelected = std::any_of(tracks.begin(), tracks.end(), [](const Track& t) { return t.type == "sub" && t.selected; });
        remember(addPanelButton("app/player/off"_i18n, !anySelected,
            [this]()
            {
                mpv->setSubtitleTrack(std::nullopt);
                closePanel();
            }));

        for (const auto& track : tracks)
            if (track.type == "sub" && !track.external)
                remember(addPanelButton(describe(track, brls::getStr("app/player/track", track.id)), track.selected,
                    [this, id = track.id]()
                    {
                        mpv->setSubtitleTrack(id);
                        closePanel();
                    }));

        // external ones, grouped by language name; one that mpv has loaded shows as active through its track
        std::vector<std::pair<std::string, std::string>> languages; // name, code
        for (const auto& s : subtitles)
        {
            std::string name = nuvio::data::player::languageName(s.lang);
            if (std::none_of(languages.begin(), languages.end(), [&name](const auto& l) { return l.first == name; }))
                languages.emplace_back(name, s.lang);
        }
        std::sort(languages.begin(), languages.end());
        for (const auto& [name, code] : languages)
        {
            int count = 0;
            for (const auto& s : subtitles)
                count += nuvio::data::player::languageName(s.lang) == name;

            auto* header = new brls::Label();
            header->setText(name + " (" + std::to_string(count) + ")");
            header->setFontSize(16);
            header->setTextColor(brls::Application::getTheme()["brls/text_disabled"]);
            header->setMarginTop(10);
            header->setMarginBottom(6);
            panelList->addView(header);

            int number = 0;
            for (const auto& s : subtitles)
            {
                if (nuvio::data::player::languageName(s.lang) != name)
                    continue;
                number++;
                bool active = std::any_of(tracks.begin(), tracks.end(), [&s](const Track& t)
                    { return t.type == "sub" && t.external && t.selected && t.externalFilename == s.url; });
                remember(addPanelButton(s.addonName + " " + std::to_string(number), active,
                    [this, s]()
                    {
                        mpv->addSubtitle(s.url, s.addonName, s.lang);
                        closePanel();
                    }));
            }
        }
    }

    if (hadFocus && focusedIndex < panelList->getChildren().size())
        brls::Application::giveFocus(panelList->getChildren()[focusedIndex]);
    else if (first && !hadFocus)
        brls::Application::giveFocus(first);
}
