#include "Client/Rendering/DragonFixtureRenderer.h"

#include "axmol/3d/Animate3D.h"
#include "axmol/3d/Animation3D.h"
#include "axmol/3d/MeshRenderer.h"
#include "axmol/3d/StylizedMaterial.h"
#include "axmol/renderer/Texture2D.h"

#include <algorithm>
#include <array>
#include <string_view>

namespace
{
using namespace ax;

constexpr std::string_view DRAGON_ASSET    = "Local/DragonFireFlyIdle.glb";
constexpr unsigned short WORLD_CAMERA_MASK = static_cast<unsigned short>(CameraFlag::USER1);

Texture2D* dragonToonRamp()
{
    static Texture2D* texture = nullptr;
    if (texture)
        return texture;

    constexpr size_t TEXEL_COUNT = 32;
    std::array<uint8_t, TEXEL_COUNT * 4> pixels{};
    const auto smoothStep = [](float minimum, float maximum, float value) {
        const float t = std::clamp((value - minimum) / (maximum - minimum), 0.0F, 1.0F);
        return t * t * (3.0F - 2.0F * t);
    };

    for (size_t index = 0; index < TEXEL_COUNT; ++index)
    {
        const float x           = static_cast<float>(index) / static_cast<float>(TEXEL_COUNT - 1);
        const float shadowToMid = smoothStep(0.42F, 0.54F, x);
        const float midToLight  = smoothStep(0.64F, 0.74F, x);
        float value             = 0.18F + (0.55F - 0.18F) * shadowToMid;
        value += (1.0F - value) * midToLight;
        const auto channel    = static_cast<uint8_t>(std::clamp(value * 255.0F + 0.5F, 0.0F, 255.0F));
        pixels[index * 4]     = channel;
        pixels[index * 4 + 1] = channel;
        pixels[index * 4 + 2] = channel;
        pixels[index * 4 + 3] = 255;
    }

    texture = new Texture2D();
    if (!texture->initWithData(pixels.data(), static_cast<ssize_t>(pixels.size()), rhi::PixelFormat::RGBA8,
                               static_cast<int>(TEXEL_COUNT), 1))
    {
        delete texture;
        texture = nullptr;
        return nullptr;
    }
    texture->setTexParameters(Texture2D::TexParams{});
    return texture;
}

Animation3D* dragonAnimation()
{
    static Animation3D* animation = nullptr;
    if (!animation)
    {
        animation = Animation3D::create(DRAGON_ASSET);
        if (animation)
            animation->retain();
    }
    return animation;
}
}  // namespace

namespace cmc::client
{
MeshRenderer* createDragonFixture(const DragonFixtureStyle& style, std::string& error)
{
    error.clear();
    auto* dragon   = MeshRenderer::create(DRAGON_ASSET);
    auto* skeleton = dragon ? dragon->getSkeleton() : nullptr;
    auto* material =
        dragon && dragon->getMeshCount() > 0 ? dynamic_cast<StylizedMaterial*>(dragon->getMaterial(0)) : nullptr;
    auto* animation = dragon ? dragonAnimation() : nullptr;

    if (!dragon || !skeleton || !material || !material->isSkinned() || !animation || animation->getDuration() <= 0.0F)
    {
        error = "Dragon fixture validation failed";
        return nullptr;
    }

    auto description                = material->getDescription();
    description.highlightColor      = Color::white;
    description.shadowColor         = Color{0.25F, 0.25F, 0.25F, 1.0F};
    description.diffuseTint         = style.diffuseTint;
    description.toonRampTexture     = dragonToonRamp();
    description.bandThreshold       = 0.68F;
    description.bandSoftness        = 0.25F;
    description.rampTextureStrength = description.toonRampTexture ? 0.82F : 0.0F;
    description.minimumBrightness   = 0.092F;
    description.unlitStrength       = 0.5F;
    description.subsurfaceColor     = Color{1.0F, 0.45F, 0.38F, 1.0F};
    description.subsurfaceStrength  = 0.328F;
    description.subsurfaceFalloff   = 2.31F;
    description.rimColor            = style.rimColor;
    description.rimStart            = 0.0F;
    description.rimEnd              = 0.82F;
    description.rimPower            = 2.2F;
    description.rimLightThreshold   = 0.04F;
    description.rimLightSoftness    = 0.14F;
    description.rimIntensity        = 0.75F;
    if (!material->setDescription(description))
    {
        error = "Dragon stylized material setup failed";
        return nullptr;
    }

    const AABB bounds     = dragon->getAABBRecursively();
    const AABB bindBounds = dragon->getMesh()->getAABB();
    if (bounds.isEmpty() || bindBounds.isEmpty())
    {
        error = "Dragon fixture bounds are invalid";
        return nullptr;
    }

    const Vec3 bindSize = bindBounds._max - bindBounds._min;
    const float extent  = std::max({bindSize.x, bindSize.y, bindSize.z});
    if (extent <= 1.0e-4F || style.targetExtent <= 0.0F)
    {
        error = "Dragon fixture extent is invalid";
        return nullptr;
    }

    const float scale = style.targetExtent / extent;
    const Vec3 center = (bounds._min + bounds._max) * 0.5F;
    dragon->setScale(scale);
    dragon->setPosition3D({-center.x * scale, 0.03F - bounds._min.y * scale, -center.z * scale});
    dragon->setRotation3D({0.0F, style.yawDegrees, 0.0F});
    dragon->setCameraMask(WORLD_CAMERA_MASK);
    dragon->setCastShadow(true);
    dragon->setReceiveShadow(true);
    dragon->runAction(RepeatForever::create(Animate3D::create(animation)));
    return dragon;
}
}  // namespace cmc::client
