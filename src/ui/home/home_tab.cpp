#include "ui/home/home_tab.hpp"

#include <unordered_map>

#include "data/addons/addon_repository.hpp"
#include "data/catalog/home_selection.hpp"
#include "ui/common/viewport_box.hpp"
#include "ui/home/catalog_row_view.hpp"
#include "ui/home/hero_carousel.hpp"

using namespace brls::literals; // for _i18n
using nuvio::data::addons::AddonRepository;
using namespace nuvio::domain;
namespace catalog = nuvio::data::catalog;

namespace
{
// Rows that loaded this session, by catalog key, so a recreated Home shows them at once.
std::unordered_map<std::string, CatalogRow>& loadedRows()
{
    static std::unordered_map<std::string, CatalogRow> rows;
    return rows;
}

brls::Box* makeButton(brls::Label*& label)
{
    auto* button = new brls::Box(brls::Axis::ROW);
    button->setFocusable(true);
    button->setCornerRadius(8);
    button->setHighlightCornerRadius(8);
    button->setBackgroundColor(brls::Application::getTheme()["nuvio/card"]);
    button->setPadding(14, 28, 14, 28);
    button->setMarginTop(24);
    label = new brls::Label();
    label->setFontSize(18);
    button->addView(label);
    return button;
}
} // namespace

HomeTab::HomeTab()
{
    this->setAxis(brls::Axis::COLUMN);

    // B (Escape on desktop) in Home's content goes back to Home's sidebar item.
    // TabFrame adds its own B to every tab, but in touch mode (any mouse movement
    // on desktop) that one calls dismiss(), which on the root screen opens the
    // "exit this app" dialog. Registered first, this one runs instead.
    this->registerAction(
        "hints/back"_i18n, brls::ControllerButton::BUTTON_B,
        [this](brls::View*)
        {
            for (brls::View* view = this->getParent(); view; view = view->getParent())
                if (auto* tabs = dynamic_cast<brls::TabFrame*>(view))
                {
                    tabs->focusTab(HOME_TAB_INDEX);
                    break;
                }
            return true;
        },
        false, false, brls::SOUND_BACK);

    scroll = new brls::ScrollingFrame();
    scroll->setGrow(1);
    scroll->setScrollingBehavior(brls::ScrollingBehavior::CENTERED);
    content = new ViewportBox(brls::Axis::COLUMN, scroll);
    content->setPadding(32, 48, 32, 48);
    scroll->setContentView(content);
    this->addView(scroll);

    hero = new HeroCarousel();
    content->addView(hero);

    stateBox = new brls::Box(brls::Axis::COLUMN);
    stateBox->setAlignItems(brls::AlignItems::CENTER);
    stateBox->setMarginTop(120);
    stateText = new brls::Label();
    stateText->setFontSize(20);
    stateText->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    stateBox->addView(stateText);
    stateButton = makeButton(stateButtonText);
    stateButton->registerClickAction([this](brls::View*)
        {
            if (state == State::Error)
            {
                rebuild(true);
                return true;
            }
            // no catalogs: go to the Add-ons tab
            for (brls::View* view = this->getParent(); view; view = view->getParent())
                if (auto* tabs = dynamic_cast<brls::TabFrame*>(view))
                {
                    tabs->focusTab(ADDONS_TAB_INDEX);
                    break;
                }
            return true;
        });
    stateBox->addView(stateButton);
    content->addView(stateBox);

    subscription = AddonRepository::shared().subscribe([this] { rebuild(); });
    rebuild(true);
}

HomeTab::~HomeTab()
{
    if (AddonRepository::hasShared())
        AddonRepository::shared().unsubscribe(subscription);
}

brls::View* HomeTab::create()
{
    return new HomeTab();
}

bool HomeTab::hasFocusInside() const
{
    for (brls::View* view = brls::Application::getCurrentFocus(); view; view = view->getParent())
        if (view == this)
            return true;
    return false;
}

void HomeTab::rebuild(bool force)
{
    auto sources = catalog::selectHomeCatalogs(AddonRepository::shared().addons());
    std::vector<std::string> newKeys;
    for (const auto& source : sources)
        newKeys.push_back(source.key());
    // only a change in which catalogs show, or their order, rebuilds
    if (!force && newKeys == keys)
        return;
    keys = newKeys;

    // focus can't stay on a row that is about to be deleted: park it on the state
    // button, which updateState() moves on from once there is content
    bool focused = hasFocusInside();
    if (focused)
    {
        stateBox->setVisibility(brls::Visibility::VISIBLE);
        stateButton->setVisibility(brls::Visibility::VISIBLE);
        brls::Application::giveFocus(stateButton);
    }

    for (auto* row : rows)
        content->removeView(row); // cancels its request
    rows.clear();
    content->setLastFocusedView(nullptr); // removeView leaves it pointing at a deleted row

    for (size_t i = 0; i < sources.size(); i++)
    {
        auto* row = new CatalogRowView(sources[i],
            catalog::rowTitle(sources[i].catalog, "app/home/type_movie"_i18n, "app/home/type_series"_i18n));
        row->onFirstPage = [this](CatalogRowView* r) { onRowFirstPage(r); };
        row->onPageAdded = [](CatalogRowView* r) { loadedRows()[r->getRow().source.key()] = r->getRow(); };
        // before the state box, which stays last
        content->addView(row, content->getChildren().size() - 1);
        rows.push_back(row);

        auto cached = loadedRows().find(newKeys[i]);
        if (cached != loadedRows().end())
            row->show(cached->second);
        else if (i < EAGER_ROWS)
            row->load();
    }

    updateHero();
    updateState();
}

