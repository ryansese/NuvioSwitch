#pragma once

#include <borealis.hpp>

#include "domain/meta.hpp"

// One episode in the details list: thumbnail, "S1E2 · Title", release date and a short overview.
class EpisodeCell : public brls::RecyclerCell
{
  public:
    EpisodeCell();
    ~EpisodeCell() override;

    void setEpisode(const nuvio::domain::Video& episode, const std::string& fallbackPoster);
    void prepareForReuse() override;

    // the cell's height with its margin, for RecyclerDataSource::heightForRow
    static constexpr float HEIGHT = 128;

    static brls::RecyclerCell* create();

  private:
    BRLS_BIND(brls::Image, thumbnail, "episode/thumbnail");
    BRLS_BIND(brls::Label, title, "episode/title");
    BRLS_BIND(brls::Label, date, "episode/date");
    BRLS_BIND(brls::Label, overview, "episode/overview");
};
