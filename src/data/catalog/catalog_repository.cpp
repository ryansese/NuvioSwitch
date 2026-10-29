#include "data/catalog/catalog_repository.hpp"

#include <cassert>

#include "core/log.hpp"
#include "data/addons/addon_url.hpp"
#include "data/catalog/catalog_parser.hpp"
#include "domain/catalog_paging.hpp"

namespace nuvio::data::catalog
{

namespace
{
CatalogRepository* sharedRepository = nullptr;
}

CatalogRepository& CatalogRepository::shared()
{
    assert(sharedRepository && "CatalogRepository::setShared was not called");
    return *sharedRepository;
}

void CatalogRepository::setShared(CatalogRepository* repository)
{
    sharedRepository = repository;
}

net::RequestHandle CatalogRepository::load(const domain::CatalogSource& source, int skip, const Extras& extras,
    Callback callback)
{
    net::Request request;
    request.url           = buildCatalogUrl(source.addonBaseUrl, source.catalog.type, source.catalog.id, skip, extras);
    request.permissiveTls = true;

    return http.sendPrepared(std::move(request),
        [source, skip, callback = std::move(callback)](net::HttpResult response)
        {
            // on the worker: catalog pages can be large
            Result result;
            if (!response.ok)
            {
                result.error = response.error == net::HttpError::Http ? "HTTP " + std::to_string(response.status)
                                                                      : response.errorMessage;
            }
            else if (auto parsed = parseCatalog(response.body, source.catalog.type, source.addonBaseUrl))
            {
                auto next             = domain::nextPage(source.catalog, skip, parsed->rawCount);
                result.ok             = true;
                result.page.items     = std::move(parsed->items);
                result.page.rawCount  = parsed->rawCount;
                result.page.nextSkip  = next.skip;
                result.page.hasMore   = next.hasMore;
            }
            else
            {
                result.error = "invalid response";
            }

            if (!result.ok)
                core::log::warning("Catalog " + source.key() + " from " + addons::hostOf(source.addonBaseUrl) +
                                   " failed: " + result.error);
            return std::function<void()>([callback, result = std::move(result)]() mutable { callback(std::move(result)); });
        });
}

} // namespace nuvio::data::catalog
