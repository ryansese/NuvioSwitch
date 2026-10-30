#include "ui/streams/stream_card.hpp"

#include "data/streams/stream_rules.hpp"

using namespace brls::literals; // for _i18n
using namespace nuvio::data::streams;

// up to three lines of the description, as in NuvioTV's StreamCard
static constexpr size_t DESCRIPTION_LINES = 3;
// about a line and a half; longer lines wrap anyway
static constexpr size_t LINE_LIMIT = 160;

StreamCard::StreamCard()
{
    this->inflateFromXMLRes("xml/cells/stream_card.xml");
    this->registerClickAction([this](brls::View*)
        {
            if (onSelect)
                onSelect(this);
            return true;
        });
}

void StreamCard::setStream(const nuvio::domain::Stream& value)
{
    stream = value;

    std::string heading = displayName(stream);
    name->setText(heading.empty() ? "app/streams/unknown"_i18n : heading);

    std::string text;
    for (const auto& line : descriptionLines(displayDescription(stream), DESCRIPTION_LINES))
        text += (text.empty() ? "" : "\n") + (line.size() > LINE_LIMIT ? line.substr(0, LINE_LIMIT) + "…" : line);
    description->setText(text);
    description->setVisibility(text.empty() ? brls::Visibility::GONE : brls::Visibility::VISIBLE);

    std::string info = stream.addonName;
    if (stream.behaviorHints.videoSize)
    {
        std::string size = formatSize(*stream.behaviorHints.videoSize);
        if (!size.empty())
            info += "  ·  " + size;
    }
    meta->setText(info);

    NVGcolor dim = brls::Application::getTheme()["brls/text_disabled"];
    switch (classify(stream))
    {
        case Playability::Playable:
            badge->setVisibility(brls::Visibility::GONE);
            name->setTextColor(brls::Application::getTheme()["brls/text"]);
            break;
        case Playability::NeedsDebrid:
            badge->setText("app/streams/needs_debrid"_i18n);
            badge->setVisibility(brls::Visibility::VISIBLE);
            name->setTextColor(dim);
            break;
        case Playability::Unsupported:
            badge->setText("app/streams/unsupported"_i18n);
            badge->setVisibility(brls::Visibility::VISIBLE);
            name->setTextColor(dim);
            break;
    }
}
