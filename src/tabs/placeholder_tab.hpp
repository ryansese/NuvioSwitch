#pragma once

#include <borealis.hpp>

// Stand-in content for a tab whose feature hasn't landed yet.
// XML attributes: titleKey and noteKey (i18n keys), showVersion (bool).
class PlaceholderTab : public brls::Box
{
  public:
    PlaceholderTab();

    static brls::View* create();

  private:
    BRLS_BIND(brls::Label, title, "placeholder/title");
    BRLS_BIND(brls::Label, note, "placeholder/note");
    BRLS_BIND(brls::Label, comingSoon, "placeholder/coming_soon");
    BRLS_BIND(brls::Label, version, "placeholder/version");
};
