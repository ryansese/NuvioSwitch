#pragma once

#include <string>

#include "domain/catalog_row.hpp"
#include "domain/meta_preview.hpp"
#include "domain/stream_request.hpp"

// Screens other features open. Each is pushed on top of the shell, and B returns
// to where the user was.
namespace nuvio::ui
{

// The item's metadata, seasons and episodes, with Play.
void openDetails(const domain::MetaPreview& item, const std::string& sourceAddonName);

// The stream picker for a video. Until the picker (NuvioSwitch-0007) lands, this only
// tells the user it's coming.
void openStreams(const domain::StreamRequest& request);

// Every item of a catalog in a paged grid, starting from what row already holds.
void openSeeAll(const domain::CatalogRow& row);

} // namespace nuvio::ui
