#pragma once

#include <borealis.hpp>

#include "domain/meta.hpp"

// One episode in the details list: thumbnail, "S1E2 · Title", release date and a short overview, with a
// check mark when watched and a progress bar when in progress.
class EpisodeCell : public brls::RecyclerCell
{
  public:
    EpisodeCell();
    ~EpisodeCell() override;

    void setEpisode(const nuvio::domain::Video& episode, const std::string& fallbackPoster);
    // A check mark when watched; a progress bar for fraction > 0 (0..1) when not.
    void setWatchState(bool watched, float fraction);
    void prepareForReuse() override;

    // the cell's height with its margin, for RecyclerDataSource::heightForRow
    static constexpr float HEIGHT = 128;

    static brls::RecyclerCell* create();

  private:
    BRLS_BIND(brls::Image, thumbnail, "episode/thumbnail");
    BRLS_BIND(brls::Label, title, "episode/title");
    BRLS_BIND(brls::Label, check, "episode/check");
    BRLS_BIND(brls::Box, bar, "episode/bar");
    BRLS_BIND(brls::Box, fill, "episode/fill");
    BRLS_BIND(brls::Label, date, "episode/date");
    BRLS_BIND(brls::Label, overview, "episode/overview");
};
