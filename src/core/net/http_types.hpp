#pragma once

#include <atomic>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace nuvio::core::net
{

// Header names are compared case-insensitively; responses store them lowercased.
struct CaseInsensitiveLess
{
    bool operator()(const std::string& a, const std::string& b) const;
};
using Headers = std::map<std::string, std::string, CaseInsensitiveLess>;

enum class CachePolicy
{
    Default,      // use a fresh cached response, else go to the network and store the result
    NoCache,      // always go to the network; still store the result
    StaleIfError, // like Default, but return an expired cached body if the network fails
};

struct Request
{
    std::string method = "GET";
    std::string url;
    Headers headers; // added on top of the defaults, replacing any with the same name
    std::string body;

    // Skip certificate and hostname checks, as NuvioTV's addonPermissive client does.
    // Add-on requests (NuvioSwitch-0003) always set it.
    bool permissiveTls = false;

    CachePolicy cachePolicy = CachePolicy::Default;

    long connectTimeoutSeconds = 30;
    long readTimeoutSeconds    = 60; // no data for this long fails the request
};

// Mirrors NuvioTV's NetworkResult (core/network/NetworkResult.kt).
enum class HttpError
{
    None,
    Http, // the server answered with a status outside 2xx
    Timeout,
    Connect,
    Tls,
    Cancelled,
    Other,
};

struct HttpResult
{
    bool ok    = false;
    long status = 0;
    Headers headers;
    std::string body;
    std::string finalUrl;

    HttpError error = HttpError::None;
    std::string errorMessage;

    bool fromCache = false;
    bool stale     = false; // an expired cached body served under StaleIfError
};

// True for failures where an offline fallback makes sense.
inline bool isNetworkFailure(HttpError error)
{
    return error == HttpError::Timeout || error == HttpError::Connect || error == HttpError::Other;
}

using CancelFlag = std::shared_ptr<std::atomic<bool>>;

// Returned by HttpClient::send. Cancelling stops the transfer and suppresses the callback.
class RequestHandle
{
  public:
    RequestHandle() = default;
    explicit RequestHandle(CancelFlag flag) : flag(std::move(flag)) { }

    void cancel()
    {
        if (flag)
            flag->store(true);
    }
    bool isCancelled() const { return flag && flag->load(); }

  private:
    CancelFlag flag;
};

} // namespace nuvio::core::net
