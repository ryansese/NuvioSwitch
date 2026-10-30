#include "ui/search/search_tab.hpp"

#include "data/addons/addon_repository.hpp"
#include "data/catalog/home_selection.hpp"
#include "data/search/search_targets.hpp"
#include "ui/common/viewport_box.hpp"
#include "ui/home/catalog_row_view.hpp"

using namespace brls::literals; // for _i18n
using nuvio::data::addons::AddonRepository;
namespace search  = nuvio::data::search;
namespace catalog = nuvio::data::catalog;

namespace
{
const char* const HISTORY_KEY = "queries";
} // namespace

SearchTab::SearchTab()
{
    this->inflateFromXMLRes("xml/tabs/search.xml");
    scroll = static_cast<brls::ScrollingFrame*>(this->getView("search/scroll"));
    scroll->setScrollingBehavior(brls::ScrollingBehavior::CENTERED);

    // B (Escape on desktop) goes back to Search's sidebar item; see HomeTab for why the
    // tab frame's own B is not enough
    this->registerAction(
        "hints/back"_i18n, brls::ControllerButton::BUTTON_B,
        [this](brls::View*)
        {
            for (brls::View* view = this->getParent(); view; view = view->getParent())
                if (auto* tabs = dynamic_cast<brls::TabFrame*>(view))
                {
                    tabs->focusTab(SEARCH_TAB_INDEX);
                    break;
                }
            return true;
        },
        false, false, brls::SOUND_BACK);

    content = new ViewportBox(brls::Axis::COLUMN, scroll);
    content->setPadding(32, 48, 32, 48);
    scroll->setContentView(content);

    // the query field
    field = new brls::Box(brls::Axis::ROW);
    field->setFocusable(true);
    field->setHeight(56);
    field->setAlignItems(brls::AlignItems::CENTER);
    field->setCornerRadius(8);
    field->setHighlightCornerRadius(8);
    field->setBackgroundColor(brls::Application::getTheme()["nuvio/card"]);
    field->setPadding(0, 24, 0, 24);
    field->setMarginBottom(24);
    fieldLabel = new brls::Label();
    fieldLabel->setFontSize(20);
    fieldLabel->setSingleLine(true);
    field->addView(fieldLabel);
    field->registerClickAction([this](brls::View*)
        {
            openKeyboard();
            return true;
        });
    // X clears the query and brings the recent searches back; offered only while there is one
    field->registerAction("app/search/clear_query"_i18n, brls::ControllerButton::BUTTON_X, [this](brls::View*)
        {
            clearQuery();
            return true;
        });
    field->setActionAvailable(brls::ControllerButton::BUTTON_X, false);
    content->addView(field);

    message = new brls::Label();
    message->setFontSize(18);
    message->setTextColor(brls::Application::getTheme()["brls/text_disabled"]);
    message->setMarginBottom(24);
    message->setVisibility(brls::Visibility::GONE);
    content->addView(message);

    historyBox = new brls::Box(brls::Axis::COLUMN);
    // Y anywhere in the list clears it
    historyBox->registerAction("app/search/clear_all"_i18n, brls::ControllerButton::BUTTON_Y, [this](brls::View*)
        {
            if (!history.clear())
                return false;
            saveHistory();
            brls::Application::giveFocus(field);
            rebuildHistory();
            return true;
        });
    content->addView(historyBox);

    store   = nuvio::core::storage::Store::open("search_history");
    history = search::SearchHistory::fromJson(store->get(HISTORY_KEY));
    clearQuery();
}

SearchTab::~SearchTab()
{
    *alive = false;
}

brls::View* SearchTab::create()
{
    return new SearchTab();
}

void SearchTab::openKeyboard()
{
    std::weak_ptr<bool> token = alive;
    // the callback runs only on confirm; cancelling leaves the query and results alone
    brls::Application::getImeManager()->openForText(
        [this, token](std::string text)
        {
            auto stillThere = token.lock();
            if (stillThere && *stillThere)
                search(text);
        },
        "app/search/keyboard_title"_i18n, "", MAX_QUERY_LENGTH, query);
}

void SearchTab::search(const std::string& text)
{
    std::string trimmed = search::trimQuery(text);
    if (!search::isSearchableQuery(trimmed))
        return;

    // the field keeps focus; rows and the history list go away under it
    brls::Application::giveFocus(field);
    clearResults();
    query = trimmed;
    fieldLabel->setText(query);
    fieldLabel->setTextColor(brls::Application::getTheme()["brls/text"]);
    field->setActionAvailable(brls::ControllerButton::BUTTON_X, true);
    historyBox->setVisibility(brls::Visibility::GONE);
    message->setVisibility(brls::Visibility::GONE);

    auto targets = search::searchTargets(AddonRepository::shared().addons());
    if (targets.empty())
    {
        showMessage("app/search/no_support"_i18n);
        return;
    }

    generation++;
    pending    = targets.size();
    anyResults = false;
    unsigned current = generation;

    // rows in target order; each shows a loading state until its response arrives
    for (const auto& target : targets)
    {
        auto* row = new CatalogRowView(target,
            catalog::rowTitle(target.catalog, "app/home/type_movie"_i18n, "app/home/type_series"_i18n),
            CatalogRowView::Options{search::searchExtras(query), true});
        row->onFirstPage = [this, current](CatalogRowView* r) { onRowSettled(r, current); };
        content->addView(row);
        rows.push_back(row);
    }
    // requests start after the rows exist, so every row can show its loading state first
    for (auto* row : rows)
        row->load();
}

