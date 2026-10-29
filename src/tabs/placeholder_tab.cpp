#include "tabs/placeholder_tab.hpp"

#include "core/app_info.hpp"

PlaceholderTab::PlaceholderTab()
{
    this->inflateFromXMLRes("xml/tabs/placeholder.xml");

    this->registerStringXMLAttribute("titleKey", [this](std::string value)
        { this->title->setText(brls::getStr(value)); });

    this->registerStringXMLAttribute("noteKey", [this](std::string value)
        { this->note->setText(brls::getStr(value)); });

    this->version->setText(nuvio::appVersionText());
    this->registerBoolXMLAttribute("showVersion", [this](bool value)
        { this->version->setVisibility(value ? brls::Visibility::VISIBLE : brls::Visibility::GONE); });
}

brls::View* PlaceholderTab::create()
{
    return new PlaceholderTab();
}
