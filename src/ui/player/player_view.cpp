#include "ui/player/player_view.hpp"

#include <algorithm>
#include <borealis/extern/glad/glad.h>
#include <borealis/extern/nanovg/nanovg.h>

#include "core/log.hpp"

// defined by borealis's NanoVG build
extern "C" int nvglCreateImageFromHandleGL3(NVGcontext* ctx, GLuint textureId, int w, int h, int flags);

namespace nuvio::ui::player
{

namespace
{
// NVG_IMAGE_NODELETE of nanovg_gl.h: the texture stays ours
constexpr int IMAGE_NODELETE = 1 << 16;
// the largest frame buffer: more than the console's docked output isn't worth the memory
constexpr int MAX_WIDTH  = 1920;
constexpr int MAX_HEIGHT = 1080;

// The GL state mpv's renderer may disturb that NanoVG relies on between draws.
struct SavedState
{
    GLint fbo, viewport[4], activeTexture, texture2d, program, vao, arrayBuffer, unpackAlignment;
    GLint blendSrcRgb, blendDstRgb, blendSrcAlpha, blendDstAlpha, blendEqRgb, blendEqAlpha;
    GLboolean blend, scissor, depth, stencil, cull;

    SavedState()
    {
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &fbo);
        glGetIntegerv(GL_VIEWPORT, viewport);
        glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture2d);
        glGetIntegerv(GL_CURRENT_PROGRAM, &program);
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &arrayBuffer);
        glGetIntegerv(GL_UNPACK_ALIGNMENT, &unpackAlignment);
        glGetIntegerv(GL_BLEND_SRC_RGB, &blendSrcRgb);
        glGetIntegerv(GL_BLEND_DST_RGB, &blendDstRgb);
        glGetIntegerv(GL_BLEND_SRC_ALPHA, &blendSrcAlpha);
        glGetIntegerv(GL_BLEND_DST_ALPHA, &blendDstAlpha);
        glGetIntegerv(GL_BLEND_EQUATION_RGB, &blendEqRgb);
        glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &blendEqAlpha);
        blend   = glIsEnabled(GL_BLEND);
        scissor = glIsEnabled(GL_SCISSOR_TEST);
        depth   = glIsEnabled(GL_DEPTH_TEST);
        stencil = glIsEnabled(GL_STENCIL_TEST);
        cull    = glIsEnabled(GL_CULL_FACE);
    }

    void restore() const
    {
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
        glUseProgram(program);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, arrayBuffer);
        glActiveTexture(activeTexture);
        glBindTexture(GL_TEXTURE_2D, texture2d);
        glPixelStorei(GL_UNPACK_ALIGNMENT, unpackAlignment);
        glBlendFuncSeparate(blendSrcRgb, blendDstRgb, blendSrcAlpha, blendDstAlpha);
        glBlendEquationSeparate(blendEqRgb, blendEqAlpha);
        auto set = [](GLenum cap, GLboolean on) { on ? glEnable(cap) : glDisable(cap); };
        set(GL_BLEND, blend);
        set(GL_SCISSOR_TEST, scissor);
        set(GL_DEPTH_TEST, depth);
        set(GL_STENCIL_TEST, stencil);
        set(GL_CULL_FACE, cull);
    }
};
} // namespace

PlayerView::PlayerView(MpvCore& mpv) : mpv(mpv)
{
    this->setGrow(1);
    this->setWidth(brls::View::AUTO);
    this->setHeight(brls::View::AUTO);
}

PlayerView::~PlayerView() = default;

void PlayerView::releaseGl()
{
    if (fbo)
        glDeleteFramebuffers(1, &fbo);
    if (texture)
        glDeleteTextures(1, &texture);
    fbo     = 0;
    texture = 0;
    // the NanoVG image only wraps the texture
    image = 0;
    pixelWidth = pixelHeight = 0;
}

void PlayerView::resize(NVGcontext* vg, int width, int height)
{
    if (image)
        nvgDeleteImage(vg, image);
    releaseGl();

    SavedState saved;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        core::log::error("player framebuffer is incomplete");
    saved.restore();

    image       = nvglCreateImageFromHandleGL3(vg, texture, width, height, IMAGE_NODELETE);
    pixelWidth  = width;
    pixelHeight = height;
    hasFrame    = false;
}

void PlayerView::draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style, brls::FrameContext*)
{
    if (!contextTried)
    {
        contextTried = true;
        core::log::info(std::string("GL ") + reinterpret_cast<const char*>(glGetString(GL_VERSION)) + " / " +
                        reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
        bool ok = mpv.createRenderContext();
        if (onContextReady)
            onContextReady(ok);
    }

    // frame buffer pixels per borealis unit
    float scale = static_cast<float>(brls::Application::windowScale *
                                     brls::Application::getPlatform()->getVideoContext()->getScaleFactor());
    int w = std::min(MAX_WIDTH, std::max(2, static_cast<int>(width * scale)));
    int h = std::min(MAX_HEIGHT, std::max(2, static_cast<int>(height * scale)));
    // keep the 16:9 canvas the whole screen gets
    if (w != pixelWidth || h != pixelHeight)
        resize(vg, w, h);

    {
        SavedState saved;
        if (mpv.render(fbo, pixelWidth, pixelHeight, !hasFrame))
            hasFrame = true;
        saved.restore();
    }

    if (!image)
        return;
    nvgBeginPath(vg);
    nvgRect(vg, x, y, width, height);
    nvgFillPaint(vg, nvgImagePattern(vg, x, y, width, height, 0, image, 1));
    nvgFill(vg);
}

} // namespace nuvio::ui::player
