#include "ui/catalog/see_all_activity.hpp"

#include "data/catalog/catalog_repository.hpp"
#include "data/catalog/home_selection.hpp"
#include "ui/common/poster_cell.hpp"
#include "ui/common/viewport_box.hpp"
#include "ui/navigation.hpp"

using namespace brls::literals; // for _i18n
using nuvio::data::catalog::CatalogRepository;
using namespace nuvio::domain;

static constexpr float CELL_GAP = 16;

// The grid's box: it draws only the lines on screen, and each frame lets the
// activity load textures near the screen and release the rest.
class CatalogSeeAllActivity::Grid : public ViewportBox
{
  public:
    Grid(CatalogSeeAllActivity* owner, brls::View* viewport)
        : ViewportBox(brls::Axis::COLUMN, viewport)
        , owner(owner)
    {
    }

    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
        brls::FrameContext* ctx) override
    {
        owner->updateTextures();
        ViewportBox::draw(vg, x, y, width, height, style, ctx);
    }

  private:
    CatalogSeeAllActivity* owner;
};

CatalogSeeAllActivity::CatalogSeeAllActivity(const CatalogRow& source) : row(source) { }

CatalogSeeAllActivity::~CatalogSeeAllActivity()
{
    request.cancel();
}

size_t CatalogSeeAllActivity::columns() const
{
    PosterShape shape = row.items.empty() ? PosterShape::Poster : row.items.front().posterShape;
    switch (shape)
    {
        case PosterShape::Landscape: return 4;
        case PosterShape::Square: return 5;
        default: return 6;
    }
}

brls::View* CatalogSeeAllActivity::createContentView()
{
    scroll = new brls::ScrollingFrame();
    scroll->setScrollingBehavior(brls::ScrollingBehavior::CENTERED);
    grid = new Grid(this, scroll);
    grid->setPadding(24, 48, 32, 48);
    grid->setAlignItems(brls::AlignItems::CENTER);
    scroll->setContentView(grid);

    status = new brls::Label();
    status->setFontSize(16);
    status->setTextColor(brls::Application::getTheme()["brls/text_disabled"]);
    status->setMarginTop(12);
    status->setVisibility(brls::Visibility::GONE);
    grid->addView(status);

    addItems(0);

    auto* frame = new brls::AppletFrame(scroll);
    frame->setTitle(nuvio::data::catalog::rowTitle(row.source.catalog, "app/home/type_movie"_i18n,
                        "app/home/type_series"_i18n) +
                    "  ·  " + brls::getStr("app/home/from", row.source.addonName));
    return frame;
}

void CatalogSeeAllActivity::addItems(size_t from)
{
    size_t perLine = columns();
    for (size_t i = from; i < row.items.size(); i++)
    {
        size_t lineIndex = i / perLine;
        if (lineIndex == lines.size())
        {
            auto* line = new brls::Box(brls::Axis::ROW);
            line->setMarginBottom(24);
            line->setWidth(perLine * (PosterCell::widthFor(row.items[i].posterShape) + CELL_GAP));
            // before the status label, which stays last
            grid->addView(line, grid->getChildren().size() - 1);
            lines.push_back(line);
            lineCells.emplace_back();
            lineReleased.push_back(false);
        }

        auto* cell = new PosterCell();
        cell->setMarginRight(CELL_GAP);
        cell->setItem(row.items[i]);
        cell->onSelect = [this](PosterCell* c) { nuvio::ui::openDetails(c->getItem(), row.source.addonName); };
        cell->onFocus  = [this, lineIndex](PosterCell*)
        {
            if (lineIndex + LOAD_MORE_LINES >= lines.size())
                loadMore();
        };
        lines[lineIndex]->addView(cell);
        lineCells[lineIndex].push_back(cell);
    }
}

void CatalogSeeAllActivity::loadMore()
{
    if (loadingMore || !row.hasMore)
        return;
    loadingMore = true;
    status->setText("app/home/loading_row"_i18n);
    status->setVisibility(brls::Visibility::VISIBLE);

    request = CatalogRepository::shared().load(row.source, row.nextSkip, {},
        [this](CatalogRepository::Result result)
        {
            loadingMore = false;
            status->setVisibility(brls::Visibility::GONE);
            if (!result.ok)
            {
                // stop here; reopening See all tries again
                row.hasMore = false;
                brls::Application::notify(brls::getStr("app/home/page_failed", result.error));
                return;
            }

            size_t from = row.items.size();
            for (auto& item : result.page.items)
            {
                bool known = false;
                for (const auto& existing : row.items)
                    known = known || existing.id == item.id;
                if (!known)
                    row.items.push_back(std::move(item));
            }
            row.nextSkip = result.page.nextSkip;
            row.hasMore  = result.page.hasMore;
            addItems(from);
        });
}

void CatalogSeeAllActivity::updateTextures()
{
    float top    = scroll->getContentOffsetY();
    float bottom = top + scroll->getHeight();
    float far    = RELEASE_DISTANCE * scroll->getHeight();

    for (size_t i = 0; i < lines.size(); i++)
    {
        float lineTop    = lines[i]->getLocalY();
        float lineBottom = lineTop + lines[i]->getHeight();
        bool distant     = lineBottom < top - far || lineTop > bottom + far;
        if (distant == lineReleased[i])
            continue;
        lineReleased[i] = distant;
        for (auto* cell : lineCells[i])
        {
            if (distant)
                cell->releaseImage();
            else
                cell->loadImage();
        }
    }
}
