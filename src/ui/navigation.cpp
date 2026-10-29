#include "ui/navigation.hpp"

#include <borealis.hpp>

#include "ui/catalog/see_all_activity.hpp"

using namespace brls::literals; // for _i18n

namespace nuvio::ui
{

namespace
{
brls::Label* line(brls::Box* parent, const std::string& text, float fontSize, bool dim)
{
    auto* label = new brls::Label();
    label->setText(text);
    label->setFontSize(fontSize);
    label->setMarginBottom(12);
    if (dim)
        label->setTextColor(brls::Application::getTheme()["brls/text_disabled"]);
    parent->addView(label);
    return label;
}
} // namespace

void openDetails(const domain::MetaPreview& item, const std::string& sourceAddonName)
{
    // a focusable card, so B reaches the frame's back action
    auto* card = new brls::Box(brls::Axis::COLUMN);
    card->setFocusable(true);
    card->setCornerRadius(12);
    card->setBackgroundColor(brls::Application::getTheme()["nuvio/card"]);
    card->setPadding(40, 40, 40, 40);
    card->setMargins(48, 48, 48, 48);

    line(card, item.name, 32, false);
    line(card, brls::getStr("app/details_stub/type", item.type), 20, true);
    line(card, brls::getStr("app/details_stub/id", item.id), 20, true);
    line(card, brls::getStr("app/details_stub/source", sourceAddonName), 20, true);
    line(card, "app/details_stub/note"_i18n, 16, true)->setMarginTop(16);

    auto* frame = new brls::AppletFrame(card);
    frame->setTitle(item.name);
    brls::Application::pushActivity(new brls::Activity(frame));
}

void openSeeAll(const domain::CatalogRow& row)
{
    brls::Application::pushActivity(new CatalogSeeAllActivity(row));
}

} // namespace nuvio::ui
