#include <array>
#include <cstdlib>
#include <cstring>
#include <limits>

#include <doctest.h>

#include "axmol/2d/Light.h"
#include "axmol/3d/StylizedQuality.h"
#include "axmol/3d/StylizedMaterial.h"
#include "axmol/3d/StylizedRenderSurface.h"
#include "axmol/3d/StylizedRenderer.h"
#include "axmol/base/Director.h"
#include "axmol/renderer/CustomCommand.h"
#include "axmol/renderer/MeshCommand.h"
#include "axmol/renderer/Pass.h"
#include "axmol/renderer/Renderer.h"
#include "axmol/renderer/Technique.h"
#include "axmol/renderer/Texture2D.h"
#include "axmol/renderer/TextureCache.h"
#include "axmol/rhi/ProgramState.h"
#include "axmol/scene/Scene.h"

using namespace ax;

TEST_CASE("Stylized renderer is owned by one scene component")
{
    auto* scene = Scene::create();
    REQUIRE(scene != nullptr);

    StylizedRendererConfig config;
    config.qualityPreset = StylizedQualityPreset::Balanced;
    auto* renderer       = StylizedRenderer::attach(*scene, config);
    REQUIRE(renderer != nullptr);
    CHECK(renderer->getOwner() == scene);
    CHECK(StylizedRenderer::get(*scene) == renderer);
    CHECK(renderer->getResolvedQuality().cascadeCount == 1);

    config.qualityPreset = StylizedQualityPreset::Quality;
    CHECK(StylizedRenderer::attach(*scene, config) == renderer);
    CHECK(renderer->getResolvedQuality().cascadeCount == 2);
}

TEST_CASE("Stylized renderer prioritizes its explicit directional light")
{
    auto* scene    = Scene::create();
    auto* renderer = StylizedRenderer::attach(*scene);
    auto* light    = DirectionLight::create(Vec3{0.0F, -1.0F, 0.0F}, Color32::white);
    REQUIRE(renderer != nullptr);
    REQUIRE(light != nullptr);

    renderer->setMainLight(light);
    CHECK(renderer->resolveMainLight(static_cast<unsigned int>(light->getLightFlag())) == light);

    light->setEnabled(false);
    CHECK(renderer->resolveMainLight() == nullptr);
    renderer->setMainLight(nullptr);
}

TEST_CASE("Reserved default UI camera does not submit a stylized frame")
{
    auto* scene = Scene::create();
    REQUIRE(scene != nullptr);

    StylizedRendererConfig config;
    config.reserveDefaultCameraForNativeUi = true;
    auto* stylizedRenderer                 = StylizedRenderer::attach(*scene, config);
    REQUIRE(stylizedRenderer != nullptr);

    auto* defaultCamera   = scene->getDefaultCamera();
    const auto generation = stylizedRenderer->getResourceGeneration();
    Renderer commandBuffer;

    REQUIRE(defaultCamera != nullptr);
    stylizedRenderer->beginSceneVisit(commandBuffer, *defaultCamera);

    CHECK_FALSE(stylizedRenderer->isFrameActive());
    CHECK(stylizedRenderer->getResourceGeneration() == generation);
}

TEST_CASE("Default camera remains a native stylized world camera unless explicitly reserved")
{
    auto* scene = Scene::create();
    REQUIRE(scene != nullptr);

    StylizedRendererConfig config;
    config.qualityPreset = StylizedQualityPreset::Quality;
    CHECK_FALSE(config.reserveDefaultCameraForNativeUi);
    auto* stylizedRenderer = StylizedRenderer::attach(*scene, config);
    REQUIRE(stylizedRenderer != nullptr);

    auto* renderer = Director::getInstance()->getRenderer();
    auto* camera   = scene->getDefaultCamera();
    REQUIRE(renderer != nullptr);
    REQUIRE(camera != nullptr);

    const size_t initialDepth = renderer->getCommandGroupDepth();
    stylizedRenderer->beginSceneVisit(*renderer, *camera);
    CHECK(stylizedRenderer->isFrameActive());
    CHECK(renderer->getCommandGroupDepth() == initialDepth + 1);
    stylizedRenderer->endSceneVisit(*renderer);
    CHECK_FALSE(stylizedRenderer->isFrameActive());
    CHECK(renderer->getCommandGroupDepth() == initialDepth);
    renderer->render();
}

