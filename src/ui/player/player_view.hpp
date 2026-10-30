#pragma once

#include <borealis.hpp>
#include <functional>

#include "ui/player/mpv_core.hpp"

namespace nuvio::ui::player
{

// The video: a borealis view that has mpv draw each frame into an off-screen texture and then
// paints that texture over its box (the way Switchfin and wiliwili embed mpv). mpv's render
// context is created on the first draw, when borealis's GL context is current. The GL state
// mpv may change is saved and restored around its render call, so NanoVG goes on unaffected.
class PlayerView : public brls::View
{
  public:
    explicit PlayerView(MpvCore& mpv);
    ~PlayerView() override;

    // Frees the texture and framebuffer; call with the GL context current (before the core is destroyed).
    void releaseGl();

    // Runs once the render context exists (or failed to be created, with false).
    std::function<void(bool ok)> onContextReady;

    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
        brls::FrameContext* ctx) override;

  private:
    void resize(NVGcontext* vg, int width, int height);

    MpvCore& mpv;
    bool contextTried  = false;
    unsigned fbo       = 0;
    unsigned texture   = 0;
    int image          = 0;
    int pixelWidth     = 0;
    int pixelHeight    = 0;
    bool hasFrame      = false;
};

} // namespace nuvio::ui::player
