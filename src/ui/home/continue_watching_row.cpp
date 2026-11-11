#include "ui/home/continue_watching_row.hpp"

#include "core/image/image_loader.hpp"
#include "data/addons/addon_repository.hpp"
#include "data/meta/meta_repository.hpp"
#include "data/progress/progress_repository.hpp"
#include "ui/common/options_menu.hpp"
#include "ui/common/today.hpp"
#include "ui/common/viewport_box.hpp"
#include "ui/navigation.hpp"

using namespace brls::literals; // for _i18n
using nuvio::core::image::ImageLoader;
using nuvio::data::addons::AddonRepository;
using nuvio::data::meta::MetaRepository;
using nuvio::data::progress::ContinueItem;
using nuvio::data::progress::ProgressRepository;
namespace progress = nuvio::data::progress;

namespace
{
constexpr float CARD_WIDTH  = 224; // NuvioTV's landscape card
constexpr float CARD_HEIGHT = 126;
constexpr float BAR_HEIGHT  = 4;

// "S1E4 · Title" for episodes; for in-progress movies the time left
std::string subtitleOf(const ContinueItem& item)
{
    const auto& p = item.progress;
    if (p.season && p.episode)
    {
        std::string text = "S" + std::to_string(*p.season) + "E" + std::to_string(*p.episode);
        if (p.episodeTitle && !p.episodeTitle->empty())
            text += " · " + *p.episodeTitle;
        return text;
    }
    if (!item.nextUp && p.duration > 0)
        return brls::getStr("app/home/time_left", progress::shortDuration(progress::remainingMs(p)));
    return "";
}
} // namespace

// One item: landscape artwork with a progress bar or a "Next up" badge, its title and a second line.
class ContinueCard : public brls::Box
{
  public:
    ContinueCard()
    {
        this->setAxis(brls::Axis::COLUMN);
        this->setWidth(CARD_WIDTH);
        this->setMarginRight(16);

        art = new brls::Box();
        art->setFocusable(true);
        art->setDimensions(CARD_WIDTH, CARD_HEIGHT);
        art->setCornerRadius(12);
        art->setHighlightCornerRadius(12);
        art->setHideHighlightBackground(true);
        art->setBackgroundColor(brls::Application::getTheme()["nuvio/card"]);
        this->addView(art);

        image = new brls::Image();
        image->setScalingType(brls::ImageScalingType::FILL);
        image->setCornerRadius(12);
        image->setDimensions(CARD_WIDTH, CARD_HEIGHT);
        art->addView(image);

        badge = new brls::Box();
        badge->setPositionType(brls::PositionType::ABSOLUTE);
        badge->setPositionTop(8);
        badge->setPositionLeft(8);
        badge->setBackgroundColor(nvgRGBA(0, 0, 0, 0xB0));
        badge->setPadding(3, 8, 3, 8);
        badge->setCornerRadius(6);
        auto* badgeText = new brls::Label();
        badgeText->setText("app/home/next_up"_i18n);
        badgeText->setFontSize(13);
        badge->addView(badgeText);
        art->addView(badge);

        bar = new brls::Box();
        bar->setPositionType(brls::PositionType::ABSOLUTE);
        bar->setPositionLeft(10);
        bar->setPositionRight(10);
        bar->setPositionBottom(8);
        bar->setHeight(BAR_HEIGHT);
        bar->setCornerRadius(BAR_HEIGHT / 2);
        bar->setBackgroundColor(nvgRGBA(0xFF, 0xFF, 0xFF, 0x50));
        fill = new brls::Box();
        fill->setHeight(BAR_HEIGHT);
        fill->setCornerRadius(BAR_HEIGHT / 2);
        fill->setBackgroundColor(nvgRGB(0xFF, 0xFF, 0xFF));
        bar->addView(fill);
        art->addView(bar);

        title = new brls::Label();
        title->setFontSize(15);
        title->setSingleLine(true);
        title->setMarginTop(8);
        title->setWidth(CARD_WIDTH);
        this->addView(title);

        subtitle = new brls::Label();
        subtitle->setFontSize(13);
        subtitle->setSingleLine(true);
        subtitle->setWidth(CARD_WIDTH);
        subtitle->setTextColor(brls::Application::getTheme()["brls/text_disabled"]);
        this->addView(subtitle);
    }

    ~ContinueCard() override
    {
        if (ImageLoader::hasShared())
            ImageLoader::shared().cancel(image);
    }

