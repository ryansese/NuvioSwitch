#pragma once

#include <borealis.hpp>
#include <chrono>
#include <vector>

#include "domain/meta_preview.hpp"

// The hero above Home's rows (NuvioTV ui/components/HeroCarousel.kt): the current
// item's background, its logo (or name), release info and rating, description,
// and a position indicator. It advances every 8 s unless focused; when focused,
// left and right change the item and A opens it.
class HeroCarousel : public brls::Box
{
  public:
    static constexpr std::chrono::seconds ADVANCE_EVERY{8};
    static constexpr float HEIGHT = 340;

    HeroCarousel();
    ~HeroCarousel() override;

    // items with their source add-on names; an empty list hides the hero.
    void setItems(std::vector<nuvio::domain::MetaPreview> items, std::vector<std::string> sourceNames);

    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
        brls::FrameContext* ctx) override;

  private:
    void showItem(size_t index);
    bool step(int delta);

    std::vector<nuvio::domain::MetaPreview> items;
    std::vector<std::string> sourceNames;
    size_t current = 0;
    std::chrono::steady_clock::time_point shownAt;

    brls::Image* background;
    brls::Image* logo;
    brls::Label* name;
    brls::Label* meta;
    brls::Label* description;
    brls::Label* position;
};
