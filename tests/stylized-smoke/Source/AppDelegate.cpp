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
#include "axmol/3d/Mesh.h"
#include "axmol/3d/MeshRenderer.h"
#include "axmol/3d/StylizedMaterial.h"
#include "axmol/3d/StylizedRenderer.h"
#include "axmol/scene/CameraBackgroundBrush.h"

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
                    IndexArray{uint16_t{0}, uint16_t{1}, uint16_t{2}, uint16_t{0}, uint16_t{2}, uint16_t{3}}, material);
}

Scene* makeScene()
{
    auto* director = Director::getInstance();
    auto* scene    = Scene::create();

    auto* uiCamera = scene->getDefaultCamera();
    uiCamera->setBackgroundBrush(CameraBackgroundBrush::createNoneBrush());

    const auto canvas = director->getCanvasSize();
    auto* worldCamera = Camera::createPerspective(50.0F, canvas.width / canvas.height, 0.1F, 50.0F);
    worldCamera->setCameraFlag(CameraFlag::USER1);
    worldCamera->setDepth(-1);
    worldCamera->setPosition3D({0.0F, 3.0F, 8.0F});
    worldCamera->lookAt({0.0F, 0.5F, 0.0F});
    worldCamera->setBackgroundBrush(CameraBackgroundBrush::createColorBrush(Color{0.62F, 0.78F, 0.86F, 1.0F}, 1.0F));
    scene->addChild(worldCamera);

    auto* mainLight = DirectionLight::create(Vec3{-0.7F, -1.0F, -0.4F}, Color32{255, 224, 188, 255});
    mainLight->setCameraMask(WORLD_CAMERA_MASK);
    scene->addChild(mainLight);

    StylizedRendererConfig config;
    config.qualityPreset                   = StylizedQualityPreset::Auto30;
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
    groundDesc.baseColor   = Color{0.46F, 0.72F, 0.38F, 1.0F};
    auto* groundMaterial   = StylizedMaterial::create(groundDesc);

    auto* cube = makeCube(cubeMaterial);
    cube->setPosition3D({0.0F, 1.2F, 0.0F});
    cube->runAction(RepeatForever::create(RotateBy::create(5.0F, Vec3{0.0F, 360.0F, 0.0F})));
    scene->addChild(cube);

    auto* ground = makeGround(groundMaterial);
    scene->addChild(ground);

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

    // This bar remains on CameraFlag::DEFAULT and therefore proves the UI is
    // rendered after the scaled world composite at native canvas resolution.
    auto* nativeUi = DrawNode::create();
    nativeUi->drawSolidRect({24.0F, canvas.height - 42.0F}, {344.0F, canvas.height - 22.0F},
                            Color{0.10F, 0.95F, 0.72F, 1.0F});
    scene->addChild(nativeUi);
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
