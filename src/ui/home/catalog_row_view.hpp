#pragma once

#include <borealis.hpp>
#include <functional>
#include <vector>

#include "core/net/http_types.hpp"
#include "domain/catalog_row.hpp"

class PosterCell;

// One Home row: "<Catalog> - <Type>" with "from <add-on>", then the items in a
// horizontal scroller and a trailing "See all" card while more pages exist.
// It starts pending (a placeholder of the final height) until load() is called;
// focus on one of the last 5 items loads the next page. Y opens See all.
class CatalogRowView : public brls::Box
{
  public:
    enum class State
    {
        Pending,
        Loading,
        Loaded,
        Failed, // hidden
    };

    static constexpr size_t MAX_ITEMS     = 200; // no load-more past this; See all goes further
    static constexpr size_t LOAD_MORE_AT  = 5;
    static constexpr float PENDING_HEIGHT = 280;

    CatalogRowView(const nuvio::domain::CatalogSource& source, const std::string& title);
    ~CatalogRowView() override;

    // Requests the first page, unless it already loaded or is loading.
    void load();
    // Shows a row loaded earlier without a request.
    void show(const nuvio::domain::CatalogRow& loaded);

    State getState() const { return state; }
    const nuvio::domain::CatalogRow& getRow() const { return row; }

    // Texture management for rows far from the viewport.
    void releaseImages();
    void restoreImages();
    bool imagesReleased() const { return released; }

    std::function<void(CatalogRowView*)> onFirstPage; // loaded or failed
    std::function<void(CatalogRowView*)> onPageAdded; // a later page arrived (row data changed)

  private:
    void requestPage(int skip);
    void applyPage(nuvio::domain::CatalogPage page, bool first);
    void addCells(size_t from);
    void updateSeeAll();

    nuvio::domain::CatalogRow row;
    State state   = State::Pending;
    bool released = false;
    bool loadingMore = false;
    nuvio::core::net::RequestHandle request;

    brls::Box* header;
    brls::HScrollingFrame* scroller;
    brls::Box* items;
    brls::Label* placeholder;
    brls::Box* seeAll = nullptr;
    std::vector<PosterCell*> cells;
};
