#include "ui/library/library_tab.hpp"

#include "data/library/library_repository.hpp"
#include "ui/common/options_menu.hpp"
#include "ui/common/poster_cell.hpp"
#include "ui/common/viewport_box.hpp"
#include "ui/navigation.hpp"

using namespace brls::literals; // for _i18n
using nuvio::data::library::Filter;
using nuvio::data::library::LibraryRepository;
using nuvio::data::library::Sort;

namespace
{
constexpr float CELL_GAP = 16;

// in the order the menus list them
const std::vector<Sort> SORTS = {Sort::Provider, Sort::AddedNewest, Sort::AddedOldest, Sort::TitleAsc, Sort::TitleDesc};
const std::vector<Filter> FILTERS = {Filter::All, Filter::Movies, Filter::Series};

std::string sortLabel(Sort sort)
{
    switch (sort)
    {
        case Sort::Provider: return "app/library/sort_default"_i18n;
        case Sort::AddedNewest: return "app/library/sort_added_desc"_i18n;
        case Sort::AddedOldest: return "app/library/sort_added_asc"_i18n;
        case Sort::TitleAsc: return "app/library/sort_title_asc"_i18n;
        case Sort::TitleDesc: return "app/library/sort_title_desc"_i18n;
    }
    return "";
}

std::string filterLabel(Filter filter)
{
    switch (filter)
    {
        case Filter::Movies: return "app/library/type_movies"_i18n;
        case Filter::Series: return "app/library/type_series"_i18n;
        default: return "app/library/type_all"_i18n;
    }
}

template <typename T>
int indexOf(const std::vector<T>& list, T value)
{
    for (size_t i = 0; i < list.size(); i++)
        if (list[i] == value)
            return static_cast<int>(i);
    return 0;
}
} // namespace

LibraryTab::LibraryTab()
{
    this->setAxis(brls::Axis::COLUMN);
    this->setGrow(1);

    scroll = new brls::ScrollingFrame();
    scroll->setGrow(1);
    scroll->setScrollingBehavior(brls::ScrollingBehavior::CENTERED);
    grid = new ViewportBox(brls::Axis::COLUMN, scroll);
    grid->setPadding(32, 48, 32, 48);
    scroll->setContentView(grid);
    this->addView(scroll);

    header = new brls::Box(brls::Axis::ROW);
    header->setAlignItems(brls::AlignItems::CENTER);
    header->setMarginBottom(24);
    auto* title = new brls::Label();
    title->setText("app/tabs/library"_i18n);
    title->setFontSize(28);
    title->setGrow(1);
    header->addView(title);
    sortButton = new brls::Button();
    sortButton->setMarginRight(12);
    sortButton->registerClickAction([this](brls::View*)
        {
            chooseSort();
            return true;
        });
    header->addView(sortButton);
    filterButton = new brls::Button();
    filterButton->registerClickAction([this](brls::View*)
        {
            chooseFilter();
            return true;
        });
    header->addView(filterButton);
    grid->addView(header);

    empty = new brls::Box(brls::Axis::COLUMN);
    empty->setAlignItems(brls::AlignItems::CENTER);
    empty->setMarginTop(100);
    emptyTitle = new brls::Label();
    emptyTitle->setFontSize(22);
    emptyTitle->setMarginBottom(12);
    empty->addView(emptyTitle);
    emptyHint = new brls::Label();
    emptyHint->setFontSize(16);
    emptyHint->setTextColor(brls::Application::getTheme()["brls/text_disabled"]);
    emptyHint->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    empty->addView(emptyHint);
    grid->addView(empty);

    subscription = LibraryRepository::shared().subscribe([this] { rebuild(); });
    rebuild();
}

LibraryTab::~LibraryTab()
{
    if (LibraryRepository::hasShared())
        LibraryRepository::shared().unsubscribe(subscription);
}

brls::View* LibraryTab::create()
{
    return new LibraryTab();
}

void LibraryTab::updateButtons()
{
    auto& repo = LibraryRepository::shared();
    sortButton->setText(brls::getStr("app/library/sort", sortLabel(repo.sort())));
    filterButton->setText(brls::getStr("app/library/type", filterLabel(repo.filter())));
}

void LibraryTab::rebuild()
{
    auto& repo = LibraryRepository::shared();
    updateButtons();

    // focus may sit on a poster that is about to go: park it on the sort button
    bool focusInGrid = false;
    for (brls::View* view = brls::Application::getCurrentFocus(); view; view = view->getParent())
        for (auto* line : lines)
            focusInGrid = focusInGrid || view == line;
    if (focusInGrid)
        brls::Application::giveFocus(sortButton);

    for (auto* line : lines)
        grid->removeView(line);
    lines.clear();
    grid->setLastFocusedView(nullptr);

    auto items = nuvio::data::library::sorted(nuvio::data::library::filtered(repo.items(), repo.filter()), repo.sort());

    bool none = items.empty();
    empty->setVisibility(none ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
    if (none)
    {
        bool filteredOut = !repo.items().empty();
        emptyTitle->setText(filteredOut ? "app/library/empty_filtered"_i18n : "app/library/empty"_i18n);
        emptyHint->setText(filteredOut ? "" : "app/library/empty_hint"_i18n);
        return;
    }

    for (size_t i = 0; i < items.size(); i++)
    {
        if (i % COLUMNS == 0)
        {
            auto* line = new brls::Box(brls::Axis::ROW);
            line->setMarginBottom(24);
            // before the empty-state box, which stays last
            grid->addView(line, grid->getChildren().size() - 1);
            lines.push_back(line);
        }
        auto* cell = new PosterCell();
        cell->setMarginRight(CELL_GAP);
        cell->setItem(nuvio::data::library::toPreview(items[i]));
        cell->onSelect = [](PosterCell* c) { nuvio::ui::openDetails(c->getItem(), ""); };
        lines.back()->addView(cell);
    }
}

void LibraryTab::chooseSort()
{
    std::vector<std::string> options;
    for (Sort sort : SORTS)
        options.push_back(sortLabel(sort));
    nuvio::ui::showOptions("app/library/sort_title"_i18n, options, indexOf(SORTS, LibraryRepository::shared().sort()),
        [](int index) { LibraryRepository::shared().setSort(SORTS[index]); });
}

void LibraryTab::chooseFilter()
{
    std::vector<std::string> options;
    for (Filter filter : FILTERS)
        options.push_back(filterLabel(filter));
    nuvio::ui::showOptions("app/library/type_title"_i18n, options,
        indexOf(FILTERS, LibraryRepository::shared().filter()),
        [](int index) { LibraryRepository::shared().setFilter(FILTERS[index]); });
}
