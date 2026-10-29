#pragma once

#include <functional>
#include <string>

#include "core/net/http_client.hpp"
#include "data/catalog/catalog_url.hpp"
#include "domain/catalog_row.hpp"

namespace nuvio::data::catalog
{

namespace net = core::net;

// Loads catalog pages from add-ons (NuvioTV CatalogRepositoryImpl.getCatalog).
// Requests skip certificate checks like every add-on request, and use the default
// cache policy so an add-on's Cache-Control is respected. Responses are parsed on
// the HTTP workers; results arrive on the UI thread.
class CatalogRepository
{
  public:
    struct Result
    {
        bool ok = false;
        domain::CatalogPage page;
        std::string error; // when !ok: "HTTP 500", a curl message, "invalid response"
    };
    using Callback = std::function<void(Result)>;

    explicit CatalogRepository(net::HttpClient& http) : http(http) { }

    // The page of source's catalog starting at skip. Cancel the handle to drop the result.
    net::RequestHandle load(const domain::CatalogSource& source, int skip, const Extras& extras, Callback callback);

    // The app-wide instance, set up in main.cpp.
    static CatalogRepository& shared();
    static void setShared(CatalogRepository* repository);

  private:
    net::HttpClient& http;
};

} // namespace nuvio::data::catalog
