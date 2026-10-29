#include "ui/home/hero_carousel.hpp"

#include <cstdio>

#include "core/image/image_loader.hpp"
#include "ui/navigation.hpp"

using nuvio::core::image::ImageLoader;
using nuvio::domain::MetaPreview;

static const char* const TRANSPARENT = "img/transparent.png";
static constexpr size_t DESCRIPTION_LIMIT = 220;

// text cut to about limit bytes on a UTF-8 boundary, with an ellipsis.
static std::string shorten(const std::string& text, size_t limit)
{
    if (text.size() <= limit)
        return text;
    size_t end = limit;
    while (end > 0 && (static_cast<unsigned char>(text[end]) & 0xC0) == 0x80)
        end--;
    while (end > 0 && text[end - 1] == ' ')
        end--;
    return text.substr(0, end) + "…";
}

HeroCarousel::HeroCarousel()
{
    this->setAxis(brls::Axis::COLUMN);
    this->setHeight(HEIGHT);
    this->setMarginBottom(28);
    this->setFocusable(true);
    this->setCornerRadius(12);
    this->setHighlightCornerRadius(12);
    this->setHideHighlightBackground(true);
    this->setBackgroundColor(brls::Application::getTheme()["nuvio/card"]);
    this->setVisibility(brls::Visibility::GONE);

    // full-bleed, dimmed so the text stays readable. Pinned to all four edges of a
    // box without padding: with padding, Yoga insets absolute children by it.
    background = new brls::Image();
    background->setPositionType(brls::PositionType::ABSOLUTE);
    background->setPositionTop(0);
    background->setPositionLeft(0);
    background->setPositionRight(0);
    background->setPositionBottom(0);
    background->setScalingType(brls::ImageScalingType::FILL);
    background->setCornerRadius(12);
    background->setAlpha(0.45f);
    this->addView(background);

    // the text, bottom-aligned with the padding the hero itself can't have
    auto* body = new brls::Box(brls::Axis::COLUMN);
    body->setGrow(1);
    body->setJustifyContent(brls::JustifyContent::FLEX_END);
    body->setPadding(32, 40, 28, 40);
    this->addView(body);

    logo = new brls::Image();
    logo->setScalingType(brls::ImageScalingType::FIT);
    logo->setMaxWidth(380);
    logo->setHeight(90);
    logo->setMarginBottom(12);
    body->addView(logo);

    name = new brls::Label();
    name->setFontSize(34);
    name->setMarginBottom(12);
    body->addView(name);

    meta = new brls::Label();
    meta->setFontSize(16);
    meta->setMarginBottom(10);
    body->addView(meta);

    description = new brls::Label();
    description->setFontSize(16);
    description->setMaxWidth(640);
    description->setTextColor(brls::Application::getTheme()["brls/text_disabled"]);
    body->addView(description);

    position = new brls::Label();
    position->setFontSize(14);
    position->setMarginTop(12);
    position->setTextColor(brls::Application::getTheme()["brls/text_disabled"]);
    body->addView(position);

    // the NAV buttons, not BUTTON_LEFT/RIGHT: a d-pad press also fires NAV_*, which
    // would move focus after the item changed. NAV_* covers the sticks too.
    this->registerAction("", brls::ControllerButton::BUTTON_NAV_LEFT, [this](brls::View*) { return step(-1); }, true, true);
    this->registerAction("", brls::ControllerButton::BUTTON_NAV_RIGHT, [this](brls::View*) { return step(1); }, true, true);
    this->registerClickAction([this](brls::View*)
        {
            if (current < items.size())
                nuvio::ui::openDetails(items[current], sourceNames[current]);
            return true;
        });
}

HeroCarousel::~HeroCarousel()
{
    if (!ImageLoader::hasShared())
        return;
    ImageLoader::shared().cancel(background);
    ImageLoader::shared().cancel(logo);
}

void HeroCarousel::setItems(std::vector<MetaPreview> newItems, std::vector<std::string> newSourceNames)
{
    // stay on the same item when it is still there
    std::string currentId = current < items.size() ? items[current].id : "";
    items                 = std::move(newItems);
    sourceNames           = std::move(newSourceNames);
    current               = 0;
    for (size_t i = 0; i < items.size(); i++)
        if (items[i].id == currentId)
            current = i;

    if (items.empty())
    {
        // the hero can't keep focus once it's gone; Home moves focus first
        this->setVisibility(brls::Visibility::GONE);
        return;
    }
    this->setVisibility(brls::Visibility::VISIBLE);
    showItem(current);
}

void HeroCarousel::showItem(size_t index)
{
    current           = index;
    shownAt           = std::chrono::steady_clock::now();
    const auto& item  = items[index];

    ImageLoader::shared().load(background, item.background.value_or(item.poster.value_or("")), TRANSPARENT);

    bool hasLogo = item.logo && !item.logo->empty();
    logo->setVisibility(hasLogo ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
    name->setVisibility(hasLogo ? brls::Visibility::GONE : brls::Visibility::VISIBLE);
    if (hasLogo)
        ImageLoader::shared().load(logo, *item.logo, TRANSPARENT);
    else
        ImageLoader::shared().cancel(logo);
    name->setText(item.name);

    std::string line = item.releaseInfo.value_or("");
    if (item.imdbRating)
    {
        char rating[16];
        std::snprintf(rating, sizeof(rating), "%.1f", *item.imdbRating);
        line += std::string(line.empty() ? "" : "   ") + "★ " + rating;
    }
    meta->setText(line);
    meta->setVisibility(line.empty() ? brls::Visibility::GONE : brls::Visibility::VISIBLE);

    description->setText(shorten(item.description.value_or(""), DESCRIPTION_LIMIT));
    position->setText(std::to_string(index + 1) + " / " + std::to_string(items.size()));
}

bool HeroCarousel::step(int delta)
{
    long next = static_cast<long>(current) + delta;
    // at either end, let the press move focus as usual
    if (next < 0 || next >= static_cast<long>(items.size()))
        return false;
    showItem(static_cast<size_t>(next));
    return true;
}

void HeroCarousel::draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
    brls::FrameContext* ctx)
{
    if (items.size() > 1 && !this->isFocused() && std::chrono::steady_clock::now() - shownAt >= ADVANCE_EVERY)
        showItem((current + 1) % items.size());
    Box::draw(vg, x, y, width, height, style, ctx);
}