TEST_CASE("Default camera can drive the stylized directional shadow frame")
{
    if (!std::getenv("AX_UNIT_TEST_RHI"))
        return;

    auto* scene = Scene::create();
    REQUIRE(scene != nullptr);
    StylizedRendererConfig config;
    config.qualityPreset   = StylizedQualityPreset::Quality;
    auto* stylizedRenderer = StylizedRenderer::attach(*scene, config);
    auto* mainLight        = DirectionLight::create(Vec3{-0.7F, -1.0F, -0.4F}, Color32::white);
    REQUIRE(stylizedRenderer != nullptr);
    REQUIRE(mainLight != nullptr);
    stylizedRenderer->setMainLight(mainLight);

    auto* renderer = Director::getInstance()->getRenderer();
    auto* camera   = scene->getDefaultCamera();
    REQUIRE(renderer != nullptr);
    REQUIRE(camera != nullptr);
    Camera::setVisitingCamera(camera);

    stylizedRenderer->beginSceneVisit(*renderer, *camera);
    REQUIRE(stylizedRenderer->isFrameActive());
    auto* material = StylizedMaterial::create();
    REQUIRE(material != nullptr);
    stylizedRenderer->configureMaterial(*material, true);
    CHECK(material->isMainShadowEnabled());
    stylizedRenderer->endSceneVisit(*renderer);
    renderer->render();
    Camera::setVisitingCamera(nullptr);
}

TEST_CASE("Stylized renderer validates debug output mode")
{
    StylizedRendererConfig config;
    config.debugView = StylizedDebugView::RimMask;
    CHECK(config.isValid());

    config.debugView = static_cast<StylizedDebugView>(255);
    CHECK_FALSE(config.isValid());
}

TEST_CASE("Stylized world queue encloses every main global order after the outer shadow group")
{
    CustomCommand shadowGroup;
    CustomCommand worldGroup;
    shadowGroup.init(-9000.0F);
    worldGroup.init(0.0F);

    RenderQueue rootQueue;
    rootQueue.emplace_back(&worldGroup);
    rootQueue.emplace_back(&shadowGroup);
    rootQueue.sort();
    CHECK(rootQueue[0] == &shadowGroup);
    CHECK(rootQueue[1] == &worldGroup);

    CustomCommand bind;
    CustomCommand negativeMain;
    CustomCommand positiveMain;
    CustomCommand restore;
    bind.init(std::numeric_limits<float>::lowest());
    negativeMain.init(-1000000000.0F);
    positiveMain.init(1000000000.0F);
    restore.init(std::numeric_limits<float>::max());

    RenderQueue worldQueue;
    worldQueue.emplace_back(&positiveMain);
    worldQueue.emplace_back(&restore);
    worldQueue.emplace_back(&negativeMain);
    worldQueue.emplace_back(&bind);
    worldQueue.sort();
    CHECK(worldQueue[0] == &bind);
    CHECK(worldQueue[1] == &negativeMain);
    CHECK(worldQueue[2] == &positiveMain);
    CHECK(worldQueue[3] == &restore);
}

TEST_CASE("Stylized surface leaves the renderer group stack symmetric on rejection and unscaled fallback")
{
    auto* renderer = Director::getInstance()->getRenderer();
    auto* scene    = Scene::create();
    REQUIRE(renderer != nullptr);
    REQUIRE(scene != nullptr);
    auto* camera = scene->getDefaultCamera();
    REQUIRE(camera != nullptr);

    StylizedRenderSurface surface;
    const size_t initialDepth = renderer->getCommandGroupDepth();
    CHECK_FALSE(surface.submit(*renderer, *camera, std::numeric_limits<float>::quiet_NaN()));
    CHECK(renderer->getCommandGroupDepth() == initialDepth);

    const Viewport savedViewport = renderer->getViewport();
    renderer->setViewport(0, 0, 0, 0);
    REQUIRE(surface.submit(*renderer, *camera, 0.5F));
    CHECK(surface.isFrameOpen());
    CHECK_FALSE(surface.isScaledFrame());
    CHECK(renderer->getCommandGroupDepth() == initialDepth + 1);
    CHECK(renderer->getCurrentCommandGroupId() == surface.getWorldQueueId());
    CHECK(surface.finish(*renderer));
    CHECK_FALSE(surface.isFrameOpen());
    CHECK(renderer->getCommandGroupDepth() == initialDepth);
    renderer->setViewport(savedViewport.x, savedViewport.y, savedViewport.width, savedViewport.height);
    renderer->render();
}

