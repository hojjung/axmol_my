#include <doctest.h>

#if AX_ENABLE_MTL
#    include "axmol/rhi/metal/UtilsMTL.h"

using namespace ax::rhi;

TEST_CASE("Metal selects an sRGB pixel format for sampled textures")
{
    TextureDesc desc{};
    desc.pixelFormat  = PixelFormat::RGBA8;
    desc.textureUsage = TextureUsage::READ;
    desc.colorSpace   = ColorSpace::Srgb;
    CHECK(mtl::UtilsMTL::toMTLPixelFormat(desc) == MTLPixelFormatRGBA8Unorm_sRGB);

    desc.pixelFormat = PixelFormat::S3TC_DXT5;
    CHECK(mtl::UtilsMTL::toMTLPixelFormat(desc) == MTLPixelFormatBC3_RGBA_sRGB);

    desc.textureUsage = TextureUsage::RENDER_TARGET;
    CHECK(mtl::UtilsMTL::toMTLPixelFormat(desc) == MTLPixelFormatBC3_RGBA);
}
#endif
