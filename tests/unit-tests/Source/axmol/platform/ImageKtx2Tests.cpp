#include <doctest.h>

#include "axmol/platform/FileUtils.h"
#include "axmol/platform/Image.h"

#include <array>
#include <filesystem>

using namespace ax;

TEST_CASE("KTX2 ETC1S texture transcodes with mipmaps")
{
    const std::filesystem::path fixture =
        std::filesystem::path(__FILE__).parent_path() / "fixtures" / "stylized_albedo.ktx2";

    Image image;
    REQUIRE(image.initWithImageFile(fixture.string()));
    CHECK(image.getFileType() == Image::Format::KTX2);
    CHECK(image.getWidth() == 24);
    CHECK(image.getHeight() == 24);
    CHECK(image.getNumberOfMipmaps() == 5);
    CHECK(image.getColorSpace() == rhi::ColorSpace::Srgb);
    CHECK(image.getPixelFormat() != rhi::PixelFormat::NONE);
    CHECK(image.getData() != nullptr);
    CHECK(image.getDataSize() > 0);

    uint32_t expectedMipLevel = 0;
    for (const MipmapInfo& mip : image.getMipmaps())
    {
        CHECK(mip.data != nullptr);
        CHECK(mip.dataSize > 0);
        CHECK(mip.layerIndex == 0);
        CHECK(mip.mipLevel == expectedMipLevel++);
    }
}

TEST_CASE("truncated KTX2 payload is rejected")
{
    constexpr std::array<uint8_t, 12> signature = {
        0xAB, 0x4B, 0x54, 0x58, 0x20, 0x32, 0x30, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A,
    };

    Image image;
    CHECK_FALSE(image.initWithImageData(signature.data(), signature.size()));
}

TEST_CASE("KTX2 extent larger than the 16-bit RHI contract is rejected")
{
    const std::filesystem::path fixture =
        std::filesystem::path(__FILE__).parent_path() / "fixtures" / "stylized_albedo.ktx2";
    Data payload = FileUtils::getInstance()->getDataFromFile(fixture.string());
    REQUIRE(payload.getSize() > 24);

    // KTX2 pixelWidth is the little-endian uint32 at byte offset 20.
    auto* bytes = payload.getBytes();
    bytes[20]   = 0x00;
    bytes[21]   = 0x00;
    bytes[22]   = 0x01;
    bytes[23]   = 0x00;

    Image image;
    CHECK_FALSE(image.initWithImageData(bytes, payload.getSize()));
}
