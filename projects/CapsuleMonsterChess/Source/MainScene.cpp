#include "MainScene.h"

#include "axmol/2d/Light.h"
#include "axmol/3d/Animate3D.h"
#include "axmol/3d/Animation3D.h"
#include "axmol/3d/Mesh.h"
#include "axmol/3d/MeshRenderer.h"
#include "axmol/3d/StylizedMaterial.h"
#include "axmol/3d/StylizedRenderer.h"
#include "axmol/scene/CameraBackgroundBrush.h"
#include "axmol/renderer/Texture2D.h"

#include <algorithm>
#include <array>

namespace
{
using namespace ax;

constexpr unsigned short WORLD_CAMERA_MASK = static_cast<unsigned short>(CameraFlag::USER1);
constexpr std::string_view DRAGON_ASSET    = "Local/DragonFireFlyIdle.glb";

Texture2D* createDragonToonRamp()
{
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
        const float shadowBand  = 0.18F;
        const float middleBand  = 0.55F;
        float value             = shadowBand + (middleBand - shadowBand) * shadowToMid;
        value += (1.0F - value) * midToLight;
        const auto channel    = static_cast<uint8_t>(std::clamp(value * 255.0F + 0.5F, 0.0F, 255.0F));
        pixels[index * 4]     = channel;
        pixels[index * 4 + 1] = channel;
        pixels[index * 4 + 2] = channel;
        pixels[index * 4 + 3] = 255;
    }

    auto* texture = new Texture2D();
    if (!texture->initWithData(pixels.data(), static_cast<ssize_t>(pixels.size()), rhi::PixelFormat::RGBA8,
                               static_cast<int>(TEXEL_COUNT), 1))
    {
        delete texture;
        return nullptr;
    }
    texture->setTexParameters(Texture2D::TexParams{});
    texture->autorelease();
    return texture;
}

MeshRenderer* createGround(StylizedMaterial* material)
{
    const std::vector<float> positions = {-5.0F, 0.0F, -5.0F, 5.0F, 0.0F, -5.0F, 5.0F, 0.0F, 5.0F, -5.0F, 0.0F, 5.0F};
    const std::vector<float> normals   = {0.0F, 1.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 1.0F, 0.0F};
    const std::vector<float> texCoords = {0.0F, 0.0F, 1.0F, 0.0F, 1.0F, 1.0F, 0.0F, 1.0F};
    const IndexArray indices{uint16_t{0}, uint16_t{2}, uint16_t{1}, uint16_t{0}, uint16_t{3}, uint16_t{2}};

    auto* renderer = MeshRenderer::create();
    renderer->addMesh(Mesh::create(positions, normals, texCoords, indices));
    renderer->setMaterial(material);
    renderer->setCameraMask(WORLD_CAMERA_MASK);
    renderer->setCastShadow(false);
    renderer->setReceiveShadow(true);
    renderer->setScale(8.0F);
    return renderer;
}

void showStatus(Scene& scene, std::string_view text, const Color32& color)
{
    auto* label = Label::createWithTTF(text, "fonts/Marker Felt.ttf", 22.0F);
    if (!label)
        return;

    const auto visibleSize = Director::getInstance()->getVisibleSize();
    label->setTextColor(color);
    label->setAnchorPoint({0.0F, 1.0F});
    label->setPosition({24.0F, visibleSize.height - 20.0F});
    scene.addChild(label);
}

