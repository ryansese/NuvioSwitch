#pragma once

#include <borealis.hpp>
#include <memory>
#include <string>
#include <vector>

#include "core/storage/store.hpp"
#include "data/search/search_history.hpp"

class CatalogRowView;

// The Search tab (NuvioTV ui/screens/search): a query field that opens the software
// keyboard, then one row per searchable catalog filled in as responses arrive.
// With no query it lists the recent searches (X removes one, Y clears all).
// The search runs when the keyboard is confirmed; there is no search while typing.
//
// borealis rebuilds a tab's view each time the tab is selected, so the query and
// results start empty each time; only the history is kept (in the search_history store).
class SearchTab : public brls::Box
{
  public:
    static constexpr int SEARCH_TAB_INDEX = 1;  // position in main.xml's sidebar
    static constexpr int RELEASE_DISTANCE = 3;  // screens away before a row drops its textures
    static constexpr int MAX_QUERY_LENGTH = 64;

    SearchTab();
    ~SearchTab() override;

    static brls::View* create();

    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
        brls::FrameContext* ctx) override;

  private:
    class Field;

    void openKeyboard();
    // Runs a search for text; a query too short to search changes nothing.
    void search(const std::string& text);
    void clearQuery();
    void clearResults();
    void onRowSettled(CatalogRowView* row, unsigned generation);
    void finishSearch();
    void showMessage(const std::string& text);
    void rebuildHistory(size_t focusIndex = 0);
    void saveHistory();
    void updateRowTextures();

    std::shared_ptr<bool> alive = std::make_shared<bool>(true); // for the keyboard callback, which can outlive the tab
    std::shared_ptr<nuvio::core::storage::Store> store;
    nuvio::data::search::SearchHistory history;

    std::string query;
    unsigned generation = 0; // each search takes the next one; rows of an earlier one are ignored
    size_t pending      = 0;
    bool anyResults     = false;

    brls::ScrollingFrame* scroll;
    brls::Box* content;
    brls::Box* field;
    brls::Label* fieldLabel;
    brls::Label* message;
    brls::Box* historyBox;
    std::vector<CatalogRowView*> rows;
};