TEST_CASE("Shared stylized Pass restores command-local color lighting shadow texture and skin palette")
{
    if (!std::getenv("AX_UNIT_TEST_RHI"))
        return;

    auto* material = StylizedMaterial::create({}, true);
    REQUIRE(material != nullptr);
    auto* pass         = material->getTechnique()->getPassByIndex(0);
    auto* programState = pass->getProgramState();
    REQUIRE(programState != nullptr);

    auto* textureA = Director::getInstance()->getTextureCache()->getWhiteTexture();
    auto* textureB = Director::getInstance()->getTextureCache()->getDummyTexture();
    REQUIRE(textureA != nullptr);
    REQUIRE(textureB != nullptr);

    const Vec4 colorA{0.1F, 0.2F, 0.3F, 0.4F};
    const Vec4 colorB{0.9F, 0.8F, 0.7F, 0.6F};
    std::array<Vec4, 9> lightA{};
    std::array<Vec4, 9> lightB{};
    lightA[1].set(1.0F, 2.0F, 3.0F, 4.0F);
    lightB[1].set(5.0F, 6.0F, 7.0F, 8.0F);
    std::array<Vec4, 180> paletteA{};
    std::array<Vec4, 180> paletteB{};
    paletteA[0].set(1.0F, 2.0F, 3.0F, 4.0F);
    paletteB[0].set(9.0F, 8.0F, 7.0F, 6.0F);
    const float receivesShadowA = 0.0F;
    const float receivesShadowB = 1.0F;

    MeshCommand commandA;
    MeshCommand commandB;
    pass->setUniformColor(&colorA, sizeof(colorA));
    pass->setUniformStylizedLighting(lightA.data(), sizeof(lightA));
    pass->setUniformShadowEnabled(&receivesShadowA, sizeof(receivesShadowA));
    pass->setUniformMatrixPalette(paletteA.data(), sizeof(paletteA));
    pass->setUniformTexture(0, textureA->getRHITexture());
    REQUIRE(commandA.captureProgramState(*programState));

    pass->setUniformColor(&colorB, sizeof(colorB));
    pass->setUniformStylizedLighting(lightB.data(), sizeof(lightB));
    pass->setUniformShadowEnabled(&receivesShadowB, sizeof(receivesShadowB));
    pass->setUniformMatrixPalette(paletteB.data(), sizeof(paletteB));
    pass->setUniformTexture(0, textureB->getRHITexture());
    REQUIRE(commandB.captureProgramState(*programState));

    REQUIRE(commandA.restoreProgramState(*programState));
    const auto readUniform = [programState](std::string_view name, void* output, size_t size) {
        const auto location = programState->getUniformLocation(name);
        REQUIRE(location);
        const auto& buffer = programState->getUniformBuffer();
        std::memcpy(output, buffer.data() + location.cpuOffset + location.offset, size);
    };

    Vec4 restoredColor;
    std::array<Vec4, 9> restoredLight{};
    Vec4 restoredPalette;
    float restoredShadow = -1.0F;
    readUniform("u_color", &restoredColor, sizeof(restoredColor));
    readUniform("u_stylizedLightData", restoredLight.data(), sizeof(restoredLight));
    readUniform("u_matrixPalette", &restoredPalette, sizeof(restoredPalette));
    readUniform("u_shadowEnabled", &restoredShadow, sizeof(restoredShadow));
    CHECK(restoredColor == colorA);
    CHECK(restoredLight[1] == lightA[1]);
    CHECK(restoredPalette == paletteA[0]);
    CHECK(restoredShadow == doctest::Approx(receivesShadowA));

    const auto textureLocation = programState->getUniformLocation("u_tex0");
    const auto binding         = programState->getTextureBindingSets().find(textureLocation.location);
    REQUIRE(binding != programState->getTextureBindingSets().end());
    REQUIRE_FALSE(binding->second.texs.empty());
    CHECK(binding->second.texs.front() == textureA->getRHITexture());

    programState->setTexture(textureLocation, 0, nullptr);
    const auto clearedBinding = programState->getTextureBindingSets().find(textureLocation.location);
    REQUIRE(clearedBinding != programState->getTextureBindingSets().end());
    CHECK(clearedBinding->second.texs.empty());
    CHECK(clearedBinding->second.slots.empty());

    REQUIRE(material->setMainShadow(Mat4::identity, textureA->getRHITexture(), Vec2{1.0F / 1024.0F, 1.0F / 1024.0F},
                                    0.001F, true));
    const auto shadowLocation = programState->getUniformLocation("u_mainShadowMap");
    const auto shadowBinding  = programState->getTextureBindingSets().find(shadowLocation.location);
    REQUIRE(shadowBinding != programState->getTextureBindingSets().end());
    REQUIRE_FALSE(shadowBinding->second.texs.empty());
    CHECK(shadowBinding->second.texs.front() == textureA->getRHITexture());

    REQUIRE(material->setMainShadow(Mat4::identity, nullptr, Vec2{1.0F / 1024.0F, 1.0F / 1024.0F}, 0.001F, false));
    const auto clearedShadowBinding = programState->getTextureBindingSets().find(shadowLocation.location);
    REQUIRE(clearedShadowBinding != programState->getTextureBindingSets().end());
    CHECK(clearedShadowBinding->second.texs.empty());
    CHECK(clearedShadowBinding->second.slots.empty());
}
