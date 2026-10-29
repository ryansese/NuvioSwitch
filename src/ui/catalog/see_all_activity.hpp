#pragma once

#include <borealis.hpp>
#include <vector>

#include "core/net/http_types.hpp"
#include "domain/catalog_row.hpp"

class PosterCell;

// Every item of one catalog (NuvioTV ui/screens/CatalogSeeAllScreen.kt): a grid
// that starts from what the Home row already loaded and fetches further pages
// as focus reaches its last two lines. B returns to Home.
//
// The grid is lines of PosterCells in a ScrollingFrame rather than a
// RecyclerFrame: appending a page there keeps focus and scroll where they are,
// while RecyclerFrame can only reload everything. Lines far from the screen
// release their textures instead.
class CatalogSeeAllActivity : public brls::Activity
{
  public:
    static constexpr size_t LOAD_MORE_LINES = 2;
    static constexpr int RELEASE_DISTANCE   = 3; // screens

    explicit CatalogSeeAllActivity(const nuvio::domain::CatalogRow& row);
    ~CatalogSeeAllActivity() override;

    brls::View* createContentView() override;

  private:
    class Grid;

    void addItems(size_t from);
    void loadMore();
    void updateTextures();
    size_t columns() const;

    nuvio::domain::CatalogRow row;
    bool loadingMore = false;
    nuvio::core::net::RequestHandle request;

    brls::ScrollingFrame* scroll = nullptr;
    brls::Box* grid              = nullptr;
    brls::Label* status          = nullptr;
    std::vector<brls::Box*> lines;
    std::vector<std::vector<PosterCell*>> lineCells;
    std::vector<bool> lineReleased;
};
