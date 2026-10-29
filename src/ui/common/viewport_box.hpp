#pragma once

#include <borealis.hpp>

// A Box that only draws the children overlapping a viewport (normally the
// scrolling frame it sits in), grown by margin for focus highlights.
//
// borealis culls leaf views only: a nested Box is always drawn, and each leaf
// it holds is then checked against every ancestor. Home's rows and posters are
// nested boxes, so without this every poster of every row was drawn each frame,
// which made scrolling lag on the console.
class ViewportBox : public brls::Box
{
  public:
    static constexpr float DEFAULT_MARGIN = 24;

    ViewportBox(brls::Axis axis, brls::View* viewport, float margin = DEFAULT_MARGIN)
        : brls::Box(axis)
        , viewport(viewport)
        , margin(margin)
    {
    }

    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
        brls::FrameContext* ctx) override
    {
        float left   = viewport->getX() - margin;
        float top    = viewport->getY() - margin;
        float right  = left + viewport->getWidth() + 2 * margin;
        float bottom = top + viewport->getHeight() + 2 * margin;

        for (brls::View* child : this->getChildren())
        {
            float childLeft = child->getX();
            float childTop  = child->getY();
            if (childLeft + child->getWidth() < left || childLeft > right || childTop + child->getHeight() < top ||
                childTop > bottom)
                continue;
            child->frame(ctx);
        }
    }

  private:
    brls::View* viewport;
    float margin;
};
