#pragma once

#include <borealis.hpp>
#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include "data/player/player_rules.hpp"
#include "data/player/subtitle_repository.hpp"
#include "domain/playback_request.hpp"
#include "domain/subtitle.hpp"
#include "ui/player/mpv_core.hpp"

namespace nuvio::ui::player
{
class PlayerView;
class SeekBar;
} // namespace nuvio::ui::player

// The player (NuvioTV ui/screens/player/PlayerScreen.kt): libmpv drawing full screen, a loading
// spinner until the first frame, a controls overlay, audio and subtitle panels, and an error overlay.
//
// With the controls hidden A pauses, Left/Right seek (further the longer they are held), Up, Down
// and X show the controls and B leaves. ZL/ZR seek 10 s any time. B first hides panels and controls.
// Leaving the activity stops mpv and frees its render context before the views go.
class PlayerActivity : public brls::Activity
{
  public:
    explicit PlayerActivity(const nuvio::domain::PlaybackRequest& request);
    ~PlayerActivity() override;

    brls::View* createContentView() override;
    void onContentAvailable() override;

  private:
    enum class Panel
    {
        None,
        Audio,
        Subtitles,
    };

    void start();
    void registerInput(brls::Box* surface);
    bool controlsVisible() const { return controls->getVisibility() == brls::Visibility::VISIBLE; }
    void showControls(bool focusSeekBar = true);
    void hideControls();
    void scheduleHide();
    void seek(bool forward);
    void seekBySeconds(double seconds);
    void togglePause();
    void updateTimes();
    void updatePaused(bool value);
    void updateSpinner();
    void back();
    void closeToPicker();
    void closeToDetails();
    void showError(const std::string& reason);
    void openPanel(Panel which);
    void closePanel();
    void fillPanel();
    brls::Button* addPanelButton(const std::string& text, bool active, std::function<void()> onClick);
    void teardown();

    nuvio::domain::PlaybackRequest request;
    std::unique_ptr<nuvio::ui::player::MpvCore> mpv;
    nuvio::data::player::SubtitleRepository::Ticket subtitleTicket = 0;

    // what mpv last told us
    double position = 0;
    double duration = 0;
    double buffered = 0;
    bool paused     = false;
    bool waiting    = false; // paused for the cache
    bool started    = false; // the first frame is out
    bool failed     = false;
    bool closing    = false;
    long long shownSecond = -1;
    std::vector<nuvio::data::player::Track> tracks;
    std::vector<nuvio::domain::Subtitle> subtitles;

    Panel panel             = Panel::None;
    brls::View* panelOpener = nullptr;

    // held-seek acceleration: a repeat counts while presses arrive close together
    std::chrono::steady_clock::time_point lastSeek{};
    int seekRepeat = 0;
    int hideToken  = 0;

    nuvio::ui::player::PlayerView* video = nullptr;
    nuvio::ui::player::SeekBar* seekBar  = nullptr;
    brls::Box* loading                   = nullptr;
    brls::ProgressSpinner* spinner       = nullptr;
    brls::Label* pausedLabel             = nullptr;
    brls::Box* controls                  = nullptr;
    brls::Label* title                   = nullptr;
    brls::Label* episode                 = nullptr;
    brls::Label* elapsed                 = nullptr;
    brls::Label* remaining               = nullptr;
    brls::Button* playButton             = nullptr;
    brls::Button* subtitlesButton        = nullptr;
    brls::Button* audioButton            = nullptr;
    brls::Button* sourcesButton          = nullptr;
    brls::Box* panelBox                  = nullptr;
    brls::Label* panelTitle              = nullptr;
    brls::Box* panelList                 = nullptr;
    brls::Box* errorBox                  = nullptr;
    brls::Label* errorReason             = nullptr;
    brls::Button* errorOther             = nullptr;
    brls::Button* errorBack              = nullptr;
    brls::Box* root                      = nullptr;
};
