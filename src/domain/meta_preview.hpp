#pragma once

#include <optional>
#include <string>
#include <vector>

// A catalog item as add-on catalogs return it (NuvioTV domain/model/MetaPreview.kt).
namespace nuvio::domain
{

enum class PosterShape
{
    Poster,    // 2:3
    Landscape, // 16:9
    Square,
};

// "landscape" and "square" in any case; anything else is a poster.
PosterShape posterShapeFrom(const std::string& value);

struct MetaPreview
{
    std::string id;
    std::string type; // as the add-on spells it; the catalog's type when the item has none
    std::string name;
    std::optional<std::string> poster;
    PosterShape posterShape = PosterShape::Poster;
    std::optional<std::string> background;
    std::optional<std::string> logo;
    std::optional<std::string> description;
    std::optional<std::string> releaseInfo;
    std::optional<float> imdbRating;
    std::vector<std::string> genres;
    std::string sourceAddonBaseUrl; // the add-on whose catalog listed it

    // Whether the hero can show it full-bleed (NuvioTV hasHeroArtwork).
    bool hasHeroArtwork() const;
};

} // namespace nuvio::domain
