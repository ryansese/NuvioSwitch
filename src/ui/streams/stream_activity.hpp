#pragma once

#include <borealis.hpp>
#include <string>
#include <vector>

#include "data/streams/stream_repository.hpp"
#include "domain/stream.hpp"
#include "domain/stream_request.hpp"

class StreamCard;

// The stream picker (NuvioTV ui/screens/stream/StreamScreen.kt): the title on the left,
// and on the right a row of add-on chips over the streams grouped by add-on. Every add-on
// that serves the video is asked at once and its group appears when it answers, in installed
// add-on order. Playable streams go to the player; the rest say why they can't.
// B returns to the details screen.
//
// The list is a ScrollingFrame of one box per add-on, not a RecyclerFrame: adding a group
// there keeps focus where it is, while RecyclerFrame can only reload (and reset) everything.
class StreamActivity : public brls::Activity
{
  public:
    explicit StreamActivity(const nuvio::domain::StreamRequest& request);
    ~StreamActivity() override;

    brls::View* createContentView() override;
    void onContentAvailable() override;

  private:
    class List;
    struct GroupView
    {
        size_t order = 0;
        std::string addonName;
        brls::Box* box          = nullptr;
        brls::Button* chip      = nullptr;
        StreamCard* firstCard   = nullptr;
        brls::Label* header     = nullptr;
    };

    void load();
    void reset();
    void addGroup(const nuvio::data::streams::StreamRepository::Group& group);
    void finished(const nuvio::data::streams::StreamRepository::Summary& summary);
    void showStatus(const std::string& message, const std::string& detail, bool retryable);
    void selectFilter(int index); // -1 is "All"
    void revealChip(size_t index);
    void select(StreamCard* card);
    void keepFocusStill();

    nuvio::domain::StreamRequest request;
    nuvio::data::streams::StreamRepository::Ticket ticket = 0;
    bool loading                                           = false;

    std::vector<GroupView> groups; // in order
    brls::Button* allChip = nullptr;
    int filter            = -1;
    float chipOffset      = 0;

    // the card that was focused when a group went in above it, and where it was
    brls::View* anchor = nullptr;
    float anchorY      = 0;

    brls::Image* backdrop        = nullptr;
    brls::Image* logo            = nullptr;
    brls::Label* title           = nullptr;
    brls::Label* episode         = nullptr;
    brls::Box* progress          = nullptr;
    brls::ProgressSpinner* spinner = nullptr;
    brls::Label* progressText    = nullptr;
    brls::Box* chipScroll        = nullptr;
    brls::Box* chips             = nullptr;
    brls::ScrollingFrame* scroll = nullptr;
    brls::Box* status            = nullptr;
    brls::Label* message         = nullptr;
    brls::Label* detail          = nullptr;
    brls::Button* retry          = nullptr;
    brls::Button* back           = nullptr;
    brls::Box* list              = nullptr;
};
