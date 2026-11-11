#pragma once

#include <borealis.hpp>
#include <vector>

class PosterCell;

// The Library tab (NuvioTV ui/screens/library, local source only): the saved titles as a poster grid,
// with Sort and Type buttons above it whose choices are remembered per profile. Selecting a poster
// opens its Details; an empty library explains how to add titles. It rebuilds when the library changes.
class LibraryTab : public brls::Box
{
  public:
    static constexpr size_t COLUMNS = 6;

    LibraryTab();
    ~LibraryTab() override;

    static brls::View* create();

  private:
    void rebuild();
    void updateButtons();
    void chooseSort();
    void chooseFilter();

    brls::ScrollingFrame* scroll = nullptr;
    brls::Box* grid              = nullptr;
    brls::Box* header            = nullptr;
    brls::Button* sortButton     = nullptr;
    brls::Button* filterButton   = nullptr;
    brls::Box* empty             = nullptr;
    brls::Label* emptyTitle      = nullptr;
    brls::Label* emptyHint       = nullptr;
    std::vector<brls::Box*> lines;
    int subscription = 0;
};
