#include "core/image/image_loader.hpp"

#include <borealis.hpp>
#include <cassert>

#include "core/async/ui_thread.hpp"
#include "core/net/headers.hpp"

namespace nuvio::core::image
{

namespace
{
ImageLoader* sharedLoader = nullptr;
}

ImageLoader& ImageLoader::shared()
{
    assert(sharedLoader && "ImageLoader::setShared was not called");
    return *sharedLoader;
}

void ImageLoader::setShared(ImageLoader* loader)
{
    sharedLoader = loader;
}

bool ImageLoader::hasShared()
{
    return sharedLoader != nullptr;
}

ImageLoader::ImageLoader(Fetch fetchFn, net::Headers defaultHeaders, const std::string& diskDir, uint64_t memoryBytes)
    : fetch(std::move(fetchFn)), headers(std::move(defaultHeaders)), cache(diskDir, memoryBytes), pool(4)
{
}

ImageLoader::~ImageLoader()
{
    shutdown();
}

void ImageLoader::shutdown()
{
    for (auto& [view, request] : pending)
        request.cancelled->store(true);
    pending.clear();
    pool.shutdown();
    cache.flush();
}

void ImageLoader::load(brls::Image* view, const std::string& url, const std::string& placeholder)
{
    cancel(view);

    // JPEG/PNG from memory go straight to nanovg; WebP still needs the pool to decode
    if (Bytes bytes = cache.fromMemory(url); bytes && !isWebp(*bytes) && apply(view, Ready{bytes, nullptr}))
        return;

    view->clear();
    view->setImageFromRes(placeholder);
    if (url.empty())
        return;

    uint64_t token = nextToken++;
    auto cancelled = std::make_shared<std::atomic<bool>>(false);
    pending[view]  = Pending{token, cancelled};

    pool.post([this, view, token, url, cancelled]
        {
            // the view moved on while this was queued: don't download
            if (cancelled->load())
                return;

            Bytes bytes = cache.fromMemory(url);
            if (!bytes)
                bytes = cache.fromDisk(url);
            if (!bytes)
            {
                net::Request request;
                request.url     = url;
                request.headers = headers;
                net::HttpResult result = fetch(request, *cancelled);
                if (cancelled->load())
                    return;
                if (result.ok && looksLikeImage(result.body))
                {
                    bytes = std::make_shared<const std::string>(std::move(result.body));
                    cache.put(url, bytes);
                }
                else
                {
                    brls::Logger::debug("ImageLoader: {} failed: {}", url,
                        result.ok ? std::string("not an image") : result.errorMessage);
                }
            }

            Ready ready = prepare(bytes);
            if (cancelled->load())
                return;
            async::postToUi([this, view, token, ready] { finish(view, token, ready); });
        });
}

void ImageLoader::cancel(brls::Image* view)
{
    auto it = pending.find(view);
    if (it == pending.end())
        return;
    it->second.cancelled->store(true);
    pending.erase(it);
}

ImageLoader::Ready ImageLoader::prepare(Bytes bytes)
{
    if (!bytes || !isWebp(*bytes))
        return Ready{bytes, nullptr};

    auto rgba = decodeWebp(*bytes);
    if (!rgba)
    {
        brls::Logger::debug("ImageLoader: undecodable WebP ({} bytes)", bytes->size());
        return Ready{};
    }
    return Ready{nullptr, std::make_shared<const Rgba>(std::move(*rgba))};
}

void ImageLoader::finish(brls::Image* view, uint64_t token, const Ready& ready)
{
    // a newer load or a cancel replaced this one; view may even be gone
    auto it = pending.find(view);
    if (it == pending.end() || it->second.token != token)
        return;
    pending.erase(it);

    if ((ready.encoded || ready.rgba) && !apply(view, ready))
        brls::Logger::debug("ImageLoader: could not create a texture"); // the placeholder stays
}

bool ImageLoader::apply(brls::Image* view, const Ready& ready)
{
    NVGcontext* vg = brls::Application::getNVGContext();
    int texture    = 0;
    if (ready.rgba)
        texture = nvgCreateImageRGBA(vg, ready.rgba->width, ready.rgba->height, 0, ready.rgba->pixels.data());
    else if (ready.encoded)
        texture = nvgCreateImageMem(vg, 0, (unsigned char*)ready.encoded->data(), (int)ready.encoded->size());
    if (texture == 0)
        return false;

    // clear() drops the old texture: deleted if it was ours, left to borealis's
    // texture cache if it was the placeholder resource
    view->clear();
    view->setFreeTexture(true);
    view->innerSetImage(texture);
    return true;
}

} // namespace nuvio::core::image