bool addDragon(Scene& scene)
{
    auto* dragon   = MeshRenderer::create(DRAGON_ASSET);
    auto* skeleton = dragon ? dragon->getSkeleton() : nullptr;
    auto* material =
        dragon && dragon->getMeshCount() > 0 ? dynamic_cast<StylizedMaterial*>(dragon->getMaterial(0)) : nullptr;
    auto* animation = dragon ? Animation3D::create(DRAGON_ASSET) : nullptr;

    if (!dragon || !skeleton || !material || !material->isSkinned() || !animation || animation->getDuration() <= 0.0F)
    {
        AXLOGE("Dragon validation failed: asset={}, renderer={}, skeleton={}, material={}, animation={}", DRAGON_ASSET,
               dragon != nullptr, skeleton != nullptr, material != nullptr, animation != nullptr);
        showStatus(scene, "Dragon asset load failed: Content/Local/DragonFireFlyIdle.glb", Color32{255, 96, 96, 255});
        return false;
    }

    auto desc                = material->getDescription();
    desc.highlightColor      = Color::white;
    desc.shadowColor         = Color{0.25F, 0.25F, 0.25F, 1.0F};
    desc.diffuseTint         = Color{1.0F, 0.9290111F, 0.75F, 1.0F};
    desc.toonRampTexture     = createDragonToonRamp();
    desc.bandThreshold       = 0.68F;
    desc.bandSoftness        = 0.25F;
    desc.rampTextureStrength = desc.toonRampTexture ? 0.82F : 0.0F;
    desc.minimumBrightness   = 0.092F;
    desc.unlitStrength       = 0.5F;
    desc.subsurfaceColor     = Color{1.0F, 0.45F, 0.38F, 1.0F};
    desc.subsurfaceStrength  = 0.328F;
    desc.subsurfaceFalloff   = 2.31F;
    desc.rimColor            = Color{1.0F, 0.9F, 0.8F, 1.0F};
    desc.rimStart            = 0.0F;
    desc.rimEnd              = 0.82F;
    desc.rimPower            = 2.2F;
    desc.rimLightThreshold   = 0.04F;
    desc.rimLightSoftness    = 0.14F;
    desc.rimIntensity        = 0.75F;
    if (!material->setDescription(desc))
    {
        AXLOGE("Dragon stylized material setup failed");
        showStatus(scene, "Dragon stylized material setup failed", Color32{255, 96, 96, 255});
        return false;
    }

    const AABB bounds     = dragon->getAABBRecursively();
    const AABB bindBounds = dragon->getMesh()->getAABB();
    const Vec3 bindSize   = bindBounds._max - bindBounds._min;
    const float extent    = std::max({bindSize.x, bindSize.y, bindSize.z});
    if (bounds.isEmpty() || bindBounds.isEmpty() || extent <= 1.0e-4F)
    {
        AXLOGE("Dragon bounds are invalid");
        showStatus(scene, "Dragon bounds are invalid", Color32{255, 96, 96, 255});
        return false;
    }

    constexpr float targetExtent = 4.15F;
    const float scale            = targetExtent / extent;
    const Vec3 center            = (bounds._min + bounds._max) * 0.5F;
    dragon->setScale(scale);
    dragon->setPosition3D({-center.x * scale, 0.03F - bounds._min.y * scale, -center.z * scale});
    dragon->setRotation3D({0.0F, -18.0F, 0.0F});
    dragon->setCameraMask(WORLD_CAMERA_MASK);
    dragon->setCastShadow(true);
    dragon->setReceiveShadow(true);
    dragon->runAction(RepeatForever::create(Animate3D::create(animation)));
    scene.addChild(dragon);

    AXLOGI("Dragon ready: meshes={}, joints={}, animation={}s", dragon->getMeshCount(), skeleton->getBoneCount(),
           animation->getDuration());
    showStatus(scene, "Capsule Monster Chess | JMO Ramp LUT | AC Wide Directional Rim", Color32{245, 242, 255, 255});
    return true;
}
}  // namespace

bool MainScene::init()
{
    if (!Scene::init())
        return false;

    auto* director = Director::getInstance();
    auto* uiCamera = getDefaultCamera();
    uiCamera->setBackgroundBrush(CameraBackgroundBrush::createNoneBrush());

    const auto canvas = director->getCanvasSize();
    auto* worldCamera = Camera::createPerspective(42.0F, canvas.width / canvas.height, 0.1F, 50.0F);
    worldCamera->setCameraFlag(CameraFlag::USER1);
    worldCamera->setDepth(-1);
    worldCamera->setPosition3D({0.0F, 5.8F, 8.2F});
    worldCamera->lookAt({0.0F, 2.15F, 0.0F});
    worldCamera->setBackgroundBrush(
        CameraBackgroundBrush::createColorBrush(Color{0.5176471F, 0.4901961F, 0.6862745F, 1.0F}, 1.0F));
    addChild(worldCamera);

    auto* mainLight = DirectionLight::create(Vec3{0.55F, -1.0F, -0.85F}, Color32{255, 226, 196, 255});
    mainLight->setCameraMask(WORLD_CAMERA_MASK);
    mainLight->setIntensity(1.15F);
    addChild(mainLight);

    auto* ambientLight = AmbientLight::create(Color32{158, 145, 176, 255});
    ambientLight->setCameraMask(WORLD_CAMERA_MASK);
    ambientLight->setIntensity(0.65F);
    addChild(ambientLight);

    StylizedRendererConfig config;
    config.qualityPreset                   = StylizedQualityPreset::Balanced;
    config.reserveDefaultCameraForNativeUi = true;
    auto* stylized                         = StylizedRenderer::attach(*this, config);
    stylized->setMainLight(mainLight);

    StylizedMaterialDesc groundDesc;
    groundDesc.baseTexture  = director->getTextureCache()->getWhiteTexture();
    groundDesc.baseColor    = Color{0.18F, 0.22F, 0.65F, 1.0F};
    groundDesc.rimIntensity = 0.0F;
    addChild(createGround(StylizedMaterial::create(groundDesc)));

    addDragon(*this);
    return true;
}
