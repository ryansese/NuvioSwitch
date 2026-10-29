#pragma once

#include <atomic>
#include <memory>
#include <string>

#include "core/net/http_types.hpp"

namespace nuvio::core::net
{

// Runs requests with one curl easy handle each, sharing DNS, connection and TLS
// session caches between threads through a CURLSH share handle.
// curl_global_init must have been called (main.cpp) before constructing it.
class CurlExecutor
{
  public:
    // caBundle: path for CURLOPT_CAINFO, or empty for curl's default store.
    explicit CurlExecutor(std::string caBundle = "");
    ~CurlExecutor();

    CurlExecutor(const CurlExecutor&)            = delete;
    CurlExecutor& operator=(const CurlExecutor&) = delete;

    // Performs request synchronously. request.headers are sent as given;
    // HttpClient has already merged the defaults in.
    HttpResult perform(const Request& request, const std::atomic<bool>& cancelled) const;

  private:
    struct Share;
    std::unique_ptr<Share> share;
    std::string caBundle;
};

} // namespace nuvio::core::net
