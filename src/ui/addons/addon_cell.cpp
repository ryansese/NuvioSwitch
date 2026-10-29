#include "ui/addons/addon_cell.hpp"

#include "core/image/image_loader.hpp"

using namespace brls::literals; // for _i18n
using nuvio::core::image::ImageLoader;

static const char* const LOGO_PLACEHOLDER = "img/addon_logo.png";

// Roughly two lines at the cell's width; add-on descriptions can be paragraphs.
static constexpr size_t DESCRIPTION_LIMIT = 150;

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

AddonCell::AddonCell()
{
    this->inflateFromXMLRes("xml/cells/addon_cell.xml");

    this->registerAction("app/addons/disable"_i18n, brls::ControllerButton::BUTTON_X, [this](brls::View*)
        {
            if (onToggle)
                onToggle(this);
            return true;
        });
    this->registerAction("app/addons/move_up"_i18n, brls::ControllerButton::BUTTON_LB, [this](brls::View*)
        {
            if (onMove)
                onMove(this, -1);
            return true;
        });
    this->registerAction("app/addons/move_down"_i18n, brls::ControllerButton::BUTTON_RB, [this](brls::View*)
        {
            if (onMove)
                onMove(this, 1);
            return true;
        });
}

AddonCell::~AddonCell()
{
    if (ImageLoader::hasShared())
        ImageLoader::shared().cancel(logo);
}

void AddonCell::setAddon(const nuvio::domain::Addon& addon, bool loading)
{
    name->setText(addon.displayName);

    version->setVisibility(addon.placeholder ? brls::Visibility::GONE : brls::Visibility::VISIBLE);
    version->setText(brls::getStr("app/addons/version", addon.version));

    off->setVisibility(addon.enabled ? brls::Visibility::GONE : brls::Visibility::VISIBLE);
    this->updateActionHint(brls::ControllerButton::BUTTON_X,
        addon.enabled ? "app/addons/disable"_i18n : "app/addons/enable"_i18n);

    if (loading)
        description->setText("app/addons/loading"_i18n);
    else if (addon.placeholder)
        description->setText("app/addons/no_manifest"_i18n);
    else
        description->setText(shorten(addon.description.value_or(""), DESCRIPTION_LIMIT));

    if (addon.logo && !addon.logo->empty())
        ImageLoader::shared().load(logo, *addon.logo, LOGO_PLACEHOLDER);
    else
    {
        ImageLoader::shared().cancel(logo);
        logo->clear(); // drops a remote logo's texture from an earlier add-on
        logo->setImageFromRes(LOGO_PLACEHOLDER);
    }
}

void AddonCell::prepareForReuse()
{
    ImageLoader::shared().cancel(logo);
}

brls::RecyclerCell* AddonCell::create()
{
    return new AddonCell();
}

AddonActionCell::AddonActionCell()
{
    this->inflateFromXMLRes("xml/cells/addon_action_cell.xml");
}

brls::RecyclerCell* AddonActionCell::create()
{
    return new AddonActionCell();
}
