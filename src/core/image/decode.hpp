#pragma once

#include <optional>
#include <string>
#include <vector>

namespace nuvio::core::image
{

struct Rgba
{
    int width  = 0;
    int height = 0;
    std::vector<unsigned char> pixels; // width * height * 4, row by row
};

// RIFF....WEBP: the format images.metahub.space serves, which stb_image can't decode.
bool isWebp(const std::string& bytes);

// Decodes a WebP image with libwebp. nullopt if it isn't valid WebP.
std::optional<Rgba> decodeWebp(const std::string& bytes);

} // namespace nuvio::core::image
