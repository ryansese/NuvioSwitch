#pragma once

#include <borealis.hpp>
#include <memory>
#include <vector>

#include "data/progress/continue_watching.hpp"

class ContinueCard;

// Home's "Continue Watching" row (NuvioSwitch-0009): in-progress movies and episodes with a progress
// bar, and "Next up" episodes, newest activity first. It loads itself from the progress repository and
// reloads when that changes; it is hidden while it has nothing to show.
//
// A plays (in-progress items resume), X opens the options: Start from beginning, Go to details, Remove.
class ContinueWatchingRow : public brls::Box
{
  public:
    ContinueWatchingRow();
    ~ContinueWatchingRow() override;

    bool isEmpty() const { return items.empty(); }

    // Called when the row is about to lose its last card while one has focus: Home moves focus elsewhere.
    std::function<void(ContinueWatchingRow*)> onFocusLeaving;

  private:
    void reload();
    void setItems(const std::vector<nuvio::data::progress::ContinueItem>& value);
    void play(const nuvio::data::progress::ContinueItem& item, bool fromBeginning);
    void showOptions(const nuvio::data::progress::ContinueItem& item);
    void removeItem(const nuvio::data::progress::ContinueItem& item);

    std::unique_ptr<nuvio::data::progress::ContinueWatchingLoader> loader;
    std::vector<nuvio::data::progress::ContinueItem> items;
    std::vector<ContinueCard*> cards;
    brls::HScrollingFrame* scroller = nullptr;
    brls::Box* list                 = nullptr;
    int subscription                = 0;
};
