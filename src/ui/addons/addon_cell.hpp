#pragma once

#include <borealis.hpp>
#include <functional>

#include "domain/addon.hpp"

// One installed add-on in the Add-ons tab: logo, name, version, description,
// an "Off" badge when disabled, and a note while its manifest is missing.
// X, L and R call onToggle and onMove; A is handled by the tab's data source.
class AddonCell : public brls::RecyclerCell
{
  public:
    AddonCell();
    ~AddonCell() override;

    void setAddon(const nuvio::domain::Addon& addon, bool loading);
    void prepareForReuse() override;

    std::function<void(AddonCell*)> onToggle;
    std::function<void(AddonCell*, int delta)> onMove;

    static brls::RecyclerCell* create();

  private:
    BRLS_BIND(brls::Image, logo, "addon/logo");
    BRLS_BIND(brls::Label, name, "addon/name");
    BRLS_BIND(brls::Label, version, "addon/version");
    BRLS_BIND(brls::Label, off, "addon/off");
    BRLS_BIND(brls::Label, description, "addon/description");
};

// "Install add-on" and "Refresh add-ons" above the list.
class AddonActionCell : public brls::RecyclerCell
{
  public:
    AddonActionCell();

    void setTitle(const std::string& text) { title->setText(text); }

    static brls::RecyclerCell* create();

  private:
    BRLS_BIND(brls::Label, title, "action/title");
};
