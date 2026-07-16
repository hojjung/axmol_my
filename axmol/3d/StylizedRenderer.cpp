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

#include "axmol/3d/StylizedRenderer.h"

#include <algorithm>
#include <array>
#include <cmath>

#include "axmol/2d/Light.h"
#include "axmol/3d/Mesh.h"
#include "axmol/3d/MeshRenderer.h"
#include "axmol/3d/StylizedMaterial.h"
#include "axmol/3d/StylizedQuality.h"
#include "axmol/3d/StylizedRenderSurface.h"
#include "axmol/base/CustomEventListener.h"
#include "axmol/base/Director.h"
#include "axmol/base/EventDispatcher.h"
#include "axmol/base/EventType.h"
#include "axmol/base/Logging.h"
#include "axmol/renderer/CallbackCommand.h"
#include "axmol/renderer/GroupCommand.h"
#include "axmol/renderer/RenderTexture.h"
#include "axmol/renderer/Renderer.h"
#include "axmol/rhi/GraphicsCore.h"
#include "axmol/scene/Camera.h"
#include "axmol/scene/Scene.h"

namespace ax
{
namespace
{
constexpr float SHADOW_GROUP_ORDER    = -9000.0F;
constexpr float SHADOW_BEGIN_ORDER    = -100000.0F;
constexpr float SHADOW_CLEAR_ORDER    = -5000.0F;
constexpr float SHADOW_END_ORDER      = 100000.0F;
constexpr float CASCADE_SPLIT_RATIO   = 0.35F;
constexpr float MINIMUM_SHADOW_RADIUS = 0.25F;
constexpr float RADIUS_QUANTIZATION   = 16.0F;
constexpr float SHADOW_NEAR_PLANE     = 0.01F;
constexpr Vec3 LUMINANCE_WEIGHTS{0.2126F, 0.7152F, 0.0722F};

float srgbToLinear(uint8_t channel) noexcept
{
    const float value = static_cast<float>(channel) * (1.0F / 255.0F);
    return value <= 0.04045F ? value * (1.0F / 12.92F) : std::pow((value + 0.055F) * (1.0F / 1.055F), 2.4F);
}

Vec4 linearLightColor(const BaseLight& light) noexcept
{
    const Color32& color  = light.getDisplayedColor();
    const float intensity = light.getIntensity();
    return {srgbToLinear(color.r) * intensity, srgbToLinear(color.g) * intensity, srgbToLinear(color.b) * intensity,
            1.0F};
}

Vec3 unprojectNdc(const Mat4& inverseViewProjection, float x, float y, float z)
{
    Vec4 world = inverseViewProjection * Vec4{x, y, z, 1.0F};
    if (std::abs(world.w) <= 1.0e-6F)
        return Vec3::zero;
    const float inverseW = 1.0F / world.w;
    return {world.x * inverseW, world.y * inverseW, world.z * inverseW};
}

bool isAabbOutsideClipPlane(const std::array<Vec4, 8>& corners, int plane, bool zeroToOneDepth)
{
    return std::all_of(corners.begin(), corners.end(), [plane, zeroToOneDepth](const Vec4& corner) {
        switch (plane)
        {
        case 0:
            return corner.x < -corner.w;
        case 1:
            return corner.x > corner.w;
        case 2:
            return corner.y < -corner.w;
        case 3:
            return corner.y > corner.w;
        case 4:
            return corner.z < (zeroToOneDepth ? 0.0F : -corner.w);
        case 5:
            return corner.z > corner.w;
        default:
            return false;
        }
    });
}
}  // namespace

bool StylizedRendererConfig::isValid() const noexcept
{
    return std::isfinite(shadowDistance) && std::isfinite(shadowBias) && std::isfinite(shadowNormalBias) &&
           std::isfinite(minimumRenderScale) && std::isfinite(maximumRenderScale) && shadowDistance > 0.0F &&
           shadowBias >= 0.0F && shadowNormalBias >= 0.0F && minimumRenderScale > 0.0F &&
           minimumRenderScale <= maximumRenderScale && maximumRenderScale <= 1.0F &&
           maximumPointLights <= MAX_POINT_LIGHTS &&
           static_cast<uint8_t>(debugView) <= static_cast<uint8_t>(StylizedDebugView::RimMask);
}

StylizedRenderer::StylizedRenderer()
{
    setName(COMPONENT_NAME);
}

StylizedRenderer::~StylizedRenderer()
{
    if (_rendererRecreatedListener)
        Director::getInstance()->getEventDispatcher()->removeEventListener(_rendererRecreatedListener);
    AX_SAFE_RELEASE(_mainLight);
    AX_SAFE_RELEASE(_shadowTarget);
}

StylizedRenderer* StylizedRenderer::attach(Scene& scene, const StylizedRendererConfig& config)
{
    if (auto* existing = get(scene))
    {
        return existing->setConfig(config) ? existing : nullptr;
    }

    auto* renderer = new StylizedRenderer();
    if (!renderer->init(config) || !scene.addComponent(renderer))
    {
        renderer->release();
        return nullptr;
    }

    renderer->autorelease();
    return renderer;
}

StylizedRenderer* StylizedRenderer::get(const Scene& scene) noexcept
{
    return dynamic_cast<StylizedRenderer*>(const_cast<Scene&>(scene).getComponent(COMPONENT_NAME));
}

bool StylizedRenderer::init(const StylizedRendererConfig& config)
{
    if (!Component::init() || !config.isValid())
        return false;

    _qualityController = std::make_unique<StylizedQualityController>(config);
    _renderSurface     = std::make_unique<StylizedRenderSurface>();
    _rendererRecreatedListener =
        CustomEventListener::create(EVENT_RENDERER_RECREATED, [this](CustomEvent*) { invalidateResources(); });
    // Drop renderer-owned targets before RenderTexture's -2 restore listener
    // runs, avoiding a redundant recreate immediately followed by release.
    Director::getInstance()->getEventDispatcher()->addEventListenerWithFixedPriority(_rendererRecreatedListener, -3);
    return true;
}

bool StylizedRenderer::setConfig(const StylizedRendererConfig& config)
{
    if (!config.isValid() || !_qualityController->setConfig(config))
        return false;
    invalidateResources();
    return true;
}

const StylizedRendererConfig& StylizedRenderer::getConfig() const noexcept
{
    return _qualityController->getConfig();
}

const StylizedResolvedQuality& StylizedRenderer::getResolvedQuality() const noexcept
{
    return _qualityController->getResolvedQuality();
}

void StylizedRenderer::setMainLight(DirectionLight* light)
{
    Object::assign(_mainLight, light);
}

DirectionLight* StylizedRenderer::resolveMainLight(unsigned int lightMask) const noexcept
{
    const auto isUsable = [lightMask](DirectionLight* light) {
        return light && light->isEnabled() && (static_cast<unsigned int>(light->getLightFlag()) & lightMask) != 0U;
    };

    if (isUsable(_mainLight))
        return _mainLight;

    const auto* scene = dynamic_cast<const Scene*>(getOwner());
    if (!scene)
        return nullptr;

    for (auto* light : scene->getLights())
    {
        if (light->getLightType() != LightType::DIRECTIONAL)
            continue;
        auto* directional = static_cast<DirectionLight*>(light);
        if (isUsable(directional))
            return directional;
    }
    return nullptr;
}

void StylizedRenderer::recordGpuFrameTime(float milliseconds, bool disjoint)
{
    _qualityController->recordGpuFrameTime(milliseconds, disjoint);
}

void StylizedRenderer::update(float delta)
{
    if (isEnabled())
        _qualityController->recordCpuFrameTime(delta * 1000.0F);
}

void StylizedRenderer::onRemove()
{
    resetFrameState();
}

void StylizedRenderer::onExit()
{
    resetFrameState();
}

void StylizedRenderer::invalidateResources()
{
    resetFrameState();
    AX_SAFE_RELEASE_NULL(_shadowTarget);
    if (_renderSurface)
        _renderSurface->invalidate();
    _allocatedResolution   = 0;
    _allocatedCascadeCount = 0;
    ++_resourceGeneration;
}

bool StylizedRenderer::ensureShadowTarget()
{
    const auto& quality        = getResolvedQuality();
    const uint8_t cascadeCount = std::clamp<uint8_t>(quality.cascadeCount, 1, MAX_CASCADES);
    if (_shadowTarget && _allocatedResolution == quality.shadowResolution && _allocatedCascadeCount == cascadeCount)
    {
        return true;
    }

    AX_SAFE_RELEASE_NULL(_shadowTarget);
    const int atlasWidth = static_cast<int>(quality.shadowResolution) * cascadeCount;
    _shadowTarget = RenderTexture::createDepthOnly(atlasWidth, quality.shadowResolution, rhi::PixelFormat::D24S8);
    if (!_shadowTarget)
    {
        AXLOGE("StylizedRenderer: failed to create {}x{} D24S8 shadow atlas", atlasWidth, quality.shadowResolution);
        _allocatedResolution   = 0;
        _allocatedCascadeCount = 0;
        return false;
    }

    _shadowTarget->retain();
    _allocatedResolution   = quality.shadowResolution;
    _allocatedCascadeCount = cascadeCount;
    ++_resourceGeneration;
    return true;
}

bool StylizedRenderer::updateShadowCascades(const Camera& camera, DirectionLight& light)
{
    Vec3 lightDirection = light.getDirectionInWorld();
    if (lightDirection.lengthSquared() <= 1.0e-8F)
        return false;
    lightDirection.normalize();

    const auto& quality = getResolvedQuality();
    _activeCascadeCount = std::clamp<uint8_t>(quality.cascadeCount, 1, MAX_CASCADES);

    const float cameraNear = std::max(camera.getNearPlane(), 0.01F);
    const float cameraFar  = std::max(camera.getFarPlane(), cameraNear + 0.01F);
    const float shadowFar =
        std::clamp(getConfig().shadowDistance * quality.shadowDistanceScale, cameraNear + 0.01F, cameraFar);
    _cascadeSplitDistance =
        _activeCascadeCount == 2 ? cameraNear + (shadowFar - cameraNear) * CASCADE_SPLIT_RATIO : shadowFar;

    const Mat4 inverseViewProjection = camera.getViewProjectionMatrix().getInversed();
    const float nearClipZ            = rhi::GraphicsCore::isOpenGL() ? -1.0F : 0.0F;
    std::array<Vec3, 4> fullNear{};
    std::array<Vec3, 4> fullFar{};
    constexpr std::array<Vec2, 4> ndcCorners = {Vec2{-1.0F, -1.0F}, Vec2{1.0F, -1.0F}, Vec2{1.0F, 1.0F},
                                                Vec2{-1.0F, 1.0F}};
    for (size_t corner = 0; corner < ndcCorners.size(); ++corner)
    {
        fullNear[corner] = unprojectNdc(inverseViewProjection, ndcCorners[corner].x, ndcCorners[corner].y, nearClipZ);
        fullFar[corner]  = unprojectNdc(inverseViewProjection, ndcCorners[corner].x, ndcCorners[corner].y, 1.0F);
    }

    const float inverseCameraRange = 1.0F / (cameraFar - cameraNear);
    for (uint8_t cascade = 0; cascade < _activeCascadeCount; ++cascade)
    {
        const float cascadeNearDistance = cascade == 0 ? cameraNear : _cascadeSplitDistance;
        const float cascadeFarDistance  = cascade == 0 && _activeCascadeCount == 2 ? _cascadeSplitDistance : shadowFar;
        const float nearT = std::clamp((cascadeNearDistance - cameraNear) * inverseCameraRange, 0.0F, 1.0F);
        const float farT  = std::clamp((cascadeFarDistance - cameraNear) * inverseCameraRange, 0.0F, 1.0F);

        std::array<Vec3, 8> corners{};
        Vec3 center = Vec3::zero;
        for (size_t corner = 0; corner < fullNear.size(); ++corner)
        {
            const Vec3 ray      = fullFar[corner] - fullNear[corner];
            corners[corner]     = fullNear[corner] + ray * nearT;
            corners[corner + 4] = fullNear[corner] + ray * farT;
            center += corners[corner];
            center += corners[corner + 4];
        }
        center *= 1.0F / static_cast<float>(corners.size());

        float radius = MINIMUM_SHADOW_RADIUS;
        for (const auto& corner : corners)
            radius = std::max(radius, (corner - center).length());
        radius = std::ceil(radius * RADIUS_QUANTIZATION) / RADIUS_QUANTIZATION;

        const float casterExtrusion = getConfig().shadowDistance * quality.shadowDistanceScale;
        const float eyeDistance     = radius + casterExtrusion + SHADOW_NEAR_PLANE;
        const float depthRange      = radius * 2.0F + casterExtrusion + SHADOW_NEAR_PLANE;
        Vec3 up                     = std::abs(lightDirection.dot(Vec3::yAxis)) > 0.95F ? Vec3::zAxis : Vec3::yAxis;
        const Vec3 eye              = center - lightDirection * eyeDistance;
        Mat4 lightView;
        Mat4::createLookAt(eye, center, up, &lightView);
        Mat4 lightProjection;
        Mat4::createOrthographic(radius * 2.0F, radius * 2.0F, SHADOW_NEAR_PLANE, depthRange, &lightProjection);

        // Stable shadows: snap the projected world origin to the cascade texel grid.
        const Mat4 unsnapped   = lightProjection * lightView;
        Vec4 shadowOrigin      = unsnapped * Vec4{0.0F, 0.0F, 0.0F, 1.0F};
        const float texelScale = static_cast<float>(quality.shadowResolution) * 0.5F;
        shadowOrigin.x *= texelScale;
        shadowOrigin.y *= texelScale;
        lightProjection.m[12] += (std::round(shadowOrigin.x) - shadowOrigin.x) / texelScale;
        lightProjection.m[13] += (std::round(shadowOrigin.y) - shadowOrigin.y) / texelScale;
        _lightViewProjection[cascade] = lightProjection * lightView;

        Mat4 textureBias;
        textureBias.m[0]  = 0.5F;
        textureBias.m[5]  = rhi::GraphicsCore::isOpenGL() ? 0.5F : -0.5F;
        textureBias.m[10] = rhi::GraphicsCore::isOpenGL() ? 0.5F : 1.0F;
        textureBias.m[12] = 0.5F;
        textureBias.m[13] = 0.5F;
        textureBias.m[14] = rhi::GraphicsCore::isOpenGL() ? 0.5F : 0.0F;

        Mat4 atlasTransform;
        atlasTransform.m[0]            = 1.0F / static_cast<float>(_activeCascadeCount);
        atlasTransform.m[12]           = static_cast<float>(cascade) / static_cast<float>(_activeCascadeCount);
        _worldToShadowTexture[cascade] = atlasTransform * textureBias * _lightViewProjection[cascade];
    }

    for (uint8_t cascade = _activeCascadeCount; cascade < MAX_CASCADES; ++cascade)
    {
        _lightViewProjection[cascade]  = _lightViewProjection[0];
        _worldToShadowTexture[cascade] = _worldToShadowTexture[0];
    }
    return true;
}

void StylizedRenderer::cacheLighting(const Camera& camera)
{
    _cachedAmbientLights.clear();
    _cachedPointLights.clear();
    _cachedMainDirection.set(0.0F, -1.0F, 0.0F, 0.0F);
    _cachedMainColor.set(0.0F, 0.0F, 0.0F, 0.0F);
    _cachedMainMask = 0;
    _frameMainLight = resolveMainLight();

    const Mat4 cameraTransform = camera.getNodeToWorldTransform();
    _cachedCameraPosition.set(cameraTransform.m[12], cameraTransform.m[13], cameraTransform.m[14], 1.0F);
    Vec3 cameraForward;
    cameraTransform.getForwardVector(&cameraForward);
    if (cameraForward.lengthSquared() > 1.0e-8F)
        cameraForward.normalize();
    else
        cameraForward.set(0.0F, 0.0F, -1.0F);
    _cachedCameraForward.set(cameraForward.x, cameraForward.y, cameraForward.z, 0.0F);

    if (_frameMainLight)
    {
        Vec3 direction = _frameMainLight->getDirectionInWorld();
        if (direction.lengthSquared() > 1.0e-8F)
        {
            direction.normalize();
            _cachedMainDirection.set(direction.x, direction.y, direction.z, 1.0F);
            _cachedMainColor = linearLightColor(*_frameMainLight);
            _cachedMainMask  = static_cast<uint32_t>(_frameMainLight->getLightFlag());
        }
    }

    const auto* scene = dynamic_cast<const Scene*>(getOwner());
    if (scene)
    {
        _cachedAmbientLights.reserve(scene->getLights().size());
        _cachedPointLights.reserve(scene->getLights().size());
        for (auto* light : scene->getLights())
        {
            if (!light || !light->isEnabled())
                continue;

            const uint32_t mask = static_cast<uint32_t>(light->getLightFlag());
            if (light->getLightType() == LightType::AMBIENT)
            {
                _cachedAmbientLights.push_back({linearLightColor(*light), mask});
                continue;
            }

            if (light->getLightType() != LightType::POINT)
                continue;

            const auto* point = static_cast<const PointLight*>(light);
            const float range = point->getRange();
            if (!std::isfinite(range) || range <= 0.0F)
                continue;

            const Mat4 pointTransform = point->getNodeToWorldTransform();
            _cachedPointLights.push_back(
                {Vec4{pointTransform.m[12], pointTransform.m[13], pointTransform.m[14], 1.0F / range},
                 linearLightColor(*point), mask});
        }
    }
    _lightingCacheValid = true;
}

bool StylizedRenderer::fillStylizedLightData(std::array<Vec4, 9>& lightData,
                                             unsigned int lightMask,
                                             const Vec3& objectPosition,
                                             uint8_t maximumPointLights) const noexcept
{
    if (!_lightingCacheValid)
        return false;

    struct PointCandidate
    {
        float score = 0.0F;
        Vec4 positionAndInverseRange{};
        Vec4 color{};
    };

    lightData = {};
    lightData[0].set(0.0F, -1.0F, 0.0F, 0.0F);
    lightData[3] = _cachedCameraPosition;
    lightData[4] = _cachedCameraForward;

    if ((_cachedMainMask & lightMask) != 0U)
    {
        lightData[0] = _cachedMainDirection;
        lightData[1] = _cachedMainColor;
    }

    for (const auto& ambient : _cachedAmbientLights)
    {
        if ((ambient.mask & lightMask) == 0U)
            continue;
        lightData[2].x += ambient.color.x;
        lightData[2].y += ambient.color.y;
        lightData[2].z += ambient.color.z;
    }

    std::array<PointCandidate, StylizedRendererConfig::MAX_POINT_LIGHTS> candidates{};
    for (const auto& point : _cachedPointLights)
    {
        if ((point.mask & lightMask) == 0U)
            continue;

        const Vec3 toLight{point.positionAndInverseRange.x - objectPosition.x,
                           point.positionAndInverseRange.y - objectPosition.y,
                           point.positionAndInverseRange.z - objectPosition.z};
        const float inverseRange              = point.positionAndInverseRange.w;
        const float normalizedDistanceSquared = toLight.lengthSquared() * inverseRange * inverseRange;
        if (normalizedDistanceSquared >= 1.0F)
            continue;

        const float attenuation = 1.0F - normalizedDistanceSquared;
        PointCandidate candidate;
        candidate.score = (point.color.x * LUMINANCE_WEIGHTS.x + point.color.y * LUMINANCE_WEIGHTS.y +
                           point.color.z * LUMINANCE_WEIGHTS.z) *
                          attenuation * attenuation;
        candidate.positionAndInverseRange = point.positionAndInverseRange;
        candidate.color                   = point.color;
        for (auto& selected : candidates)
        {
            if (candidate.score > selected.score)
                std::swap(candidate, selected);
        }
    }

    const uint8_t pointCount =
        std::min<uint8_t>({maximumPointLights, getResolvedQuality().maximumPointLights, getConfig().maximumPointLights,
                           StylizedRendererConfig::MAX_POINT_LIGHTS});
    for (uint8_t index = 0; index < pointCount; ++index)
    {
        lightData[5 + index * 2] = candidates[index].positionAndInverseRange;
        lightData[6 + index * 2] = candidates[index].color;
    }
    return true;
}

void StylizedRenderer::beginShadowQueues(Renderer& renderer)
{
    auto* rootGroup = renderer.getNextGroupCommand();
    rootGroup->init(SHADOW_GROUP_ORDER);
    renderer.addCommand(rootGroup);
    _shadowRootQueueId = rootGroup->getRenderQueueID();

    renderer.pushGroup(_shadowRootQueueId);
    auto* beginCommand = renderer.nextCallbackCommand();
    beginCommand->init(SHADOW_BEGIN_ORDER);
    beginCommand->func = [rendererPtr = &renderer, target = RefPtr<RenderTexture>(_shadowTarget),
                          self = RefPtr<StylizedRenderer>(this), this]() {
        _savedRenderTarget = rendererPtr->getRenderTarget();
        _savedViewport     = rendererPtr->getViewport();
        rendererPtr->setRenderTarget(target->getRenderTarget());
        rendererPtr->setViewport(0, 0, static_cast<unsigned int>(target->getRenderSize().width),
                                 static_cast<unsigned int>(target->getRenderSize().height));
    };
    renderer.addCommand(beginCommand);
    renderer.clear(ClearFlag::DEPTH, Color::black, 1.0F, 0, SHADOW_CLEAR_ORDER);

    const auto& quality = getResolvedQuality();
    for (uint8_t cascade = 0; cascade < _activeCascadeCount; ++cascade)
    {
        auto* cascadeGroup = renderer.getNextGroupCommand();
        cascadeGroup->init(0.0F);
        renderer.addCommand(cascadeGroup);
        _cascadeQueueIds[cascade] = cascadeGroup->getRenderQueueID();

        renderer.pushGroup(_cascadeQueueIds[cascade]);
        auto* viewportCommand = renderer.nextCallbackCommand();
        viewportCommand->init(SHADOW_BEGIN_ORDER);
        viewportCommand->func = [rendererPtr = &renderer, cascade, resolution = quality.shadowResolution]() {
            rendererPtr->setViewport(static_cast<int>(cascade) * resolution, 0, resolution, resolution);
        };
        renderer.addCommand(viewportCommand);
        renderer.popGroup();
    }
    renderer.popGroup();
}

void StylizedRenderer::beginSceneVisit(Renderer& renderer, const Camera& camera)
{
    const bool nativeUiCamera =
        getConfig().reserveDefaultCameraForNativeUi && camera.getCameraFlag() == CameraFlag::DEFAULT;
    if (nativeUiCamera || !isEnabled() || _frameActive || dynamic_cast<Scene*>(getOwner()) == nullptr)
        return;

    _frameRenderer = &renderer;
    _qualityController->consumeQualityChange();
    cacheLighting(camera);

    // Keep the shadow group in the parent queue. The world surface group is
    // submitted afterwards at zero order and captures every main globalZ
    // subqueue during the single Scene traversal.
    if (_frameMainLight && ensureShadowTarget() && updateShadowCascades(camera, *_frameMainLight))
        beginShadowQueues(renderer);

    if (!_renderSurface->submit(renderer, camera, getResolvedQuality().renderScale))
    {
        AXLOGE("StylizedRenderer: failed to open the isolated world command queue");
        resetFrameState();
        return;
    }
    _frameActive = true;
}

void StylizedRenderer::endSceneVisit(Renderer& renderer)
{
    if (!_frameActive || _frameRenderer != &renderer)
    {
        if (_renderSurface && _renderSurface->isFrameOpen())
            _renderSurface->finish(renderer);
        resetFrameState();
        return;
    }

    if (_shadowRootQueueId >= 0)
    {
        auto* endCommand = renderer.nextCallbackCommand();
        endCommand->init(SHADOW_END_ORDER);
        endCommand->func = [rendererPtr = &renderer, self = RefPtr<StylizedRenderer>(this), this]() {
            rendererPtr->setRenderTarget(_savedRenderTarget);
            rendererPtr->setViewport(_savedViewport.x, _savedViewport.y, _savedViewport.width, _savedViewport.height);
            _savedRenderTarget = nullptr;
        };
        renderer.addCommand(endCommand, _shadowRootQueueId);
    }
    _renderSurface->finish(renderer);
    resetFrameState();
}

void StylizedRenderer::submitShadowCaster(MeshRenderer& meshRenderer,
                                          Renderer& renderer,
                                          uint8_t cascadeMask,
                                          const Mat4& transform,
                                          const Vec4& color)
{
    if (!_frameActive || _frameRenderer != &renderer || !meshRenderer.isCastingShadow() || cascadeMask == 0)
        return;

    auto visibleCascadeQueues = _cascadeQueueIds;
    for (uint8_t cascade = 0; cascade < _activeCascadeCount; ++cascade)
    {
        if ((cascadeMask & (1U << cascade)) == 0U)
            visibleCascadeQueues[cascade] = -1;
    }

    // Render traversal submits every MeshRenderer node independently. Use
    // only the meshes owned by this node; the public asset-level accessors
    // flatten glTF import children for callers and would duplicate shadow
    // draws with the wrapper transform here.
    for (auto* mesh : meshRenderer._meshes)
    {
        auto* material = mesh->getStylizedMaterial();
        // The mobile shadow budget intentionally has opaque and alpha-cutout
        // casters only. Alpha-blended surfaces stay in the existing forward
        // transparent queue and do not cast a misleading solid silhouette.
        if (!material || material->isTransparent())
            continue;
        mesh->drawStylizedShadow(renderer, *material, visibleCascadeQueues, _lightViewProjection, _activeCascadeCount,
                                 transform, color, _resourceGeneration);
    }
}

void StylizedRenderer::configureMaterial(StylizedMaterial& material, bool receiveShadow)
{
    const auto& quality = getResolvedQuality();
    material.setMaximumPointLights(std::min<uint8_t>(quality.maximumPointLights, getConfig().maximumPointLights));
    material.setDebugView(getConfig().debugView);

    const bool enabled = receiveShadow && _frameActive && _shadowTarget && _cachedMainDirection.w > 0.5F;
    if (!_shadowTarget)
    {
        material.setMainShadowEnabled(false);
        return;
    }

    const Vec2 atlasTexelSize{1.0F / static_cast<float>(_shadowTarget->getRenderSize().width),
                              1.0F / static_cast<float>(_shadowTarget->getRenderSize().height)};
    const uint8_t cascadeCount = std::max<uint8_t>(_activeCascadeCount, 1);
    material.setMainShadowCascades(_worldToShadowTexture, _shadowTarget->getDepthTexture()->getRHITexture(),
                                   atlasTexelSize, getConfig().shadowBias, cascadeCount, _cascadeSplitDistance,
                                   quality.pcfTapCount, getConfig().shadowNormalBias, enabled);
}

bool StylizedRenderer::isShadowVisible(const AABB& worldBounds) const noexcept
{
    return getShadowCascadeMask(worldBounds) != 0;
}

uint8_t StylizedRenderer::getShadowCascadeMask(const AABB& worldBounds) const noexcept
{
    if (!_frameActive || worldBounds.isEmpty())
        return 0;

    std::array<Vec3, 8> worldCorners{};
    worldBounds.getCorners(worldCorners.data());
    const bool zeroToOneDepth = !rhi::GraphicsCore::isOpenGL();
    uint8_t cascadeMask       = 0;
    for (uint8_t cascade = 0; cascade < _activeCascadeCount; ++cascade)
    {
        std::array<Vec4, 8> clipCorners{};
        for (size_t corner = 0; corner < worldCorners.size(); ++corner)
            clipCorners[corner] = _lightViewProjection[cascade] *
                                  Vec4{worldCorners[corner].x, worldCorners[corner].y, worldCorners[corner].z, 1.0F};

        bool outside = false;
        for (int plane = 0; plane < 6; ++plane)
        {
            if (isAabbOutsideClipPlane(clipCorners, plane, zeroToOneDepth))
            {
                outside = true;
                break;
            }
        }
        if (!outside)
            cascadeMask |= static_cast<uint8_t>(1U << cascade);
    }
    return cascadeMask;
}

void StylizedRenderer::resetFrameState() noexcept
{
    _frameRenderer      = nullptr;
    _frameMainLight     = nullptr;
    _shadowRootQueueId  = -1;
    _cascadeQueueIds    = {-1, -1};
    _frameActive        = false;
    _lightingCacheValid = false;
    _activeCascadeCount = 0;
}

}  // namespace ax
