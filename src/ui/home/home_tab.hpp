#pragma once

#include <borealis.hpp>
#include <string>
#include <vector>

class CatalogRowView;
class HeroCarousel;

// Home in NuvioTV's Classic layout: a hero carousel above one row per catalog of
// the enabled add-ons. The first 4 rows load at once; the rest load as they come
// within a row of the screen, and rows far off screen drop their textures.
// Loaded pages are kept for the session (the tab view is recreated on every tab
// switch), and Home rebuilds when the list of Home catalogs changes.
class HomeTab : public brls::Box
{
  public:
    static constexpr size_t EAGER_ROWS = 4;      // NuvioTV eagerCatalogLoadCount
    static constexpr int RELEASE_DISTANCE = 3;   // rows away from the screen before textures go
    static constexpr int HOME_TAB_INDEX   = 0;   // positions in main.xml's sidebar
    static constexpr int ADDONS_TAB_INDEX = 3;

    HomeTab();
    ~HomeTab() override;

    static brls::View* create();

    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
        brls::FrameContext* ctx) override;

  private:
    enum class State
    {
        Loading,
        Content,
        NoCatalogs,
        Error,
    };

    // Rebuilds the rows when the Home catalogs differ from the ones shown (or force).
    void rebuild(bool force = false);
    void onRowFirstPage(CatalogRowView* row);
    void updateHero();
    void updateState();
    void showState(State state);
    void loadNearbyRows();
    bool hasFocusInside() const;

    brls::ScrollingFrame* scroll;
    brls::Box* content;
    HeroCarousel* hero;
    brls::Box* stateBox;
    brls::Label* stateText;
    brls::Box* stateButton;
    brls::Label* stateButtonText;

    std::vector<std::string> keys;
    std::vector<CatalogRowView*> rows;
    State state = State::Loading;
    int subscription = 0;
};
