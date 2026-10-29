#pragma once

#include <string>

#include "domain/catalog_row.hpp"
#include "domain/meta_preview.hpp"

// Screens other features open. Each is pushed on top of the shell, and B returns
// to where the user was.
namespace nuvio::ui
{

// The item's details. Until Details (NuvioSwitch-0006) lands, a stub with the
// name, type, id and source add-on.
void openDetails(const domain::MetaPreview& item, const std::string& sourceAddonName);

// Every item of a catalog in a paged grid, starting from what row already holds.
void openSeeAll(const domain::CatalogRow& row);

} // namespace nuvio::ui
