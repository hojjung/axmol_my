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
#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

#include "axmol/math/AABB.h"
#include "axmol/math/Mat4.h"
#include "axmol/base/Types.h"
#include "axmol/platform/PlatformMacros.h"
#include "axmol/scene/Component.h"

namespace ax
{

class Camera;
class CustomEventListener;
class DirectionLight;
class MeshRenderer;
class Renderer;
class RenderTexture;
class Scene;
class StylizedMaterial;
class StylizedQualityController;
class StylizedRenderSurface;
struct StylizedResolvedQuality;

namespace rhi
{
class RenderTarget;
}

/** Runtime quality policy for the lightweight forward stylized renderer. */
enum class StylizedQualityPreset : uint8_t
{
    Auto30,
    Low,
    Balanced,
    Quality,
};

/** Optional single-channel output used by visual and automated validation. */
enum class StylizedDebugView : uint8_t
{
    FinalColor,
    ToonBand,
    ShadowVisibility,
    RimMask,
};

/** Bounds used by the renderer and its automatic quality controller. */
struct AX_DLL StylizedRendererConfig
{
    static constexpr uint8_t MAX_POINT_LIGHTS = 2;

    StylizedQualityPreset qualityPreset = StylizedQualityPreset::Auto30;
    float shadowDistance                = 20.0F;
    float shadowBias                    = 0.0015F;
    float shadowNormalBias              = 0.4F;
    float minimumRenderScale            = 0.65F;
    float maximumRenderScale            = 1.0F;
    uint8_t maximumPointLights          = MAX_POINT_LIGHTS;
    StylizedDebugView debugView         = StylizedDebugView::FinalColor;
    /** Reserve DEFAULT for a second native-resolution UI traversal. */
    bool reserveDefaultCameraForNativeUi = false;

    [[nodiscard]] bool isValid() const noexcept;
};

/**
 * Scene-owned mobile forward renderer extension.
 *
 * The component prepares a directional shadow pass before the normal 3D
 * queue. MeshRenderer submits shadow packets while performing its normal
 * scene traversal, so enabling the component never traverses Scene twice and
 * never swaps a mesh's main material.
 */
class AX_DLL StylizedRenderer final : public Component
{
public:
    static constexpr std::string_view COMPONENT_NAME = "__ax_stylized_renderer";
    static constexpr uint8_t MAX_CASCADES            = 2;

    static StylizedRenderer* attach(Scene& scene, const StylizedRendererConfig& config = {});
    static StylizedRenderer* get(const Scene& scene) noexcept;

    ~StylizedRenderer() override;

    bool setConfig(const StylizedRendererConfig& config);
    [[nodiscard]] const StylizedRendererConfig& getConfig() const noexcept;
    [[nodiscard]] const StylizedResolvedQuality& getResolvedQuality() const noexcept;

    /** Retains the explicit main light. Passing nullptr restores scene fallback selection. */
    void setMainLight(DirectionLight* light);
    [[nodiscard]] DirectionLight* getMainLight() const noexcept { return _mainLight; }
    [[nodiscard]] DirectionLight* resolveMainLight(unsigned int lightMask = ~0U) const noexcept;

    void recordGpuFrameTime(float milliseconds, bool disjoint = false);
    void update(float delta) override;
    void onRemove() override;
    void onExit() override;

    /** Internal frame hooks used by Scene and MeshRenderer. */
    void beginSceneVisit(Renderer& renderer, const Camera& camera);
    void endSceneVisit(Renderer& renderer);
    void submitShadowCaster(MeshRenderer& meshRenderer,
                            Renderer& renderer,
                            uint8_t cascadeMask,
                            const Mat4& transform,
                            const Vec4& color);
    void configureMaterial(StylizedMaterial& material, bool receiveShadow);

    [[nodiscard]] bool isShadowVisible(const AABB& worldBounds) const noexcept;
    [[nodiscard]] uint8_t getShadowCascadeMask(const AABB& worldBounds) const noexcept;
    [[nodiscard]] bool isFrameActive() const noexcept { return _frameActive; }
    [[nodiscard]] uint64_t getResourceGeneration() const noexcept { return _resourceGeneration; }

private:
    friend class Mesh;

    struct CachedLight
    {
        Vec4 color{};
        uint32_t mask = 0;
    };

    struct CachedPointLight
    {
        Vec4 positionAndInverseRange{};
        Vec4 color{};
        uint32_t mask = 0;
    };

    StylizedRenderer();

    bool init(const StylizedRendererConfig& config);
    void invalidateResources();
    bool ensureShadowTarget();
    bool updateShadowCascades(const Camera& camera, DirectionLight& light);
    void cacheLighting(const Camera& camera);
    bool fillStylizedLightData(std::array<Vec4, 9>& lightData,
                               unsigned int lightMask,
                               const Vec3& objectPosition,
                               uint8_t maximumPointLights) const noexcept;
    void beginShadowQueues(Renderer& renderer);
    void resetFrameState() noexcept;

    std::unique_ptr<StylizedQualityController> _qualityController;
    std::unique_ptr<StylizedRenderSurface> _renderSurface;
    DirectionLight* _mainLight                      = nullptr;
    DirectionLight* _frameMainLight                 = nullptr;
    RenderTexture* _shadowTarget                    = nullptr;
    CustomEventListener* _rendererRecreatedListener = nullptr;

    std::array<Mat4, MAX_CASCADES> _lightViewProjection{Mat4::identity, Mat4::identity};
    std::array<Mat4, MAX_CASCADES> _worldToShadowTexture{Mat4::identity, Mat4::identity};
    std::array<int, MAX_CASCADES> _cascadeQueueIds{-1, -1};
    std::vector<CachedLight> _cachedAmbientLights;
    std::vector<CachedPointLight> _cachedPointLights;
    Vec4 _cachedMainDirection{0.0F, -1.0F, 0.0F, 0.0F};
    Vec4 _cachedMainColor{};
    Vec4 _cachedCameraPosition{};
    Vec4 _cachedCameraForward{0.0F, 0.0F, -1.0F, 0.0F};
    uint32_t _cachedMainMask = 0;

    Renderer* _frameRenderer              = nullptr;
    rhi::RenderTarget* _savedRenderTarget = nullptr;
    Viewport _savedViewport{};
    int _shadowRootQueueId         = -1;
    float _cascadeSplitDistance    = 0.0F;
    uint16_t _allocatedResolution  = 0;
    uint8_t _allocatedCascadeCount = 0;
    uint8_t _activeCascadeCount    = 0;
    bool _frameActive              = false;
    bool _lightingCacheValid       = false;
    uint64_t _resourceGeneration   = 1;
};

}  // namespace ax
