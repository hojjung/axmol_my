#include <doctest.h>

#include "axmol/renderer/Texture2D.h"
#include "axmol/rhi/Texture.h"

using namespace ax;

namespace
{
class CapturingTexture final : public rhi::Texture
{
public:
    explicit CapturingTexture(uint16_t mipLevels) { _overrideMipLevels = mipLevels; }

    void updateSamplerDesc(const rhi::SamplerDesc& desc) override { sampler = desc; }
    void updateData(const void*, int, int, int, int) override {}
    void updateCompressedData(const void*, int, int, size_t, int, int) override {}
    void updateSubData(int, int, int, int, int, const void*, int) override {}
    void updateCompressedSubData(int, int, int, int, size_t, int, const void*, int) override {}
    void updateFaceData(rhi::TextureCubeFace, const void*) override {}

    rhi::SamplerDesc sampler{};
};

class TestTexture2D final : public Texture2D
{
public:
    explicit TestTexture2D(uint16_t mipLevels)
    {
        _capturingTexture = new CapturingTexture(mipLevels);
        _rhiTexture       = _capturingTexture;
    }

    const rhi::SamplerDesc& getCapturedSampler() const { return _capturingTexture->sampler; }

private:
    CapturingTexture* _capturingTexture = nullptr;
};
}  // namespace

TEST_CASE("Texture sampler resolution disables unavailable mip filtering")
{
    Texture2D::TexParams requested{};
    requested.minFilter    = rhi::SamplerFilter::MIN_NEAREST;
    requested.magFilter    = rhi::SamplerFilter::MAG_NEAREST;
    requested.mipFilter    = rhi::SamplerFilter::MIP_LINEAR;
    requested.sAddressMode = rhi::SamplerAddressMode::MIRROR;
    requested.tAddressMode = rhi::SamplerAddressMode::REPEAT;

    auto* baseLevelTexture = new TestTexture2D(1);
    baseLevelTexture->setTexParameters(requested);
    const auto& baseLevelOnly = baseLevelTexture->getCapturedSampler();
    CHECK(static_cast<uint8_t>(baseLevelOnly.minFilter) == static_cast<uint8_t>(requested.minFilter));
    CHECK(static_cast<uint8_t>(baseLevelOnly.magFilter) == static_cast<uint8_t>(requested.magFilter));
    CHECK(baseLevelOnly.mipFilter == rhi::SamplerFilter::MIP_DEFAULT);
    CHECK(static_cast<uint8_t>(baseLevelOnly.sAddressMode) == static_cast<uint8_t>(requested.sAddressMode));
    CHECK(static_cast<uint8_t>(baseLevelOnly.tAddressMode) == static_cast<uint8_t>(requested.tAddressMode));
    baseLevelTexture->release();

    auto* mipmappedTexture = new TestTexture2D(4);
    mipmappedTexture->setTexParameters(requested);
    CHECK(static_cast<uint8_t>(mipmappedTexture->getCapturedSampler().mipFilter) ==
          static_cast<uint8_t>(requested.mipFilter));
    mipmappedTexture->release();
}
