#include "tabs/placeholder_tab.hpp"

#include "core/app_info.hpp"
#include "tabs/debug_poster_row.hpp"

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

    // registered in every build so the XML stays valid; the row only exists in debug network builds
    this->registerBoolXMLAttribute("debugPosters", [this](bool value)
        {
#ifdef NUVIO_DEBUG_NETWORK
            if (value)
                this->addView(new DebugPosterRow());
#endif
        });
}

brls::View* PlaceholderTab::create()
{
    return new PlaceholderTab();
}
