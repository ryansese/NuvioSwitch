#include "core/net/curl_executor.hpp"

#include <curl/curl.h>

#include <cctype>
#include <mutex>

namespace nuvio::core::net
{

struct CurlExecutor::Share
{
    CURLSH* handle = nullptr;
    // one lock per kind of shared data, as the libcurl share docs describe
    std::mutex locks[CURL_LOCK_DATA_LAST];
};

namespace
{
void lockShare(CURL*, curl_lock_data data, curl_lock_access, void* user)
{
    auto* locks = static_cast<std::mutex*>(user);
    locks[data].lock();
}

void unlockShare(CURL*, curl_lock_data data, void* user)
{
    auto* locks = static_cast<std::mutex*>(user);
    locks[data].unlock();
}

size_t writeBody(char* data, size_t size, size_t count, void* user)
{
    static_cast<std::string*>(user)->append(data, size * count);
    return size * count;
}

std::string trim(const std::string& s)
{
    size_t begin = 0;
    size_t end   = s.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(s[begin])))
        begin++;
    while (end > begin && std::isspace(static_cast<unsigned char>(s[end - 1])))
        end--;
    return s.substr(begin, end - begin);
}

size_t writeHeader(char* data, size_t size, size_t count, void* user)
{
    auto* headers = static_cast<Headers*>(user);
    std::string line(data, size * count);

    // a new status line starts the headers of the next response in a redirect chain
    if (line.rfind("HTTP/", 0) == 0)
    {
        headers->clear();
        return size * count;
    }

    size_t colon = line.find(':');
    if (colon == std::string::npos)
        return size * count;
    std::string name = trim(line.substr(0, colon));
    for (auto& c : name)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    std::string value = trim(line.substr(colon + 1));

    auto it = headers->find(name);
    if (it == headers->end())
        headers->emplace(name, value);
    else
        it->second += ", " + value;
    return size * count;
}

int onProgress(void* user, curl_off_t, curl_off_t, curl_off_t, curl_off_t)
{
    // non-zero aborts the transfer with CURLE_ABORTED_BY_CALLBACK
    return static_cast<const std::atomic<bool>*>(user)->load() ? 1 : 0;
}

HttpError mapError(CURLcode code)
{
    switch (code)
    {
        case CURLE_OPERATION_TIMEDOUT:
            return HttpError::Timeout;
        case CURLE_COULDNT_RESOLVE_PROXY:
        case CURLE_COULDNT_RESOLVE_HOST:
        case CURLE_COULDNT_CONNECT:
        case CURLE_SEND_ERROR:
        case CURLE_RECV_ERROR:
        case CURLE_GOT_NOTHING:
        case CURLE_PARTIAL_FILE:
            return HttpError::Connect;
        case CURLE_SSL_CONNECT_ERROR:
        case CURLE_PEER_FAILED_VERIFICATION:
        case CURLE_SSL_CERTPROBLEM:
        case CURLE_SSL_CIPHER:
        case CURLE_SSL_CACERT_BADFILE:
        case CURLE_SSL_ISSUER_ERROR:
            return HttpError::Tls;
        case CURLE_ABORTED_BY_CALLBACK:
            return HttpError::Cancelled;
        default:
            return HttpError::Other;
    }
}
} // namespace

CurlExecutor::CurlExecutor(std::string bundle) : share(std::make_unique<Share>()), caBundle(std::move(bundle))
{
    share->handle = curl_share_init();
    curl_share_setopt(share->handle, CURLSHOPT_LOCKFUNC, lockShare);
    curl_share_setopt(share->handle, CURLSHOPT_UNLOCKFUNC, unlockShare);
    curl_share_setopt(share->handle, CURLSHOPT_USERDATA, share->locks);
    curl_share_setopt(share->handle, CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS);
    curl_share_setopt(share->handle, CURLSHOPT_SHARE, CURL_LOCK_DATA_SSL_SESSION);
    curl_share_setopt(share->handle, CURLSHOPT_SHARE, CURL_LOCK_DATA_CONNECT);
}

CurlExecutor::~CurlExecutor()
{
    if (share->handle)
        curl_share_cleanup(share->handle);
}

HttpResult CurlExecutor::perform(const Request& request, const std::atomic<bool>& cancelled) const
{
    HttpResult result;
    CURL* curl = curl_easy_init();
    if (!curl)
    {
        result.error        = HttpError::Other;
        result.errorMessage = "curl_easy_init failed";
        return result;
    }

    char errorBuffer[CURL_ERROR_SIZE] = {0};
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuffer);
    curl_easy_setopt(curl, CURLOPT_URL, request.url.c_str());
    curl_easy_setopt(curl, CURLOPT_SHARE, share->handle);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L); // required with threads

    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, request.connectTimeoutSeconds);
    // read timeout: fail if under 1 byte/s for the whole period
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, request.readTimeoutSeconds);

    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 10L);
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, ""); // every encoding curl was built with

    if (request.permissiveTls)
    {
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    }
    else if (!caBundle.empty())
    {
        curl_easy_setopt(curl, CURLOPT_CAINFO, caBundle.c_str());
    }

    if (request.method == "GET")
        curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    else if (request.method == "HEAD")
        curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    else
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, request.method.c_str());
    if (!request.body.empty() || request.method == "POST")
    {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request.body.data());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(request.body.size()));
    }

    curl_slist* headerList = nullptr;
    for (const auto& [name, value] : request.headers)
        headerList = curl_slist_append(headerList, (name + ": " + value).c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerList);

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeBody);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result.body);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, writeHeader);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &result.headers);

    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, onProgress);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &cancelled);

    CURLcode code = curl_easy_perform(curl);

    char* effectiveUrl = nullptr;
    curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &effectiveUrl);
    result.finalUrl = effectiveUrl ? effectiveUrl : request.url;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &result.status);

    if (code != CURLE_OK)
    {
        result.error        = mapError(code);
        result.errorMessage = errorBuffer[0] ? errorBuffer : curl_easy_strerror(code);
        result.status       = 0;
        result.body.clear();
    }
    else if (result.status < 200 || result.status > 299)
    {
        result.error        = HttpError::Http;
        result.errorMessage = "HTTP " + std::to_string(result.status);
    }
    else
    {
        result.ok = true;
    }

    curl_slist_free_all(headerList);
    curl_easy_cleanup(curl);
    return result;
}

} // namespace nuvio::core::net
