#pragma once

#include <borealis.hpp>
#include <functional>

#include "domain/meta_preview.hpp"

class PosterTitle;

// A catalog item: artwork sized by posterShape (126×189 poster, 224×126 landscape,
// 160×160 square, NuvioTV SizeTokens) with its title under it. Only the artwork
// takes focus, so the highlight frames the artwork alone; the title turns bold
// while it is focused. The artwork goes through ImageLoader; releaseImage() drops
// the texture of an off-screen cell and loadImage() brings it back from the caches.
class PosterCell : public brls::Box
{
  public:
    PosterCell();
    ~PosterCell() override;

    void setItem(const nuvio::domain::MetaPreview& item);
    const nuvio::domain::MetaPreview& getItem() const { return item; }

    void loadImage();
    void releaseImage();

    // the title under the artwork: its margin and one line
    static constexpr float TITLE_HEIGHT = 30;

    static float widthFor(nuvio::domain::PosterShape shape);
    static float heightFor(nuvio::domain::PosterShape shape);

    std::function<void(PosterCell*)> onSelect;
    std::function<void(PosterCell*)> onFocus;

  private:
    nuvio::domain::MetaPreview item;
    brls::Box* art; // the focusable part
    brls::Image* image;
    PosterTitle* title;
    bool imageLoaded = false;
};
