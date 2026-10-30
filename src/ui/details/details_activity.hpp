#pragma once

#include <borealis.hpp>
#include <vector>

#include "data/meta/meta_repository.hpp"
#include "domain/meta.hpp"
#include "domain/meta_preview.hpp"

// A title's details (NuvioTV ui/screens/detail): metadata from the best meta add-on,
// a hero column with Play, and for series season tabs with an episode list.
// Shows a spinner while loading and an error with Retry when no add-on has the title.
// B returns to where the user came from.
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

    nuvio::domain::MetaPreview item;
    std::string sourceAddonName;
    nuvio::data::meta::MetaRepository::Ticket ticket = 0;

    nuvio::domain::Meta meta;
    std::vector<nuvio::domain::Season> seasons;
    size_t selectedSeason = 0;
    std::vector<brls::Button*> seasonButtons;
    float tabOffset = 0; // how far the season tabs are slid to the left
    bool loaded = false;

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
    brls::Label* description     = nullptr;
    brls::Label* director        = nullptr;
    brls::Label* cast            = nullptr;
    brls::Box* episodes          = nullptr;
    brls::Box* seasonBox         = nullptr;
    brls::Box* seasonScroll = nullptr;
    brls::RecyclerFrame* recycler = nullptr;
};
