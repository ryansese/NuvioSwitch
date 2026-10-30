#include "ui/streams/stream_activity.hpp"

#include <algorithm>
#include <cmath>

#include "core/image/image_loader.hpp"
#include "core/log.hpp"
#include "data/addons/addon_repository.hpp"
#include "data/streams/stream_rules.hpp"
#include "domain/meta_format.hpp"
#include "ui/common/scrim.hpp"
#include "ui/common/viewport_box.hpp"
#include "ui/navigation.hpp"
#include "ui/streams/stream_card.hpp"

using namespace brls::literals; // for _i18n
using nuvio::core::image::ImageLoader;
using nuvio::data::addons::AddonRepository;
using nuvio::data::streams::Playability;
using nuvio::data::streams::StreamRepository;
using namespace nuvio::domain;

static const char* const TRANSPARENT = "img/transparent.png";

// The list's box. After a group goes in above the focused card, the first frame laid out
// puts the card elsewhere; the activity moves the scroll offset to cancel that.
class StreamActivity::List : public ViewportBox
{
  public:
    List(StreamActivity* owner, brls::View* viewport)
        : ViewportBox(brls::Axis::COLUMN, viewport)
        , owner(owner)
    {
    }

    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
        brls::FrameContext* ctx) override
    {
        owner->keepFocusStill();
        ViewportBox::draw(vg, x, y, width, height, style, ctx);
    }

  private:
    StreamActivity* owner;
};

StreamActivity::StreamActivity(const StreamRequest& request) : request(request) { }

StreamActivity::~StreamActivity()
{
    if (ticket && StreamRepository::hasShared())
        StreamRepository::shared().cancel(ticket);
    if (ImageLoader::hasShared())
    {
        ImageLoader::shared().cancel(backdrop);
        ImageLoader::shared().cancel(logo);
    }
}

