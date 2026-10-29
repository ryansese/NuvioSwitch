#include "ui/common/poster_cell.hpp"

#include "core/image/image_loader.hpp"

using nuvio::core::image::ImageLoader;
using nuvio::domain::PosterShape;

// The title under the artwork. borealis has only the regular font, so bold is drawn
// as the text twice, half a pixel apart.
class PosterTitle : public brls::Label
{
  public:
    void setBold(bool value) { bold = value; }

    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
        brls::FrameContext* ctx) override
    {
        Label::draw(vg, x, y, width, height, style, ctx);
        if (bold)
            Label::draw(vg, x + 0.5f, y, width, height, style, ctx);
    }

  private:
    bool bold = false;
};

float PosterCell::widthFor(PosterShape shape)
{
    switch (shape)
    {
        case PosterShape::Landscape: return 224;
        case PosterShape::Square: return 160;
        default: return 126;
    }
}

float PosterCell::heightFor(PosterShape shape)
{
    switch (shape)
    {
        case PosterShape::Landscape: return 126;
        case PosterShape::Square: return 160;
        default: return 189;
    }
}

PosterCell::PosterCell()
{
    this->setAxis(brls::Axis::COLUMN);

    // the artwork is what takes focus, so the highlight frames it and not the title
    art = new brls::Box();
    art->setFocusable(true);
    art->setCornerRadius(12);
    art->setHighlightCornerRadius(12);
    art->setHideHighlightBackground(true);
    art->registerClickAction([this](brls::View*)
        {
            if (onSelect)
                onSelect(this);
            return true;
        });
    art->getFocusEvent()->subscribe([this](brls::View*)
        {
            title->setBold(true);
            if (onFocus)
                onFocus(this);
        });
    art->getFocusLostEvent()->subscribe([this](brls::View*) { title->setBold(false); });
    this->addView(art);

    image = new brls::Image();
    image->setScalingType(brls::ImageScalingType::FILL);
    image->setCornerRadius(12);
    art->addView(image);

    title = new PosterTitle();
    title->setFontSize(15);
    title->setSingleLine(true);
    title->setMarginTop(8);
    this->addView(title);
}

PosterCell::~PosterCell()
{
    if (ImageLoader::hasShared())
        ImageLoader::shared().cancel(image);
}

void PosterCell::setItem(const nuvio::domain::MetaPreview& value)
{
    item = value;
    float width  = widthFor(item.posterShape);
    float height = heightFor(item.posterShape);
    image->setDimensions(width, height);
    title->setWidth(width);
    title->setText(item.name);
    loadImage();
}

void PosterCell::loadImage()
{
    imageLoaded = true;
    ImageLoader::shared().load(image, item.poster.value_or(""));
}

void PosterCell::releaseImage()
{
    if (!imageLoaded)
        return;
    imageLoaded = false;
    ImageLoader::shared().cancel(image);
    image->clear();
}
