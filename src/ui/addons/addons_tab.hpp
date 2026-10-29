#pragma once

#include <borealis.hpp>

// The Add-ons tab: "Install add-on" and "Refresh add-ons", then the installed
// add-ons in priority order. A on an add-on opens its actions; X toggles it and
// L/R move it. It redraws whenever AddonRepository reports a change.
class AddonsTab : public brls::Box
{
  public:
    AddonsTab();
    ~AddonsTab() override;

    static brls::View* create();

    enum Section
    {
        ACTIONS = 0,
        INSTALLED,
    };
    enum ActionRow
    {
        INSTALL = 0,
        REFRESH,
    };

    bool isInstalling() const { return installing; }

    void openInstall();
    void refresh();
    void openActions(size_t index);
    void toggle(size_t index);
    void move(size_t index, int delta);
    void confirmRemove(size_t index);

  private:
    // Reloads the list; if focus was inside the tab it moves to focusRow
    // (a row of the installed section), or stays on the row it was on.
    void reload(int focusRow = -1);
    bool hasFocusInside() const;

    BRLS_BIND(brls::RecyclerFrame, recycler, "addons/recycler");
    int subscription    = 0;
    int pendingFocusRow = -1; // set by move() for the reload its change triggers
    bool installing     = false;
};
