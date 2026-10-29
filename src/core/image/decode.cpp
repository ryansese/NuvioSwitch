#include "core/image/decode.hpp"

#include <webp/decode.h>

namespace nuvio::core::image
{

bool isWebp(const std::string& b)
{
    return b.size() >= 12 && b.compare(0, 4, "RIFF") == 0 && b.compare(8, 4, "WEBP") == 0;
}

std::optional<Rgba> decodeWebp(const std::string& bytes)
{
    if (!isWebp(bytes))
        return std::nullopt;

    const auto* data = reinterpret_cast<const uint8_t*>(bytes.data());
    Rgba image;
    if (!WebPGetInfo(data, bytes.size(), &image.width, &image.height) || image.width <= 0 || image.height <= 0)
        return std::nullopt;

    size_t stride = static_cast<size_t>(image.width) * 4;
    image.pixels.resize(stride * static_cast<size_t>(image.height));
    if (!WebPDecodeRGBAInto(data, bytes.size(), image.pixels.data(), image.pixels.size(), static_cast<int>(stride)))
        return std::nullopt;
    return image;
}

} // namespace nuvio::core::image