void SearchTab::clearQuery()
{
    brls::Application::giveFocus(field);
    clearResults();
    generation++; // anything still arriving belongs to a search nobody is looking at
    query.clear();
    field->setActionAvailable(brls::ControllerButton::BUTTON_X, false);
    fieldLabel->setText("app/search/hint"_i18n);
    fieldLabel->setTextColor(brls::Application::getTheme()["brls/text_disabled"]);
    message->setVisibility(brls::Visibility::GONE);
    rebuildHistory();
}

void SearchTab::clearResults()
{
    for (auto* row : rows)
        content->removeView(row); // cancels its request
    rows.clear();
    // removeView leaves the last-focused pointer on a deleted row
    content->setLastFocusedView(nullptr);
    pending    = 0;
    anyResults = false;
}

void SearchTab::onRowSettled(CatalogRowView* row, unsigned forGeneration)
{
    // a row of an earlier search can't settle after clearResults() deleted it, but the
    // generation is checked all the same
    if (forGeneration != generation || pending == 0)
        return;
    pending--;
    anyResults = anyResults || row->getState() == CatalogRowView::State::Loaded;
    if (pending == 0)
        finishSearch();
}

void SearchTab::finishSearch()
{
    if (!anyResults)
    {
        showMessage(brls::getStr("app/search/no_results", query));
        return;
    }
    // a query is remembered only when it found something
    if (history.add(query))
        saveHistory();
}

void SearchTab::showMessage(const std::string& text)
{
    message->setText(text);
    message->setVisibility(brls::Visibility::VISIBLE);
}

void SearchTab::saveHistory()
{
    store->set(HISTORY_KEY, history.toJson());
    store->save();
}

void SearchTab::rebuildHistory(size_t focusIndex)
{
    historyBox->clearViews();
    historyBox->setLastFocusedView(nullptr);

    bool show = query.empty() && !history.entries().empty();
    historyBox->setVisibility(show ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
    if (!show)
        return;

    auto* title = new brls::Label();
    title->setText("app/search/recent"_i18n);
    title->setFontSize(22);
    title->setMarginBottom(12);
    historyBox->addView(title);

    brls::View* toFocus = nullptr;
    const auto& entries = history.entries();
    for (size_t i = 0; i < entries.size(); i++)
    {
        auto* entry = new brls::Box(brls::Axis::ROW);
        entry->setFocusable(true);
        entry->setHeight(48);
        entry->setAlignItems(brls::AlignItems::CENTER);
        entry->setCornerRadius(8);
        entry->setHighlightCornerRadius(8);
        entry->setBackgroundColor(brls::Application::getTheme()["nuvio/card"]);
        entry->setPadding(0, 24, 0, 24);
        entry->setMarginBottom(8);
        auto* label = new brls::Label();
        label->setText(entries[i]);
        label->setFontSize(18);
        label->setSingleLine(true);
        entry->addView(label);

        std::string text = entries[i];
        entry->registerClickAction([this, text](brls::View*)
            {
                search(text);
                return true;
            });
        entry->registerAction("app/search/remove"_i18n, brls::ControllerButton::BUTTON_X, [this, i](brls::View*)
            {
                if (!history.remove(i))
                    return false;
                saveHistory();
                brls::Application::giveFocus(field);
                // focus moves to the entry that took the removed one's place
                rebuildHistory(i);
                return true;
            });
        historyBox->addView(entry);
        if (i == focusIndex && focusIndex > 0)
            toFocus = entry;
    }
    // after a removal focus goes to the same position, or the last entry; at a plain rebuild it stays put
    if (focusIndex > 0)
    {
        if (!toFocus)
            toFocus = historyBox->getChildren().back();
        brls::Application::giveFocus(toFocus);
    }
}

void SearchTab::updateRowTextures()
{
    float top    = scroll->getContentOffsetY();
    float bottom = top + scroll->getHeight();
    float far    = RELEASE_DISTANCE * scroll->getHeight();

    for (auto* row : rows)
    {
        if (row->getState() != CatalogRowView::State::Loaded || row->getHeight() <= 0 || scroll->getHeight() <= 0)
            continue;
        float rowTop    = row->getLocalY();
        float rowBottom = rowTop + row->getHeight();
        if (rowBottom < top - far || rowTop > bottom + far)
            row->releaseImages();
        else
            row->restoreImages();
    }
}

void SearchTab::draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
    brls::FrameContext* ctx)
{
    updateRowTextures();
    Box::draw(vg, x, y, width, height, style, ctx);
}
