#pragma once

#include <optional>
#include <string>

#include "domain/meta.hpp"

// Text the details screen builds from metadata, kept free of UI code so it can be tested.
namespace nuvio::domain
{

// text cut to about limit bytes on a UTF-8 boundary, with an ellipsis.
std::string shorten(const std::string& text, size_t limit);

// "S2E3 · Title", "E3 · Title" with no season, just the title with no numbers, or just "S2E3".
std::string episodeHeading(const Video& episode);

// The date part of an ISO timestamp ("2020-01-01T00:00:00.000Z" gives "2020-01-01");
// anything else is returned as it is, and nullopt gives "".
std::string releaseDate(const std::optional<std::string>& released);

// Release info, runtime and IMDb rating joined with " · ", leaving out what's missing.
std::string infoLine(const Meta& meta);

// "A, B, C" of the first limit names.
std::string joinNames(const std::vector<std::string>& names, size_t limit);

} // namespace nuvio::domain
