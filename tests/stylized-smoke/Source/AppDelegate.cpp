/****************************************************************************
 Copyright (c) 2019-present Axmol Engine contributors (see AUTHORS.md).

 https://axmol.dev/

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 ****************************************************************************/

#include "AppDelegate.h"

#include "axmol/2d/DrawNode.h"
#include "axmol/2d/Light.h"
#include "axmol/3d/Animate3D.h"
#include "axmol/3d/Animation3D.h"
#include "axmol/3d/Mesh.h"
#include "axmol/3d/MeshRenderer.h"
#include "axmol/3d/StylizedMaterial.h"
#include "axmol/3d/StylizedRenderer.h"
#include "axmol/scene/CameraBackgroundBrush.h"

#include <algorithm>
#include <cmath>
#include <memory>

#if defined(__EMSCRIPTEN__)
#    include <emscripten.h>
#endif

using namespace ax;

namespace
{
constexpr unsigned short WORLD_CAMERA_MASK = static_cast<unsigned short>(CameraFlag::USER1);

MeshRenderer* makeMesh(const std::vector<float>& positions,
                       const std::vector<float>& normals,
                       const std::vector<float>& texCoords,
                       const IndexArray& indices,
                       StylizedMaterial* material)
{
    auto* renderer = MeshRenderer::create();
    auto* mesh     = Mesh::create(positions, normals, texCoords, indices);
    renderer->addMesh(mesh);
    renderer->setMaterial(material);
    renderer->setCameraMask(WORLD_CAMERA_MASK);
    renderer->setCastShadow(true);
    renderer->setReceiveShadow(true);
    return renderer;
}

MeshRenderer* makeCube(StylizedMaterial* material)
{
    const std::vector<float> positions = {
        -1, -1, 1,  1,  -1, 1,  1,  1,  1,  -1, 1,  1,   // front
        1,  -1, -1, -1, -1, -1, -1, 1,  -1, 1,  1,  -1,  // back
        -1, -1, -1, -1, -1, 1,  -1, 1,  1,  -1, 1,  -1,  // left
        1,  -1, 1,  1,  -1, -1, 1,  1,  -1, 1,  1,  1,   // right
        -1, 1,  1,  1,  1,  1,  1,  1,  -1, -1, 1,  -1,  // top
        -1, -1, -1, 1,  -1, -1, 1,  -1, 1,  -1, -1, 1,   // bottom
    };
    const std::vector<float> normals = {
        0,  0, 1, 0,  0, 1, 0,  0, 1, 0,  0, 1, 0, 0,  -1, 0, 0,  -1, 0, 0,  -1, 0, 0,  -1,
        -1, 0, 0, -1, 0, 0, -1, 0, 0, -1, 0, 0, 1, 0,  0,  1, 0,  0,  1, 0,  0,  1, 0,  0,
        0,  1, 0, 0,  1, 0, 0,  1, 0, 0,  1, 0, 0, -1, 0,  0, -1, 0,  0, -1, 0,  0, -1, 0,
    };
    std::vector<float> texCoords;
    texCoords.reserve(48);
    for (int face = 0; face < 6; ++face)
        texCoords.insert(texCoords.end(), {0, 0, 1, 0, 1, 1, 0, 1});

    IndexArray indices(rhi::IndexFormat::U_SHORT);
    for (uint16_t face = 0; face < 6; ++face)
    {
        const uint16_t base = face * 4;
        for (uint16_t index : {base, static_cast<uint16_t>(base + 1), static_cast<uint16_t>(base + 2), base,
                               static_cast<uint16_t>(base + 2), static_cast<uint16_t>(base + 3)})
            indices.push_back(index);
    }
    return makeMesh(positions, normals, texCoords, indices, material);
}

MeshRenderer* makeGround(StylizedMaterial* material)
{
    const std::vector<float> positions = {-5, 0, -5, 5, 0, -5, 5, 0, 5, -5, 0, 5};
    const std::vector<float> normals   = {0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0};
    const std::vector<float> texCoords = {0, 0, 1, 0, 1, 1, 0, 1};
    return makeMesh(positions, normals, texCoords,
                    IndexArray{uint16_t{0}, uint16_t{2}, uint16_t{1}, uint16_t{0}, uint16_t{3}, uint16_t{2}}, material);
}

#if defined(AX_STYLIZED_SMOKE_CHARACTER_GLTF)
void publishCharacterState(bool loaded,
                           bool textured,
                           bool skinned,
                           bool animationLoaded,
                           ssize_t meshCount,
                           ssize_t jointCount,
                           float animationDuration)
{
#    if defined(__EMSCRIPTEN__)
    EM_ASM(
        {
            var state                    = window['axmolStylizedSmoke'] || {};
            state['characterLoaded']     = Boolean($0);
            state['characterTextured']   = Boolean($1);
            state['characterSkinned']    = Boolean($2);
            state['animationLoaded']     = Boolean($3);
            state['characterMeshes']     = $4;
            state['characterJoints']     = $5;
            state['animationDuration']   = $6;
            state['poseChanged']         = false;
            window['axmolStylizedSmoke'] = state;

            var root = document.documentElement;
            root.setAttribute('data-axmol-character-loaded', state['characterLoaded'] ? '1' : '0');
            root.setAttribute('data-axmol-character-textured', state['characterTextured'] ? '1' : '0');
            root.setAttribute('data-axmol-character-skinned', state['characterSkinned'] ? '1' : '0');
            root.setAttribute('data-axmol-animation-loaded', state['animationLoaded'] ? '1' : '0');
            root.setAttribute('data-axmol-character-meshes', String(state['characterMeshes']));
            root.setAttribute('data-axmol-character-joints', String(state['characterJoints']));
            root.setAttribute('data-axmol-animation-duration', String(state['animationDuration']));
            root.setAttribute('data-axmol-pose-changed', '0');
        },
        loaded, textured, skinned, animationLoaded, meshCount, jointCount, animationDuration);
#    else
    (void)loaded;
    (void)textured;
    (void)skinned;
    (void)animationLoaded;
    (void)meshCount;
    (void)jointCount;
    (void)animationDuration;
#    endif
}

void publishPoseChanged(bool changed)
{
#    if defined(__EMSCRIPTEN__)
    EM_ASM(
        {
            var state                    = window['axmolStylizedSmoke'] || {};
            state['poseChanged']         = Boolean($0);
            window['axmolStylizedSmoke'] = state;
            document.documentElement.setAttribute('data-axmol-pose-changed', state['poseChanged'] ? '1' : '0');
        },
        changed);
#    else
    (void)changed;
#    endif
}

MeshRenderer* makeManualCharacter(Scene& scene)
{
    constexpr std::string_view path = AX_STYLIZED_SMOKE_CHARACTER_GLTF;
    auto* character                 = MeshRenderer::create(path);
    auto* skeleton                  = character ? character->getSkeleton() : nullptr;
    auto* material = character && character->getMeshCount() > 0
                         ? dynamic_cast<StylizedMaterial*>(character->getMaterial(0))
                         : nullptr;
    auto* animation = character ? Animation3D::create(path) : nullptr;

    const bool textured = material && material->getDescription().baseTexture;
    const bool skinned = skeleton && character->getMesh() && character->getMesh()->getSkin() && material &&
                         material->isSkinned();
    publishCharacterState(character != nullptr, textured, skinned, animation != nullptr,
                          character ? character->getMeshCount() : 0, skeleton ? skeleton->getBoneCount() : 0,
                          animation ? animation->getDuration() : 0.0F);

    AXASSERT(character && textured && skinned && animation && animation->getDuration() > 0.0F,
             "manual stylized character must load its texture, skin, and animation");
    if (!character || !textured || !skinned || !animation || animation->getDuration() <= 0.0F)
        return nullptr;

    character->setCameraMask(WORLD_CAMERA_MASK);
    character->setCastShadow(true);
    character->setReceiveShadow(true);

    auto characterMaterial = material->getDescription();
    characterMaterial.shadowColor = Color{0.32F, 0.32F, 0.32F, 1.0F};
    characterMaterial.bandThreshold = 0.68F;
    characterMaterial.bandSoftness  = 0.18F;
    characterMaterial.rimColor      = Color{1.0F, 0.72F, 0.42F, 1.0F};
    characterMaterial.rimStart      = 0.76F;
    characterMaterial.rimIntensity  = 0.55F;
    const bool materialTuned = material->setDescription(characterMaterial);
    AXASSERT(materialTuned, "manual stylized character material tuning failed");
    if (!materialTuned)
        return nullptr;

    const AABB bounds     = character->getAABBRecursively();
    const AABB bindBounds = character->getMesh()->getAABB();
    const Vec3 bindSize   = bindBounds._max - bindBounds._min;
    const float bindExtent = std::max({bindSize.x, bindSize.y, bindSize.z});
    if (!bounds.isEmpty() && !bindBounds.isEmpty() && bindExtent > 1.0e-4F)
    {
        constexpr float targetExtent = 4.15F;
        const float scale            = targetExtent / bindExtent;
        const Vec3 center            = (bounds._min + bounds._max) * 0.5F;
        character->setScale(scale);
        character->setPosition3D({-center.x * scale, 0.03F - bounds._min.y * scale, -center.z * scale});
        character->setRotation3D({0.0F, -18.0F, 0.0F});
    }
    auto* wing = skeleton->getBoneByName("RigLWing03");
    if (!wing && skeleton->getBoneCount() > 1)
        wing = skeleton->getBoneByIndex(1);
    AXASSERT(wing, "manual stylized character must expose an animated probe bone");

    character->runAction(RepeatForever::create(Animate3D::create(animation)));
    scene.addChild(character);

    if (wing)
    {
        auto firstPose = std::make_shared<Mat4>(Mat4::identity);
        auto* probe    = Node::create();
        probe->runAction(Sequence::create(
            DelayTime::create(0.2F), CallFunc::create([wing, firstPose] { *firstPose = wing->getWorldMat(); }),
            DelayTime::create(0.37F),
            CallFunc::create([wing, firstPose] {
                const Mat4& secondPose = wing->getWorldMat();
                float maximumDelta     = 0.0F;
                for (size_t index = 0; index < 16; ++index)
                    maximumDelta = std::max(maximumDelta, std::abs(secondPose.m[index] - firstPose->m[index]));
                publishPoseChanged(maximumDelta > 1.0e-4F);
            }),
            nullptr));
        scene.addChild(probe);
    }
    return character;
}
#endif

Scene* makeScene()
{
    auto* director = Director::getInstance();
    auto* scene    = Scene::create();

    auto* uiCamera = scene->getDefaultCamera();
    uiCamera->setBackgroundBrush(CameraBackgroundBrush::createNoneBrush());

    const auto canvas = director->getCanvasSize();
#if defined(AX_STYLIZED_SMOKE_CHARACTER_GLTF)
    auto* worldCamera = Camera::createPerspective(42.0F, canvas.width / canvas.height, 0.1F, 50.0F);
#else
    auto* worldCamera = Camera::createPerspective(50.0F, canvas.width / canvas.height, 0.1F, 50.0F);
#endif
    worldCamera->setCameraFlag(CameraFlag::USER1);
    worldCamera->setDepth(-1);
#if defined(AX_STYLIZED_SMOKE_CHARACTER_GLTF)
    // Keep the whole view below the horizon so the character reads against a
    // single pastel playfield, as it will in the product camera.
    worldCamera->setPosition3D({0.0F, 5.8F, 8.2F});
    worldCamera->lookAt({0.0F, 2.15F, 0.0F});
#else
    worldCamera->setPosition3D({0.0F, 3.0F, 8.0F});
    worldCamera->lookAt({0.0F, 0.5F, 0.0F});
#endif
#if defined(AX_STYLIZED_SMOKE_CHARACTER_GLTF)
    worldCamera->setBackgroundBrush(
        CameraBackgroundBrush::createColorBrush(Color{0.5176471F, 0.4901961F, 0.6862745F, 1.0F}, 1.0F));
#else
    worldCamera->setBackgroundBrush(CameraBackgroundBrush::createColorBrush(Color{0.62F, 0.78F, 0.86F, 1.0F}, 1.0F));
#endif
    scene->addChild(worldCamera);

#if defined(AX_STYLIZED_SMOKE_CHARACTER_GLTF)
    auto* mainLight = DirectionLight::create(Vec3{0.55F, -1.0F, -0.85F}, Color32{255, 226, 196, 255});
#else
    auto* mainLight = DirectionLight::create(Vec3{-0.7F, -1.0F, -0.4F}, Color32{255, 224, 188, 255});
#endif
    mainLight->setCameraMask(WORLD_CAMERA_MASK);
#if defined(AX_STYLIZED_SMOKE_CHARACTER_GLTF)
    mainLight->setIntensity(1.15F);
#endif
    scene->addChild(mainLight);

#if defined(AX_STYLIZED_SMOKE_CHARACTER_GLTF)
    auto* ambientLight = AmbientLight::create(Color32{158, 145, 176, 255});
    ambientLight->setIntensity(0.65F);
    ambientLight->setCameraMask(WORLD_CAMERA_MASK);
    scene->addChild(ambientLight);
#endif

    StylizedRendererConfig config;
#if defined(AX_STYLIZED_SMOKE_CHARACTER_GLTF)
    config.qualityPreset = StylizedQualityPreset::Balanced;
#else
    config.qualityPreset                   = StylizedQualityPreset::Auto30;
#endif
    config.reserveDefaultCameraForNativeUi = true;
    auto* stylized                         = StylizedRenderer::attach(*scene, config);
    stylized->setMainLight(mainLight);

    // Exercise the memory-backed glTF texture path: only the compact encoded
    // KTX2 bytes are retained for context restore, never decoded RGBA pixels.
    const Data encodedAlbedo = FileUtils::getInstance()->getDataFromFile("stylized_albedo.ktx2");
    auto* albedo = director->getTextureCache()->addImage(encodedAlbedo, "stylized-smoke://encoded-albedo.ktx2");
    StylizedMaterialDesc cubeDesc;
    cubeDesc.baseTexture = albedo;
    cubeDesc.baseColor   = Color{0.96F, 0.55F, 0.36F, 1.0F};
    auto* cubeMaterial   = StylizedMaterial::create(cubeDesc);

    StylizedMaterialDesc groundDesc;
    groundDesc.baseTexture = director->getTextureCache()->getWhiteTexture();
#if defined(AX_STYLIZED_SMOKE_CHARACTER_GLTF)
    groundDesc.baseColor = Color{0.18F, 0.22F, 0.65F, 1.0F};
    groundDesc.rimIntensity = 0.0F;
#else
    groundDesc.baseColor   = Color{0.46F, 0.72F, 0.38F, 1.0F};
#endif
    auto* groundMaterial   = StylizedMaterial::create(groundDesc);

    auto* ground = makeGround(groundMaterial);
#if defined(AX_STYLIZED_SMOKE_CHARACTER_GLTF)
    ground->setScale(8.0F);
    ground->setCastShadow(false);
#endif
    scene->addChild(ground);

#if defined(AX_STYLIZED_SMOKE_CHARACTER_GLTF)
    auto* manualCharacter = makeManualCharacter(*scene);
    AXASSERT(manualCharacter, "manual stylized character smoke setup failed");
#else
    auto* cube = makeCube(cubeMaterial);
    cube->setPosition3D({0.0F, 1.2F, 0.0F});
    cube->runAction(RepeatForever::create(RotateBy::create(5.0F, Vec3{0.0F, 360.0F, 0.0F})));
    scene->addChild(cube);

    auto* meshoptTriangle = MeshRenderer::create("meshopt.gltf");
    auto* importedMeshNode =
        meshoptTriangle ? dynamic_cast<MeshRenderer*>(meshoptTriangle->getChildByName("MeshoptTriangle")) : nullptr;
    AXASSERT(meshoptTriangle && importedMeshNode && meshoptTriangle->getMeshCount() == 1 &&
                 importedMeshNode->getMeshCount() == 1 && meshoptTriangle->getMesh() == importedMeshNode->getMesh(),
             "stylized smoke must load the EXT_meshopt_compression fixture");
    if (meshoptTriangle && importedMeshNode)
    {
        meshoptTriangle->setCameraMask(WORLD_CAMERA_MASK);
        meshoptTriangle->setLightMask(0x1234U);
        meshoptTriangle->setForceDepthWrite(true);
        meshoptTriangle->setWireframe(true);
        meshoptTriangle->setCastShadow(false);
        meshoptTriangle->setReceiveShadow(false);
        AXASSERT(importedMeshNode->getCameraMask() == WORLD_CAMERA_MASK &&
                     importedMeshNode->getLightMask() == 0x1234U && importedMeshNode->isForceDepthWrite() &&
                     importedMeshNode->isWireframe() && !importedMeshNode->isCastingShadow() &&
                     !importedMeshNode->isReceivingShadow(),
                 "glTF asset-root state must propagate to its imported mesh nodes");
        meshoptTriangle->setLightMask(0xFFFFU);
        meshoptTriangle->setWireframe(false);
        meshoptTriangle->setCastShadow(true);
        meshoptTriangle->setReceiveShadow(true);
        meshoptTriangle->setPosition3D({2.0F, 1.0F, 0.0F});
        meshoptTriangle->setScale(1.5F);
        scene->addChild(meshoptTriangle);
    }

#if defined(__EMSCRIPTEN__)
    EM_ASM(
        {
            var gltfLoaded                             = Boolean($0);
            var meshCount                              = $1;
            window['axmolStylizedSmoke']               = {};
            window['axmolStylizedSmoke']['gltfLoaded'] = gltfLoaded;
            window['axmolStylizedSmoke']['meshCount']  = meshCount;
            document.documentElement.setAttribute('data-axmol-gltf-loaded', gltfLoaded ? '1' : '0');
            document.documentElement.setAttribute('data-axmol-mesh-count', String(meshCount));
            var canvas = document.getElementById('canvas');
            if (canvas)
            {
                canvas.setAttribute('data-axmol-gltf-loaded', gltfLoaded ? '1' : '0');
                canvas.setAttribute('data-axmol-mesh-count', String(meshCount));
            }
        },
        meshoptTriangle != nullptr, meshoptTriangle ? meshoptTriangle->getMeshCount() : 0);
#endif
#endif

#if !defined(AX_STYLIZED_SMOKE_CHARACTER_GLTF)
    // This bar remains on CameraFlag::DEFAULT and therefore proves the UI is
    // rendered after the scaled world composite at native canvas resolution.
    auto* nativeUi = DrawNode::create();
    nativeUi->drawSolidRect({24.0F, canvas.height - 42.0F}, {344.0F, canvas.height - 22.0F},
                            Color{0.10F, 0.95F, 0.72F, 1.0F});
    scene->addChild(nativeUi);
#endif
    return scene;
}
}  // namespace

void AppDelegate::applicationWillLaunch()
{
    ContextAttrs attributes{};
    attributes.renderScaleMode = RenderScaleMode::Physical;
    setContextAttrs(attributes);
}

bool AppDelegate::applicationDidFinishLaunching()
{
    auto* director   = Director::getInstance();
    auto* renderView = director->getRenderView();
    if (!renderView)
    {
        renderView = RenderView::create("Axmol Stylized Web Smoke");
        director->setRenderView(renderView);
    }

    renderView->setWindowSize(1280.0F, 720.0F);
    renderView->setDesignResolutionSize(1280.0F, 720.0F, ResolutionPolicy::SHOW_ALL);
    director->setAnimationInterval(1.0F / 30.0F);
    director->runWithScene(makeScene());
    return true;
}

void AppDelegate::applicationDidEnterBackground()
{
    Director::getInstance()->stopAnimation();
}

void AppDelegate::applicationWillEnterForeground()
{
    Director::getInstance()->startAnimation();
}
