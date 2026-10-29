#include <doctest.h>

#include "core/image/decode.hpp"
#include "core/image/image_cache.hpp"

using namespace nuvio::core::image;

namespace
{
// a 3x2 opaque red image, lossless (cwebp -lossless)
const unsigned char RED_WEBP[] = {
    0x52, 0x49, 0x46, 0x46, 0x1c, 0x00, 0x00, 0x00, 0x57, 0x45, 0x42, 0x50,
    0x56, 0x50, 0x38, 0x4c, 0x0f, 0x00, 0x00, 0x00, 0x2f, 0x02, 0x40, 0x00,
    0x00, 0x07, 0x10, 0xfd, 0x8f, 0xfe, 0x07, 0x22, 0xa2, 0xff, 0x01, 0x00,
};
const std::string redWebp(reinterpret_cast<const char*>(RED_WEBP), sizeof(RED_WEBP));
} // namespace

TEST_CASE("WebP is recognised by its RIFF header")
{
    CHECK(isWebp(redWebp));
    CHECK(looksLikeImage(redWebp));
    CHECK_FALSE(isWebp("RIFF....WAVEfmt "));
    CHECK_FALSE(isWebp(std::string("\xFF\xD8\xFF\xE0", 4)));
}

TEST_CASE("decodeWebp returns RGBA pixels")
{
    auto image = decodeWebp(redWebp);
    REQUIRE(image);
    CHECK(image->width == 3);
    CHECK(image->height == 2);
    REQUIRE(image->pixels.size() == 3 * 2 * 4);
    for (size_t i = 0; i < image->pixels.size(); i += 4)
    {
        CHECK(image->pixels[i] == 255);
        CHECK(image->pixels[i + 1] == 0);
        CHECK(image->pixels[i + 2] == 0);
        CHECK(image->pixels[i + 3] == 255);
    }
}

TEST_CASE("decodeWebp rejects truncated or foreign data")
{
    CHECK_FALSE(decodeWebp(redWebp.substr(0, 20)));
    CHECK_FALSE(decodeWebp("<html>Not Found</html>"));
    CHECK_FALSE(decodeWebp(""));
}
