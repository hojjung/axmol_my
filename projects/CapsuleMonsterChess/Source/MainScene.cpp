#include "MainScene.h"

#include "Client/Battle/BattleScene.h"
#include "Client/Lobby/LobbyLayer.h"
#include "Client/Rendering/DragonFixtureRenderer.h"

#include "axmol/2d/Light.h"
#include "axmol/3d/Mesh.h"
#include "axmol/3d/MeshRenderer.h"
#include "axmol/3d/StylizedMaterial.h"
#include "axmol/3d/StylizedRenderer.h"
#include "axmol/scene/CameraBackgroundBrush.h"

#include <string>

namespace
{
using namespace ax;

constexpr unsigned short WORLD_CAMERA_MASK = static_cast<unsigned short>(CameraFlag::USER1);

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

MeshRenderer* addDragon(Scene& scene)
{
    cmc::client::DragonFixtureStyle style;
    std::string error;
    auto* dragon = cmc::client::createDragonFixture(style, error);
    if (!dragon)
    {
        AXLOGE("Dragon validation failed: {}", error);
        showStatus(scene, "Dragon asset load failed: Content/Local/DragonFireFlyIdle.glb", Color32{255, 96, 96, 255});
        return nullptr;
    }
    scene.addChild(dragon);

    AXLOGI("Dragon ready: meshes={}, joints={}", dragon->getMeshCount(), dragon->getSkeleton()->getBoneCount());
    return dragon;
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

    auto* dragon = addDragon(*this);
    auto* lobby  = cmc::client::LobbyLayer::create();
    lobby->setWorldVisibilityCallback([worldCamera, dragon](bool visible) {
        worldCamera->setVisible(visible);
        if (!dragon)
            return;

        dragon->setVisible(visible);
        if (visible)
            dragon->resume();
        else
            dragon->pause();
    });
    lobby->setBattleLaunchCallback([](const cmc::client::BattlePresentationRequest& request) {
        Director::getInstance()->postTask([request] {
            auto* battleScene = cmc::client::BattleScene::create(request);
            if (!battleScene)
            {
                AXLOGE("Battle scene creation failed: stage={}", request.stageId);
                return;
            }
            Director::getInstance()->replaceScene(battleScene);
        }, Director::TaskTiming::FrameBoundary);
    });
    addChild(lobby);
    return true;
}
