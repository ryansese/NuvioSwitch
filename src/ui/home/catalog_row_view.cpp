#include "ui/home/catalog_row_view.hpp"

#include <algorithm>

#include "data/catalog/catalog_repository.hpp"
#include "ui/common/poster_cell.hpp"
#include "ui/common/viewport_box.hpp"
#include "ui/navigation.hpp"

using namespace brls::literals; // for _i18n
using nuvio::data::catalog::CatalogRepository;
using namespace nuvio::domain;

CatalogRowView::CatalogRowView(const CatalogSource& source, const std::string& title)
    : CatalogRowView(source, title, Options())
{
}

CatalogRowView::CatalogRowView(const CatalogSource& source, const std::string& title, Options options)
    : options(std::move(options))
{
    row.source = source;

    this->setAxis(brls::Axis::COLUMN);
    this->setMarginBottom(28);

    header = new brls::Box(brls::Axis::ROW);
    header->setAlignItems(brls::AlignItems::FLEX_END);
    header->setMarginBottom(12);
    auto* titleLabel = new brls::Label();
    titleLabel->setText(title);
    titleLabel->setFontSize(22);
    titleLabel->setMarginRight(12);
    header->addView(titleLabel);
    auto* caption = new brls::Label();
    caption->setText(brls::getStr("app/home/from", source.addonName));
    caption->setFontSize(15);
    caption->setTextColor(brls::Application::getTheme()["brls/text_disabled"]);
    header->addView(caption);
    this->addView(header);

    placeholder = new brls::Label();
    placeholder->setText("app/home/loading_row"_i18n);
    placeholder->setFontSize(16);
    placeholder->setTextColor(brls::Application::getTheme()["brls/text_disabled"]);
    placeholder->setHeight(PENDING_HEIGHT - 40);
    placeholder->setVerticalAlign(brls::VerticalAlign::CENTER);
    this->addView(placeholder);

    scroller = new brls::HScrollingFrame();
    scroller->setScrollingIndicatorVisible(false);
    scroller->setVisibility(brls::Visibility::GONE);
    items = new ViewportBox(brls::Axis::ROW, scroller);
    items->setAlignItems(brls::AlignItems::FLEX_START);
    // room for the focus highlight around the first and last cells
    items->setPadding(6, 6, 6, 6);
    scroller->setContentView(items);
    this->addView(scroller);

    // Y anywhere in the row opens See all, the row's option next to the trailing card
    if (!this->options.firstPageOnly)
        this->registerAction("app/home/see_all"_i18n, brls::ControllerButton::BUTTON_Y, [this](brls::View*)
            {
                if (state != State::Loaded)
                    return false;
                nuvio::ui::openSeeAll(row);
                return true;
            });
}

CatalogRowView::~CatalogRowView()
{
    request.cancel();
}

void CatalogRowView::load()
{
    if (state != State::Pending)
        return;
    state = State::Loading;
    requestPage(0);
}

void CatalogRowView::show(const CatalogRow& loaded)
{
    request.cancel();
    row.items    = loaded.items;
    row.nextSkip = loaded.nextSkip;
    row.hasMore  = loaded.hasMore;
    state        = State::Loaded;
    placeholder->setVisibility(brls::Visibility::GONE);
    scroller->setVisibility(brls::Visibility::VISIBLE);
    addCells(0);
    updateSeeAll();
}

void CatalogRowView::requestPage(int skip)
{
    request = CatalogRepository::shared().load(row.source, skip, options.extras,
        [this, first = skip == 0](CatalogRepository::Result result)
        {
            if (!result.ok)
            {
                if (first)
                {
                    state = State::Failed;
                    this->setVisibility(brls::Visibility::GONE);
                    if (onFirstPage)
                        onFirstPage(this);
                }
                else
                {
                    // a failed later page ends the row where it is
                    loadingMore = false;
                    row.hasMore = false;
                    updateSeeAll();
                }
                return;
            }
            applyPage(std::move(result.page), first);
        });
}

