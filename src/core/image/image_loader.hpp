#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>

#include "core/async/worker_pool.hpp"
#include "core/image/decode.hpp"
#include "core/image/image_cache.hpp"
#include "core/net/http_types.hpp"

namespace brls
{
class Image;
}

namespace nuvio::core::image
{

// Loads remote posters and backdrops into borealis Image views.
// Downloads run on their own 4-thread pool, so posters never starve add-on
// requests. Encoded bytes are cached in memory and on disk. JPEG/PNG are decoded
// by nanovg on the UI thread; WebP is decoded to RGBA on the pool first.
// Call load/cancel on the UI thread only.
// Implemented in image_loader_brls.cpp, so it is not part of the test target.
class ImageLoader
{
  public:
    using Fetch = std::function<net::HttpResult(const net::Request&, const std::atomic<bool>& cancelled)>;

    static constexpr const char* PLACEHOLDER = "img/placeholder.png";

    ImageLoader(Fetch fetch, net::Headers defaultHeaders, const std::string& diskDir, uint64_t memoryBytes);
    ~ImageLoader(); // shutdown()

    // Shows placeholder in view, then the image at url once it is available.
    // Replaces any earlier load for the same view; that result is discarded.
    void load(brls::Image* view, const std::string& url, const std::string& placeholder = PLACEHOLDER);

    // Forgets the pending load for view. Views call this in prepareForReuse()
    // and before they are destroyed.
    void cancel(brls::Image* view);

    void shutdown();

    // The app-wide instance, set up in main.cpp.
    static ImageLoader& shared();
    static void setShared(ImageLoader* loader);
    // False before setup and after teardown; views check it before cancelling in their destructors.
    static bool hasShared();

  private:
    struct Pending
    {
        uint64_t token;
        net::CancelFlag cancelled;
    };

    // What a worker hands to the UI thread: pixels for WebP, else encoded bytes.
    struct Ready
    {
        Bytes encoded;
        std::shared_ptr<const Rgba> rgba;
    };

    static Ready prepare(Bytes bytes);
    void finish(brls::Image* view, uint64_t token, const Ready& ready);
    static bool apply(brls::Image* view, const Ready& ready);

    Fetch fetch;
    net::Headers headers;
    ImageCache cache;
    std::unordered_map<brls::Image*, Pending> pending; // UI thread only
    uint64_t nextToken = 1;
    async::WorkerPool pool;
};

} // namespace nuvio::core::image