    void setItem(const ContinueItem& item)
    {
        const auto& p = item.progress;
        title->setText(p.name);
        std::string second = subtitleOf(item);
        subtitle->setText(second.empty() ? " " : second);
        badge->setVisibility(item.nextUp ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
        bar->setVisibility(item.nextUp ? brls::Visibility::GONE : brls::Visibility::VISIBLE);
        fill->setWidth((CARD_WIDTH - 20) * progress::progressPercentage(p));
        ImageLoader::shared().load(image, p.backdrop.value_or(p.poster.value_or("")));
    }

    brls::Box* focusTarget() const { return art; }

  private:
    brls::Box* art;
    brls::Image* image;
    brls::Box* badge;
    brls::Box* bar;
    brls::Box* fill;
    brls::Label* title;
    brls::Label* subtitle;
};

ContinueWatchingRow::ContinueWatchingRow()
{
    this->setAxis(brls::Axis::COLUMN);
    this->setMarginBottom(28);
    this->setVisibility(brls::Visibility::GONE);

    auto* heading = new brls::Label();
    heading->setText("app/home/continue_watching"_i18n);
    heading->setFontSize(22);
    heading->setMarginBottom(12);
    this->addView(heading);

    scroller = new brls::HScrollingFrame();
    scroller->setScrollingIndicatorVisible(false);
    // artwork, two lines of text and room for the highlight
    scroller->setHeight(CARD_HEIGHT + 60);
    list = new ViewportBox(brls::Axis::ROW, scroller);
    list->setAlignItems(brls::AlignItems::FLEX_START);
    list->setPadding(6, 6, 6, 6);
    scroller->setContentView(list);
    this->addView(scroller);

    loader = std::make_unique<progress::ContinueWatchingLoader>(
        [](const progress::NextUpSeed& seed, progress::ContinueWatchingLoader::MetaCallback done)
        {
            std::string type = seed.contentType.empty() ? "series" : seed.contentType;
            return MetaRepository::shared().lookup(type, seed.contentId, "", AddonRepository::shared().addons(),
                [done](MetaRepository::Result result) { done(result.ok ? &result.meta : nullptr); });
        },
        [](uint64_t ticket)
        {
            if (MetaRepository::hasShared())
                MetaRepository::shared().cancel(ticket);
        });

    subscription = ProgressRepository::shared().subscribe([this] { reload(); });
    reload();
}

ContinueWatchingRow::~ContinueWatchingRow()
{
    if (ProgressRepository::hasShared())
        ProgressRepository::shared().unsubscribe(subscription);
    loader.reset(); // cancels its lookups while the row still exists
}

void ContinueWatchingRow::reload()
{
    auto& repo = ProgressRepository::shared();
    loader->start(repo.entries(), repo.watched(), repo.hiddenSeries(), nuvio::ui::today(),
        [this](const std::vector<ContinueItem>& value) { setItems(value); });
}

// Cards are reused and updated in place rather than rebuilt, so the focused one is never deleted under
// the focus; focus follows its title when the order changes.
void ContinueWatchingRow::setItems(const std::vector<ContinueItem>& value)
{
    std::string focusedKey;
    bool hadFocus = false;
    for (size_t i = 0; i < cards.size(); i++)
        if (cards[i]->focusTarget()->isFocused())
        {
            hadFocus   = true;
            focusedKey = progress::progressKey(items[i].progress);
        }

    items = value;

    for (size_t i = cards.size(); i < items.size(); i++)
    {
        auto* card = new ContinueCard();
        card->focusTarget()->registerClickAction([this, i](brls::View*)
            {
                if (i < items.size())
                    play(items[i], false);
                return true;
            });
        card->focusTarget()->registerAction("app/home/options"_i18n, brls::ControllerButton::BUTTON_X,
            [this, i](brls::View*)
            {
                if (i < items.size())
                    showOptions(items[i]);
                return true;
            });
        list->addView(card);
        cards.push_back(card);
    }
    for (size_t i = 0; i < items.size(); i++)
        cards[i]->setItem(items[i]);

    if (hadFocus)
    {
        if (items.empty())
        {
            if (onFocusLeaving)
                onFocusLeaving(this);
        }
        else
        {
            size_t target = items.size() - 1;
            for (size_t i = 0; i < items.size(); i++)
                if (progress::progressKey(items[i].progress) == focusedKey)
                {
                    target = i;
                    break;
                }
            brls::Application::giveFocus(cards[target]->focusTarget());
        }
    }
    while (cards.size() > items.size())
    {
        list->removeView(cards.back());
        cards.pop_back();
    }
    list->setLastFocusedView(nullptr);

    this->setVisibility(items.empty() ? brls::Visibility::GONE : brls::Visibility::VISIBLE);
}

void ContinueWatchingRow::play(const ContinueItem& item, bool fromBeginning)
{
    nuvio::ui::openStreams(progress::streamRequestFor(item.progress, fromBeginning));
}

void ContinueWatchingRow::showOptions(const ContinueItem& item)
{
    enum
    {
        START_OVER,
        DETAILS,
        REMOVE,
    };
    std::vector<std::string> options{"app/home/start_over"_i18n, "app/home/go_to_details"_i18n, "app/home/remove"_i18n};
    nuvio::ui::showOptions(item.progress.name, options, 0,
        [this, item](int chosen)
        {
            switch (chosen)
            {
                case START_OVER: play(item, true); break;
                case DETAILS: nuvio::ui::openDetails(progress::previewFor(item.progress), ""); break;
                case REMOVE: removeItem(item); break;
                default: break;
            }
        });
}

void ContinueWatchingRow::removeItem(const ContinueItem& item)
{
    auto& repo = ProgressRepository::shared();
    if (item.nextUp)
        repo.hideSeries(item.progress.contentId);
    else
        repo.remove(progress::progressKey(item.progress));
}