void CatalogRowView::applyPage(CatalogPage page, bool first)
{
    if (first && page.items.empty())
    {
        state = State::Failed;
        this->setVisibility(brls::Visibility::GONE);
        if (onFirstPage)
            onFirstPage(this);
        return;
    }

    size_t from = row.items.size();
    for (auto& item : page.items)
    {
        // pages can overlap; an id already in the row isn't added twice
        bool known = false;
        for (const auto& existing : row.items)
            known = known || existing.id == item.id;
        if (!known)
            row.items.push_back(std::move(item));
    }
    row.nextSkip = page.nextSkip;
    // whole pages are kept, so See all can carry on from nextSkip past the row's cap
    row.hasMore = page.hasMore && !options.firstPageOnly;
    loadingMore  = false;

    if (first)
    {
        state = State::Loaded;
        placeholder->setVisibility(brls::Visibility::GONE);
        scroller->setVisibility(brls::Visibility::VISIBLE);
    }
    addCells(from);
    updateSeeAll();

    if (first && onFirstPage)
        onFirstPage(this);
    else if (!first && onPageAdded)
        onPageAdded(this);
}

void CatalogRowView::addCells(size_t from)
{
    // a scrolling frame has no height of its own: fit the tallest artwork, its title and the highlight room
    float artwork = 0;
    for (const auto& item : row.items)
        artwork = std::max(artwork, PosterCell::heightFor(item.posterShape));
    scroller->setHeight(artwork + PosterCell::TITLE_HEIGHT + 12);

    for (size_t i = from; i < row.items.size(); i++)
    {
        auto* cell = new PosterCell();
        cell->setMarginRight(16);
        cell->setItem(row.items[i]);
        if (released)
            cell->releaseImage();
        cell->onSelect = [this](PosterCell* c) { nuvio::ui::openDetails(c->getItem(), row.source.addonName); };
        cell->onFocus  = [this, i](PosterCell*)
        {
            // past MAX_ITEMS the row stops growing; See all is the way further
            if (row.hasMore && !loadingMore && row.items.size() < MAX_ITEMS && i + LOAD_MORE_AT >= row.items.size())
            {
                loadingMore = true;
                requestPage(row.nextSkip);
            }
        };
        // before the See all card, which stays last
        if (seeAll)
            items->addView(cell, items->getChildren().size() - 1);
        else
            items->addView(cell);
        cells.push_back(cell);
    }
}

void CatalogRowView::updateSeeAll()
{
    if (seeAll && !row.hasMore)
    {
        // the end of the catalog: move focus off the card before it goes
        if (brls::Application::getCurrentFocus() == seeAll && !cells.empty())
            brls::Application::giveFocus(cells.back());
        items->removeView(seeAll);
        seeAll = nullptr;
        return;
    }
    if (seeAll || !row.hasMore)
        return;

    seeAll = new brls::Box(brls::Axis::COLUMN);
    seeAll->setFocusable(true);
    seeAll->setDimensions(126, 189);
    seeAll->setCornerRadius(12);
    seeAll->setHighlightCornerRadius(12);
    seeAll->setBackgroundColor(brls::Application::getTheme()["nuvio/card"]);
    seeAll->setJustifyContent(brls::JustifyContent::CENTER);
    seeAll->setAlignItems(brls::AlignItems::CENTER);
    auto* label = new brls::Label();
    label->setText("app/home/see_all"_i18n);
    label->setFontSize(18);
    seeAll->addView(label);
    seeAll->registerClickAction([this](brls::View*)
        {
            nuvio::ui::openSeeAll(row);
            return true;
        });
    items->addView(seeAll);
}

void CatalogRowView::releaseImages()
{
    if (released)
        return;
    released = true;
    for (auto* cell : cells)
        cell->releaseImage();
}

void CatalogRowView::restoreImages()
{
    if (!released)
        return;
    released = false;
    for (auto* cell : cells)
        cell->loadImage();
}
