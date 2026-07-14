#include <doctest.h>

#if AX_ENABLE_D3D11 || AX_ENABLE_D3D12
#    include "axmol/rhi/DXUtils.h"

using namespace ax::rhi;

TEST_CASE("Direct3D selects sRGB formats only for sampled read textures")
{
    const auto* rgba8 = dxutils::toDxgiFormatInfo(PixelFormat::RGBA8);
    REQUIRE(rgba8 != nullptr);

    TextureDesc sampled{};
    sampled.pixelFormat  = PixelFormat::RGBA8;
    sampled.textureUsage = TextureUsage::READ;
    sampled.colorSpace   = ColorSpace::Srgb;
    CHECK(dxutils::selectTextureResourceFormat(*rgba8, sampled) == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
    CHECK(dxutils::selectTextureSrvFormat(*rgba8, sampled) == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);

    TextureDesc renderTarget  = sampled;
    renderTarget.textureUsage = TextureUsage::RENDER_TARGET;
    CHECK(dxutils::selectTextureResourceFormat(*rgba8, renderTarget) == DXGI_FORMAT_R8G8B8A8_UNORM);
    CHECK(dxutils::selectTextureSrvFormat(*rgba8, renderTarget) == DXGI_FORMAT_R8G8B8A8_UNORM);

    const auto* depth = dxutils::toDxgiFormatInfo(PixelFormat::D24S8);
    REQUIRE(depth != nullptr);
    renderTarget.pixelFormat = PixelFormat::D24S8;
    CHECK(dxutils::selectTextureResourceFormat(*depth, renderTarget) == DXGI_FORMAT_R24G8_TYPELESS);
    CHECK(dxutils::selectTextureSrvFormat(*depth, renderTarget) == DXGI_FORMAT_R24_UNORM_X8_TYPELESS);
}
#endif
