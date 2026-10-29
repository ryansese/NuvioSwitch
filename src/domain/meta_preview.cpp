#include "domain/meta_preview.hpp"

#include <cctype>
#include <strings.h>

namespace nuvio::domain
{

PosterShape posterShapeFrom(const std::string& value)
{
    if (strcasecmp(value.c_str(), "landscape") == 0)
        return PosterShape::Landscape;
    if (strcasecmp(value.c_str(), "square") == 0)
        return PosterShape::Square;
    return PosterShape::Poster;
}

bool MetaPreview::hasHeroArtwork() const
{
    if (!background)
        return false;
    for (unsigned char c : *background)
        if (!std::isspace(c))
            return true;
    return false;
}

} // namespace nuvio::domain