void HomeTab::onRowFirstPage(CatalogRowView* row)
{
    if (row->getState() == CatalogRowView::State::Loaded)
        loadedRows()[row->getRow().source.key()] = row->getRow();
    // the hero follows first pages only, so it doesn't jump on load-more
    updateHero();
    updateState();
}

void HomeTab::updateHero()
{
    std::vector<std::vector<MetaPreview>> loaded;
    std::unordered_map<std::string, std::string> names; // addon base URL -> name
    for (auto* row : rows)
        if (row->getState() == CatalogRowView::State::Loaded)
        {
            loaded.push_back(row->getRow().items);
            names[row->getRow().source.addonBaseUrl] = row->getRow().source.addonName;
        }

    auto items = catalog::pickHeroItems(loaded);
    std::vector<std::string> sourceNames;
    for (const auto& item : items)
        sourceNames.push_back(names[item.sourceAddonBaseUrl]);

    bool wasFocused = hero->isFocused();
    hero->setItems(std::move(items), std::move(sourceNames));
    // a hidden hero can't keep focus: the first visible focusable view takes it
    if (wasFocused && hero->getVisibility() == brls::Visibility::GONE)
        brls::Application::giveFocus(this);
}

void HomeTab::updateState()
{
    bool anyLoaded = false;
    bool allFailed = !rows.empty();
    for (auto* row : rows)
    {
        anyLoaded = anyLoaded || row->getState() == CatalogRowView::State::Loaded;
        allFailed = allFailed && row->getState() == CatalogRowView::State::Failed;
    }

    if (rows.empty())
    {
        // the add-ons may still be fetching their first manifests
        bool fetching = false;
        for (const auto& addon : AddonRepository::shared().addons())
            fetching = fetching || (addon.enabled && AddonRepository::shared().isLoading(addon.baseUrl));
        showState(fetching ? State::Loading : State::NoCatalogs);
    }
    else if (anyLoaded)
        showState(State::Content);
    else if (allFailed)
        showState(State::Error);
    else
        showState(State::Loading);
}

void HomeTab::showState(State newState)
{
    state = newState;
    if (state == State::Content)
    {
        bool parked = brls::Application::getCurrentFocus() == stateButton;
        stateBox->setVisibility(brls::Visibility::GONE);
        if (parked)
        {
            // content remembers the state box as its last focus; start from the top instead
            content->setLastFocusedView(nullptr);
            brls::Application::giveFocus(this);
        }
        return;
    }

    stateBox->setVisibility(brls::Visibility::VISIBLE);
    switch (state)
    {
        case State::Loading: stateText->setText("app/home/loading"_i18n); break;
        case State::NoCatalogs:
            stateText->setText("app/home/no_catalogs"_i18n);
            stateButtonText->setText("app/home/open_addons"_i18n);
            break;
        case State::Error:
            stateText->setText("app/home/error"_i18n);
            stateButtonText->setText("app/home/retry"_i18n);
            break;
        default: break;
    }
    bool button = state == State::NoCatalogs || state == State::Error;
    // while loading nothing else is focusable; focus stays on the hidden button,
    // where B still reaches the tab's back action
    stateButton->setVisibility(button ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
}

void HomeTab::loadNearbyRows()
{
    float top    = scroll->getContentOffsetY();
    float bottom = top + scroll->getHeight();
    float reach  = CatalogRowView::PENDING_HEIGHT;

    for (auto* row : rows)
    {
        if (row->getState() == CatalogRowView::State::Failed)
            continue;
        // before the first layout every row sits at 0 with no height and would look on screen
        if (row->getHeight() <= 0 || scroll->getHeight() <= 0)
            continue;
        float rowTop    = row->getLocalY();
        float rowBottom = rowTop + row->getHeight();

        // within one row of the screen
        if (row->getState() == CatalogRowView::State::Pending && rowTop < bottom + reach && rowBottom > top - reach)
            row->load();

        float far = RELEASE_DISTANCE * reach;
        if (rowBottom < top - far || rowTop > bottom + far)
            row->releaseImages();
        else
            row->restoreImages();
    }
}

void HomeTab::draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
    brls::FrameContext* ctx)
{
    loadNearbyRows();
    Box::draw(vg, x, y, width, height, style, ctx);
}
