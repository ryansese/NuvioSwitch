#pragma once

#include "domain/addon.hpp"

// Catalog paging (NuvioTV domain/model/CatalogDescriptorExtensions.kt and
// CatalogRepositoryImpl). Whether a catalog takes "skip" is CatalogDescriptor::supportsExtra.
namespace nuvio::domain
{

constexpr int DEFAULT_SKIP_STEP = 100;

// pageSize when positive, else the smallest positive gap between the numeric
// options of the "skip" extra (with at least two), else DEFAULT_SKIP_STEP.
int skipStep(const CatalogDescriptor& catalog);

struct NextPage
{
    int skip     = 0;
    bool hasMore = false;
};

// After a page requested at skip that returned rawCount metas: the next skip is
// skip + rawCount, and there is more only if the catalog supports skip and the
// page wasn't empty.
NextPage nextPage(const CatalogDescriptor& catalog, int skip, size_t rawCount);

} // namespace nuvio::domain
