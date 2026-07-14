#include <doctest.h>

#if AX_ENABLE_GL
#    include "axmol/rhi/opengl/TextureGL.h"
#    include "axmol/rhi/opengl/UtilsGL.h"

using namespace ax::rhi;

namespace
{
class InspectableTextureGL final : public gl::TextureImpl
{
public:
    using gl::TextureImpl::TextureImpl;

    const SamplerDesc& storedSamplerDesc() const { return _desc.samplerDesc; }
};
}  // namespace

TEST_CASE("OpenGL selects an sRGB internal format for sampled textures")
{
    TextureDesc desc{};
    desc.pixelFormat  = PixelFormat::RGBA8;
    desc.textureUsage = TextureUsage::READ;
    desc.colorSpace   = ColorSpace::Srgb;

    GLint internalFormat = GL_ZERO;
    GLuint format        = GL_ZERO;
    GLenum type          = GL_ZERO;
    gl::UtilsGL::toGLTypes(desc, internalFormat, format, type);
    CHECK(internalFormat == GL_SRGB8_ALPHA8);
    CHECK(format == GL_RGBA);
    CHECK(type == GL_UNSIGNED_BYTE);

    desc.pixelFormat = PixelFormat::ETC2_RGBA;
    gl::UtilsGL::toGLTypes(desc, internalFormat, format, type);
    CHECK(internalFormat == GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC);

    desc.textureUsage = TextureUsage::RENDER_TARGET;
    gl::UtilsGL::toGLTypes(desc, internalFormat, format, type);
    CHECK(internalFormat == GL_COMPRESSED_RGBA8_ETC2_EAC);
}

TEST_CASE("OpenGL retains sampler state for context restoration")
{
    if (!axdrv)
    {
        MESSAGE("RHI integration disabled; rerun with AX_UNIT_TEST_RHI=1");
        return;
    }

    TextureDesc textureDesc{};
    textureDesc.width        = 4;
    textureDesc.height       = 4;
    textureDesc.pixelFormat  = PixelFormat::RGBA8;
    textureDesc.textureUsage = TextureUsage::READ;

    auto* texture = new InspectableTextureGL(textureDesc);
    SamplerDesc sampler{};
    sampler.minFilter    = SamplerFilter::MIN_NEAREST;
    sampler.magFilter    = SamplerFilter::MAG_NEAREST;
    sampler.mipFilter    = SamplerFilter::MIP_LINEAR;
    sampler.sAddressMode = SamplerAddressMode::REPEAT;
    sampler.tAddressMode = SamplerAddressMode::MIRROR;
    texture->updateSamplerDesc(sampler);

    const auto& stored = texture->storedSamplerDesc();
    CHECK(static_cast<uint8_t>(stored.minFilter) == static_cast<uint8_t>(sampler.minFilter));
    CHECK(static_cast<uint8_t>(stored.magFilter) == static_cast<uint8_t>(sampler.magFilter));
    CHECK(static_cast<uint8_t>(stored.mipFilter) == static_cast<uint8_t>(sampler.mipFilter));
    CHECK(static_cast<uint8_t>(stored.sAddressMode) == static_cast<uint8_t>(sampler.sAddressMode));
    CHECK(static_cast<uint8_t>(stored.tAddressMode) == static_cast<uint8_t>(sampler.tAddressMode));
    texture->release();
}
#endif
