#pragma once

#include <borealis.hpp>

// A dark fade over backdrop artwork: opaque at the left edge and bottom so text on
// them stays readable, clear towards the top right where the artwork shows.
class Scrim : public brls::View
{
  public:
    Scrim()
    {
        // pinned to all four edges of a parent without padding
        this->setPositionType(brls::PositionType::ABSOLUTE);
        this->setPositionTop(0);
        this->setPositionLeft(0);
        this->setPositionRight(0);
        this->setPositionBottom(0);
    }

    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
        brls::FrameContext* ctx) override
    {
        NVGcolor solid = brls::Application::getTheme()["brls/background"];
        NVGcolor clear = solid;
        clear.a        = 0;

        nvgBeginPath(vg);
        nvgRect(vg, x, y, width, height);
        nvgFillPaint(vg, nvgLinearGradient(vg, x, y, x + width * 0.75f, y, nvgRGBAf(solid.r, solid.g, solid.b, 0.92f), clear));
        nvgFill(vg);

        nvgBeginPath(vg);
        nvgRect(vg, x, y, width, height);
        nvgFillPaint(vg, nvgLinearGradient(vg, x, y + height * 0.55f, x, y + height, clear, solid));
        nvgFill(vg);
    }
};
