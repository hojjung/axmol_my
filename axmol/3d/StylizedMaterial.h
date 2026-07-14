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

#include "axmol/3d/MeshMaterial.h"
#include "axmol/math/Math.h"

namespace ax
{

class Texture2D;
class Mesh;
class MeshRenderer;
class StylizedRenderer;
enum class StylizedDebugView : uint8_t;

namespace rhi
{
class Texture;
}

/** Linear-space parameters for the mobile stylized material. */
struct AX_DLL StylizedMaterialDesc
{
    Texture2D* baseTexture = nullptr;
    Color baseColor        = Color::white;
    Color highlightColor   = Color::white;
    Color shadowColor      = Color{0.25F, 0.25F, 0.25F, 1.0F};
    Color diffuseTint      = Color{1.0F, 0.9290111F, 0.75F, 1.0F};
    Color rimColor         = Color{0.7028302F, 0.98113203F, 1.0F, 1.0F};
    float bandThreshold    = 0.7F;
    float bandSoftness     = 0.25F;
    float rimStart         = 0.7F;
    float rimEnd           = 1.0F;
    float rimIntensity     = 1.0F;
    float alphaCutoff      = 0.0F;
    bool doubleSided       = false;
    Vec2 uvOffset{0.0F, 0.0F};
    Vec2 uvScale{1.0F, 1.0F};
    float uvRotation = 0.0F;

    [[nodiscard]] bool isAlphaCutout() const noexcept { return alphaCutoff > 0.0F; }
    [[nodiscard]] bool isValid() const noexcept;
};

/**
 * Mesh material implementing a Half-Lambert two-band response, shadow-aware
 * light-facing rim, and at most two unshadowed point-light contributions.
 */
class AX_DLL StylizedMaterial final : public MeshMaterial
{
    friend class Mesh;
    friend class MeshRenderer;
    friend class StylizedRenderer;

public:
    static StylizedMaterial* create(const StylizedMaterialDesc& desc = {}, bool skinned = false);

    ~StylizedMaterial() override;

    Material* clone() const override;

    [[nodiscard]] const StylizedMaterialDesc& getDescription() const noexcept { return _desc; }

    /** The opaque/cutout shader variant cannot be changed after creation. */
    bool setDescription(const StylizedMaterialDesc& desc);

    /**
     * Installs the main directional-light shadow resources. The matrix must
     * transform world space directly into [0, 1] shadow texture coordinates.
     */
    bool setMainShadow(const Mat4& worldToShadowTexture,
                       rhi::Texture* shadowMap,
                       const Vec2& texelSize,
                       float depthBias,
                       bool enabled);

    bool setMainShadowCascades(const std::array<Mat4, 2>& worldToShadowTexture,
                               rhi::Texture* shadowMap,
                               const Vec2& texelSize,
                               float depthBias,
                               uint8_t cascadeCount,
                               float cascadeSplitDistance,
                               uint8_t pcfTapCount,
                               float normalBias,
                               bool enabled);

    void setMainShadowEnabled(bool enabled);
    bool setMaximumPointLights(uint8_t count);
    [[nodiscard]] bool isMainShadowEnabled() const noexcept { return _shadowEnabled; }
    [[nodiscard]] bool isSkinned() const noexcept { return _skinned; }
    [[nodiscard]] uint8_t getMaximumPointLights() const noexcept { return _maximumPointLights; }
    [[nodiscard]] uint32_t getShadowProgramType() const noexcept;

    /** CPU reference for shadow-aware rim validation and tooling. */
    [[nodiscard]] static float evaluateRimMask(float fresnel,
                                               float lightFacing,
                                               float shadowVisibility,
                                               float mainLightLuminance,
                                               float rimStart,
                                               float rimEnd,
                                               float intensity) noexcept;

private:
    StylizedMaterial() = default;

    bool init(const StylizedMaterialDesc& desc, bool skinned);
    StylizedMaterial* cloneForSkinning(bool skinned) const;
    void applyMaterialUniforms();
    void applyShadowUniforms();
    void applyCullState();
    void setDebugView(StylizedDebugView debugView);

    StylizedMaterialDesc _desc{};
    std::array<Mat4, 2> _worldToShadowTexture{Mat4::identity, Mat4::identity};
    Vec2 _shadowTexelSize{1.0F, 1.0F};
    rhi::Texture* _shadowMap    = nullptr;
    float _shadowBias           = 0.0F;
    float _shadowNormalBias     = 0.0F;
    float _cascadeSplitDistance = 0.0F;
    uint8_t _cascadeCount       = 1;
    uint8_t _pcfTapCount        = 4;
    bool _shadowEnabled         = false;
    bool _skinned               = false;
    uint8_t _maximumPointLights = 2;
    StylizedDebugView _debugView{};
};

}  // namespace ax
