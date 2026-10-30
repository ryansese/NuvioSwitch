#pragma once

#include <borealis.hpp>

namespace nuvio::ui::player
{

// The progress line of the controls: the played part, the buffered range and a knob that
// shows while it has focus. It takes focus so Left/Right can seek from it.
class SeekBar : public brls::Box
{
  public:
    SeekBar()
    {
        this->setFocusable(true);
        this->setHeight(36);
        this->setWidth(brls::View::AUTO);
        this->setHideHighlightBackground(true);
        this->setHighlightCornerRadius(10);
    }

    // all in seconds; duration 0 means unknown
    void setProgress(double position, double buffered, double duration)
    {
        this->position = position;
        this->buffered = buffered;
        this->duration = duration;
    }

    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style, brls::FrameContext*) override
    {
        constexpr float TRACK = 6;
        float top             = y + (height - TRACK) / 2;
        auto fraction         = [this](double seconds)
        { return duration > 0 ? static_cast<float>(std::max(0.0, std::min(1.0, seconds / duration))) : 0.0f; };

        nvgBeginPath(vg);
        nvgRoundedRect(vg, x, top, width, TRACK, TRACK / 2);
        nvgFillColor(vg, nvgRGBA(255, 255, 255, 60));
        nvgFill(vg);

        nvgBeginPath(vg);
        nvgRoundedRect(vg, x, top, width * fraction(buffered), TRACK, TRACK / 2);
        nvgFillColor(vg, nvgRGBA(255, 255, 255, 110));
        nvgFill(vg);

        float played = width * fraction(position);
        nvgBeginPath(vg);
        nvgRoundedRect(vg, x, top, played, TRACK, TRACK / 2);
        nvgFillColor(vg, nvgRGB(255, 255, 255));
        nvgFill(vg);

        if (this->isFocused())
        {
            nvgBeginPath(vg);
            nvgCircle(vg, x + played, y + height / 2, 10);
            nvgFillColor(vg, nvgRGB(255, 255, 255));
            nvgFill(vg);
        }
    }

  private:
    double position = 0;
    double buffered = 0;
    double duration = 0;
};

} // namespace nuvio::ui::player
