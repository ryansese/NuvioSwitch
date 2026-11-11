#pragma once

#include <borealis.hpp>
#include <vector>

#include "data/meta/meta_repository.hpp"
#include "data/progress/play_target.hpp"
#include "domain/meta.hpp"
#include "domain/meta_preview.hpp"

// A title's details (NuvioTV ui/screens/detail): metadata from the best meta add-on,
// a hero column with Play, and for series season tabs with an episode list.
// Shows a spinner while loading and an error with Retry when no add-on has the title.
// B returns to where the user came from.
//
// NuvioSwitch-0009 adds "Add to / Remove from library", "Mark as watched / unwatched" for movies, check
// marks and progress bars on episodes with X for an episode's options, and a play button that resumes
// ("Resume · 52m left") or names the next episode ("Play S1E4"). These follow the library and progress
// repositories while the screen is open.
class EpisodeCell;

class DetailsActivity : public brls::Activity
{
  public:
    DetailsActivity(const nuvio::domain::MetaPreview& item, const std::string& sourceAddonName);
    ~DetailsActivity() override;

    brls::View* createContentView() override;
    void onContentAvailable() override;

  private:
    class Episodes;

    void load();
    void showLoading();
    void showError();
    void showMeta(const nuvio::domain::Meta& meta);
    void selectSeason(size_t index);
    void revealSeason(size_t index);
    void play();
    void playEpisode(const nuvio::domain::Video& episode);
    void updateActions();
    void refreshEpisodes();
    void applyWatchState(EpisodeCell* cell, const nuvio::domain::Video& episode);
    void toggleLibrary();
    void toggleWatched();
    void episodeOptions(size_t row);

    nuvio::domain::MetaPreview item;
    std::string sourceAddonName;
    nuvio::data::meta::MetaRepository::Ticket ticket = 0;

    nuvio::domain::Meta meta;
    std::vector<nuvio::domain::Season> seasons;
    size_t selectedSeason = 0;
    std::vector<brls::Button*> seasonButtons;
    float tabOffset = 0; // how far the season tabs are slid to the left
    bool loaded = false;
    nuvio::data::progress::PlayTarget target;
    std::vector<EpisodeCell*> episodeCells; // every cell the list made, for refreshing in place
    int progressSubscription = 0;
    int librarySubscription  = 0;

    brls::Image* backdrop        = nullptr;
    brls::Box* status            = nullptr;
    brls::ProgressSpinner* spinner = nullptr;
    brls::Label* message         = nullptr;
    brls::Button* retry          = nullptr;
    brls::ScrollingFrame* heroScroll = nullptr;
    brls::Image* logo            = nullptr;
    brls::Label* title           = nullptr;
    brls::Label* info            = nullptr;
    brls::Label* genres          = nullptr;
    brls::Button* playButton     = nullptr;
    brls::Button* libraryButton  = nullptr;
    brls::Button* watchedButton  = nullptr;
    brls::Label* description     = nullptr;
    brls::Label* director        = nullptr;
    brls::Label* cast            = nullptr;
    brls::Box* episodes          = nullptr;
    brls::Box* seasonBox         = nullptr;
    brls::Box* seasonScroll = nullptr;
    brls::RecyclerFrame* recycler = nullptr;
};