brls::View* StreamActivity::createContentView()
{
    auto* content = static_cast<brls::Box*>(brls::View::createFromXMLResource("activity/streams.xml"));

    backdrop     = static_cast<brls::Image*>(content->getView("streams/backdrop"));
    logo         = static_cast<brls::Image*>(content->getView("streams/logo"));
    title        = static_cast<brls::Label*>(content->getView("streams/title"));
    episode      = static_cast<brls::Label*>(content->getView("streams/episode"));
    progress     = static_cast<brls::Box*>(content->getView("streams/progress"));
    spinner      = static_cast<brls::ProgressSpinner*>(content->getView("streams/spinner"));
    progressText = static_cast<brls::Label*>(content->getView("streams/progressText"));
    chipScroll   = static_cast<brls::Box*>(content->getView("streams/chipScroll"));
    chips        = static_cast<brls::Box*>(content->getView("streams/chips"));
    scroll       = static_cast<brls::ScrollingFrame*>(content->getView("streams/scroll"));
    status       = static_cast<brls::Box*>(content->getView("streams/status"));
    message      = static_cast<brls::Label*>(content->getView("streams/message"));
    detail       = static_cast<brls::Label*>(content->getView("streams/detail"));
    retry        = static_cast<brls::Button*>(content->getView("streams/retry"));
    back         = static_cast<brls::Button*>(content->getView("streams/back"));

    // the fade goes over the backdrop and under everything else
    content->addView(new Scrim(), 1);

    scroll->setScrollingBehavior(brls::ScrollingBehavior::CENTERED);
    list = new List(this, scroll);
    list->setPaddingBottom(24);
    scroll->setContentView(list);

    retry->registerClickAction([this](brls::View*)
        {
            load();
            return true;
        });
    back->registerClickAction([](brls::View*)
        {
            brls::Application::popActivity();
            return true;
        });

    ImageLoader::shared().load(backdrop, request.backdrop.value_or(request.poster.value_or("")), TRANSPARENT);
    bool hasLogo = request.logo && !request.logo->empty();
    logo->setVisibility(hasLogo ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
    title->setVisibility(hasLogo ? brls::Visibility::GONE : brls::Visibility::VISIBLE);
    if (hasLogo)
        ImageLoader::shared().load(logo, *request.logo, TRANSPARENT);
    title->setText(request.title);

    Video video;
    video.season  = request.season;
    video.episode = request.episode;
    video.title   = request.episodeTitle.value_or("");
    std::string line = request.season || request.episode || request.episodeTitle ? nuvio::domain::episodeHeading(video) : "";
    episode->setText(line);
    episode->setVisibility(line.empty() ? brls::Visibility::GONE : brls::Visibility::VISIBLE);

    auto* frame = new brls::AppletFrame(content);
    frame->setHeaderVisibility(brls::Visibility::GONE);
    return frame;
}

void StreamActivity::onContentAvailable()
{
    load();
}

// Clears the list and chips and asks every add-on again.
void StreamActivity::load()
{
    if (ticket)
        StreamRepository::shared().cancel(ticket);
    ticket = 0;
    reset();

    auto addons = AddonRepository::shared().addons();
    if (nuvio::data::streams::streamAddons(addons, request.contentType, request.videoId).empty())
    {
        showStatus("app/streams/no_addon"_i18n, "", false);
        return;
    }

    loading = true;
    progress->setVisibility(brls::Visibility::VISIBLE);
    spinner->animate(true);
    progressText->setText("app/streams/loading"_i18n);

    // an add-on may answer before fetchAll returns; ticket is only needed to cancel
    ticket = StreamRepository::shared().fetchAll(request.contentType, request.videoId, addons,
        [this](const StreamRepository::Group& group) { addGroup(group); },
        [this](const StreamRepository::Summary& summary) { finished(summary); });
    if (!loading)
        ticket = 0;
}

void StreamActivity::reset()
{
    loading = false;
    list->clearViews();
    chips->clearViews();
    groups.clear();
    allChip = nullptr;
    anchor  = nullptr;
    filter  = -1;
    chipOffset = 0;
    chips->setTranslationX(0);
    chipScroll->setVisibility(brls::Visibility::GONE);
    scroll->setVisibility(brls::Visibility::VISIBLE);
    status->setVisibility(brls::Visibility::GONE);
    progress->setVisibility(brls::Visibility::GONE);
    spinner->animate(false);
}

static brls::Button* makeChip(const std::string& text)
{
    auto* chip = new brls::Button();
    chip->setText(text);
    chip->setMarginRight(10);
    chip->setShrink(0); // a long row must not squeeze the labels away
    return chip;
}

void StreamActivity::addGroup(const StreamRepository::Group& group)
{
    bool first = groups.empty();

    GroupView view;
    view.order     = group.order;
    view.addonName = group.addonName;

    // its place among the groups already here
    size_t at = 0;
    while (at < groups.size() && groups[at].order < group.order)
        at++;

    view.box = new ViewportBox(brls::Axis::COLUMN, scroll);
    view.box->setMarginBottom(14);
    view.header = new brls::Label();
    view.header->setFontSize(18);
    view.header->setText(group.addonName);
    view.header->setTextColor(brls::Application::getTheme()["brls/text_disabled"]);
    view.header->setMarginBottom(8);
    view.header->setMarginLeft(4);
    view.box->addView(view.header);
    for (const auto& stream : group.streams)
    {
        auto* card = new StreamCard();
        card->setStream(stream);
        card->onSelect = [this](StreamCard* c) { select(c); };
        view.box->addView(card);
        if (!view.firstCard)
            view.firstCard = card;
    }

    // a group going in above the focused card pushes it down; remember where it was
    brls::View* focused = brls::Application::getCurrentFocus();
    if (focused && !anchor)
    {
        for (size_t i = at; i < groups.size() && !anchor; i++)
            for (brls::View* child : groups[i].box->getChildren())
                if (child == focused)
                {
                    anchor  = focused;
                    anchorY = focused->getY();
                    break;
                }
    }

    list->addView(view.box, at);
    view.header->setVisibility(filter < 0 ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
    if (filter >= 0)
        view.box->setVisibility(brls::Visibility::GONE);

    // chips: "All" with the first group, then one per add-on in group order
    if (first)
    {
        chipScroll->setVisibility(brls::Visibility::VISIBLE);
        allChip = makeChip("app/streams/all"_i18n);
        allChip->setStyle(&brls::BUTTONSTYLE_PRIMARY);
        allChip->registerClickAction([this](brls::View*)
            {
                selectFilter(-1);
                return true;
            });
        allChip->getFocusEvent()->subscribe([this](brls::View*) { revealChip(0); });
        chips->addView(allChip);
    }
    view.chip = makeChip(group.addonName);
    view.chip->registerClickAction([this, order = group.order](brls::View*)
        {
            for (size_t i = 0; i < groups.size(); i++)
                if (groups[i].order == order)
                    selectFilter(static_cast<int>(i));
            return true;
        });
    view.chip->getFocusEvent()->subscribe([this, order = group.order](brls::View*)
        {
            for (size_t i = 0; i < groups.size(); i++)
                if (groups[i].order == order)
                    revealChip(i + 1);
        });
    chips->addView(view.chip, at + 1);

    groups.insert(groups.begin() + at, std::move(view));
    // the filter index of a selected chip moves when a group goes in before it
    if (filter >= 0 && static_cast<int>(at) <= filter)
        filter++;

    if (first)
    {
        progressText->setText("app/streams/still_loading"_i18n);
        // focus the first stream, unless the user has already moved somewhere
        brls::View* current = brls::Application::getCurrentFocus();
        if (!current || current == back || current == retry)
            brls::Application::giveFocus(groups.front().firstCard);
    }
}

// Called every frame by the list: after a group went in above the focused card, scrolls
// by however far layout moved the card so that it stays where it was on screen.
void StreamActivity::keepFocusStill()
{
    if (!anchor)
        return;
    float moved = anchor->getY() - anchorY;
    anchor      = nullptr;
    if (std::fabs(moved) > 0.5f)
        scroll->setContentOffsetY(scroll->getContentOffsetY() + moved, false);
}

void StreamActivity::finished(const StreamRepository::Summary& summary)
{
    ticket  = 0;
    loading = false;
    progress->setVisibility(brls::Visibility::GONE);
    spinner->animate(false);

    if (summary.groups > 0)
        return;

    std::string names;
    for (const auto& name : summary.queried)
        names += (names.empty() ? "" : ", ") + name;
    showStatus("app/streams/none_found"_i18n, brls::getStr("app/streams/tried", names), true);
}

void StreamActivity::showStatus(const std::string& text, const std::string& more, bool retryable)
{
    scroll->setVisibility(brls::Visibility::GONE);
    chipScroll->setVisibility(brls::Visibility::GONE);
    status->setVisibility(brls::Visibility::VISIBLE);
    message->setText(text);
    detail->setText(more);
    detail->setVisibility(more.empty() ? brls::Visibility::GONE : brls::Visibility::VISIBLE);
    retry->setVisibility(retryable ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
    back->setVisibility(retryable ? brls::Visibility::GONE : brls::Visibility::VISIBLE);
    brls::Application::giveFocus(retryable ? static_cast<brls::View*>(retry) : back);
}

// Shows one add-on's streams (index into groups), or every group with -1.
void StreamActivity::selectFilter(int index)
{
    filter = index;
    allChip->setStyle(index < 0 ? &brls::BUTTONSTYLE_PRIMARY : &brls::BUTTONSTYLE_DEFAULT);
    for (size_t i = 0; i < groups.size(); i++)
    {
        bool shown = index < 0 || static_cast<int>(i) == index;
        groups[i].box->setVisibility(shown ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
        groups[i].header->setVisibility(index < 0 ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
        groups[i].chip->setStyle(static_cast<int>(i) == index ? &brls::BUTTONSTYLE_PRIMARY : &brls::BUTTONSTYLE_DEFAULT);
    }
    anchor = nullptr;
    scroll->setContentOffsetY(0, false);

    // the first stream of what is shown, once layout has caught up with the visibility change
    brls::sync([this]()
        {
            for (const auto& group : groups)
                if (group.box->getVisibility() == brls::Visibility::VISIBLE && group.firstCard)
                {
                    brls::Application::giveFocus(group.firstCard);
                    return;
                }
        });
}

// Slides the chips so that chip index (0 is "All") is fully inside the strip.
void StreamActivity::revealChip(size_t index)
{
    if (index >= chips->getChildren().size())
        return;
    constexpr float MARGIN = 12;
    brls::View* chip       = chips->getChildren()[index];
    float left             = chip->getLocalX();
    float right            = left + chip->getWidth();
    float visible          = chipScroll->getWidth();
    if (visible <= 0)
        return;
    if (left - chipOffset < MARGIN)
        chipOffset = std::max(0.0f, left - MARGIN);
    else if (right - chipOffset > visible - MARGIN)
        chipOffset = right - visible + MARGIN;
    chips->setTranslationX(-chipOffset);
}

void StreamActivity::select(StreamCard* card)
{
    const Stream& stream = card->getStream();
    switch (nuvio::data::streams::classify(stream))
    {
        case Playability::NeedsDebrid:
            brls::Application::notify("app/streams/needs_debrid_message"_i18n);
            return;
        case Playability::Unsupported:
            brls::Application::notify("app/streams/unsupported_message"_i18n);
            return;
        case Playability::Playable: break;
    }

    if (auto playback = nuvio::data::streams::playbackRequest(stream, request))
        nuvio::ui::openPlayer(*playback);
}
