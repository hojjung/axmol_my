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

#include "axmol/3d/StylizedMaterial.h"

#include <algorithm>
#include <array>
#include <cmath>

#include "axmol/3d/StylizedRenderer.h"
#include "axmol/base/CustomEventListener.h"
#include "axmol/base/Director.h"
#include "axmol/base/EventDispatcher.h"
#include "axmol/base/EventType.h"
#include "axmol/base/Logging.h"
#include "axmol/renderer/Pass.h"
#include "axmol/renderer/ProgramManager.h"
#include "axmol/renderer/Technique.h"
#include "axmol/renderer/Texture2D.h"
#include "axmol/renderer/TextureCache.h"
#include "axmol/rhi/ProgramState.h"

namespace ax
{
namespace
{
constexpr float kMinimumRimRange = 1.0e-4F;

Vec4 toVec4(const Color& color)
{
    return {color.r, color.g, color.b, color.a};
}
}  // namespace

bool StylizedMaterialDesc::isValid() const noexcept
{
    const auto finite = [](float value) { return std::isfinite(value); };
    return finite(bandThreshold) && finite(bandSoftness) && finite(rimStart) && finite(rimEnd) &&
           finite(rimIntensity) && finite(alphaCutoff) && finite(uvOffset.x) && finite(uvOffset.y) &&
           finite(uvScale.x) && finite(uvScale.y) && finite(uvRotation) && bandThreshold >= 0.0F &&
           bandThreshold <= 1.0F && bandSoftness >= 0.0F && bandSoftness <= 0.5F && rimStart >= 0.0F &&
           rimStart < rimEnd && rimEnd <= 1.0F && rimEnd - rimStart >= kMinimumRimRange && rimIntensity >= 0.0F &&
           alphaCutoff >= 0.0F && alphaCutoff <= 1.0F;
}

StylizedMaterial* StylizedMaterial::create(const StylizedMaterialDesc& desc, bool skinned)
{
    auto* material = new StylizedMaterial();
    if (material->init(desc, skinned))
    {
        material->autorelease();
        return material;
    }

    delete material;
    return nullptr;
}

StylizedMaterial::~StylizedMaterial()
{
    if (_rendererRecreatedListener)
        Director::getInstance()->getEventDispatcher()->removeEventListener(_rendererRecreatedListener);
    AX_SAFE_RELEASE(_desc.baseTexture);
}

bool StylizedMaterial::init(const StylizedMaterialDesc& desc, bool skinned)
{
    if (!desc.isValid())
    {
        AXLOGE("StylizedMaterial: invalid material description");
        return false;
    }

    uint32_t programType = rhi::ProgramType::STYLIZED_3D;
    if (skinned)
    {
        programType =
            desc.isAlphaCutout() ? rhi::ProgramType::STYLIZED_SKIN_CUTOUT_3D : rhi::ProgramType::STYLIZED_SKIN_3D;
    }
    else if (desc.isAlphaCutout())
    {
        programType = rhi::ProgramType::STYLIZED_CUTOUT_3D;
    }

    auto* program = axpm->getBuiltinProgram(programType);
    if (!program)
    {
        AXLOGE("StylizedMaterial: failed to load program type {}", programType);
        return false;
    }

    auto* programState     = new rhi::ProgramState(program);
    const bool initialized = initWithProgramState(programState);
    programState->release();
    if (!initialized)
        return false;

    _type    = MaterialType::STYLIZED;
    _skinned = skinned;

    AX_SAFE_RETAIN(desc.baseTexture);
    _desc = desc;
    applyCullState();
    applyMaterialUniforms();
    applyShadowUniforms();

    auto* texture = desc.baseTexture ? desc.baseTexture : Director::getInstance()->getTextureCache()->getWhiteTexture();
    setTexture(texture, NTextureData::Usage::Diffuse);
    installContextRestoreListener();

    return true;
}

Material* StylizedMaterial::clone() const
{
    auto* material                  = new StylizedMaterial();
    material->_renderState          = _renderState;
    material->_name                 = _name;
    material->_textureSlots         = _textureSlots;
    material->_textureSlotIndex     = _textureSlotIndex;
    material->_isTransparent        = _isTransparent;
    material->_force2DQueue         = _force2DQueue;
    material->_drawPrimitive        = _drawPrimitive;
    material->_type                 = _type;
    material->_desc                 = _desc;
    material->_skinned              = _skinned;
    material->_maximumPointLights   = _maximumPointLights;
    material->_debugView            = _debugView;
    material->_shadowMap            = _shadowMap;
    material->_shadowBias           = _shadowBias;
    material->_shadowNormalBias     = _shadowNormalBias;
    material->_shadowEnabled        = _shadowEnabled;
    material->_shadowTexelSize      = _shadowTexelSize;
    material->_cascadeSplitDistance = _cascadeSplitDistance;
    material->_cascadeCount         = _cascadeCount;
    material->_pcfTapCount          = _pcfTapCount;
    material->_worldToShadowTexture = _worldToShadowTexture;

    AX_SAFE_RETAIN(material->_desc.baseTexture);

    for (const auto& technique : _techniques)
    {
        auto* clonedTechnique = technique->clone();
        clonedTechnique->setMaterial(material);
        for (ssize_t index = 0; index < clonedTechnique->getPassCount(); ++index)
            clonedTechnique->getPassByIndex(index)->setTechnique(clonedTechnique);
        material->_techniques.pushBack(clonedTechnique);
    }

    const auto techniqueName    = _currentTechnique->getName();
    material->_currentTechnique = material->getTechniqueByName(techniqueName);
    material->installContextRestoreListener();
    material->autorelease();
    return material;
}

StylizedMaterial* StylizedMaterial::cloneForSkinning(bool skinned) const
{
    if (_skinned == skinned)
        return static_cast<StylizedMaterial*>(clone());

    auto* material = StylizedMaterial::create(_desc, skinned);
    if (!material)
        return nullptr;

    material->_renderState          = _renderState;
    material->_name                 = _name;
    material->_isTransparent        = _isTransparent;
    material->_force2DQueue         = _force2DQueue;
    material->_drawPrimitive        = _drawPrimitive;
    material->_maximumPointLights   = _maximumPointLights;
    material->_debugView            = _debugView;
    material->_shadowMap            = _shadowMap;
    material->_shadowBias           = _shadowBias;
    material->_shadowNormalBias     = _shadowNormalBias;
    material->_shadowEnabled        = _shadowEnabled;
    material->_shadowTexelSize      = _shadowTexelSize;
    material->_cascadeSplitDistance = _cascadeSplitDistance;
    material->_cascadeCount         = _cascadeCount;
    material->_pcfTapCount          = _pcfTapCount;
    material->_worldToShadowTexture = _worldToShadowTexture;
    material->applyMaterialUniforms();
    material->applyShadowUniforms();
    return material;
}

bool StylizedMaterial::setDescription(const StylizedMaterialDesc& desc)
{
    if (!desc.isValid())
    {
        AXLOGE("StylizedMaterial: invalid material description");
        return false;
    }

    if (desc.isAlphaCutout() != _desc.isAlphaCutout())
    {
        AXLOGE("StylizedMaterial: opaque/cutout mode is immutable; create a new material");
        return false;
    }

    if (desc.baseTexture != _desc.baseTexture)
    {
        AX_SAFE_RETAIN(desc.baseTexture);
        AX_SAFE_RELEASE(_desc.baseTexture);
    }

    _desc = desc;
    applyCullState();
    applyMaterialUniforms();
    auto* texture = desc.baseTexture ? desc.baseTexture : Director::getInstance()->getTextureCache()->getWhiteTexture();
    setTexture(texture, NTextureData::Usage::Diffuse);
    return true;
}

void StylizedMaterial::applyCullState()
{
    auto& state = getStateBlock();
    state.setCullFace(!_desc.doubleSided);
    state.setCullFaceSide(CullFaceSide::BACK);
}

bool StylizedMaterial::setMainShadow(const Mat4& worldToShadowTexture,
                                     rhi::Texture* shadowMap,
                                     const Vec2& texelSize,
                                     float depthBias,
                                     bool enabled)
{
    return setMainShadowCascades({worldToShadowTexture, worldToShadowTexture}, shadowMap, texelSize, depthBias, 1, 0.0F,
                                 4, 0.0F, enabled);
}

bool StylizedMaterial::setMainShadowCascades(const std::array<Mat4, 2>& worldToShadowTexture,
                                             rhi::Texture* shadowMap,
                                             const Vec2& texelSize,
                                             float depthBias,
                                             uint8_t cascadeCount,
                                             float cascadeSplitDistance,
                                             uint8_t pcfTapCount,
                                             float normalBias,
                                             bool enabled)
{
    if (depthBias < 0.0F || !std::isfinite(depthBias) || texelSize.x <= 0.0F || texelSize.y <= 0.0F ||
        !std::isfinite(texelSize.x) || !std::isfinite(texelSize.y) || cascadeCount == 0 || cascadeCount > 2 ||
        !std::isfinite(cascadeSplitDistance) || cascadeSplitDistance < 0.0F || (pcfTapCount != 4 && pcfTapCount != 9) ||
        !std::isfinite(normalBias) || normalBias < 0.0F || (enabled && !shadowMap))
    {
        AXLOGE("StylizedMaterial: invalid main-shadow state");
        return false;
    }

    _worldToShadowTexture = worldToShadowTexture;
    _shadowMap            = shadowMap;
    _shadowTexelSize      = texelSize;
    _shadowBias           = depthBias;
    _cascadeCount         = cascadeCount;
    _cascadeSplitDistance = cascadeSplitDistance;
    _pcfTapCount          = pcfTapCount;
    _shadowNormalBias     = normalBias;
    _shadowEnabled        = enabled;
    applyShadowUniforms();
    return true;
}

void StylizedMaterial::setMainShadowEnabled(bool enabled)
{
    if (enabled && !_shadowMap)
    {
        AXLOGE("StylizedMaterial: cannot enable shadows without a shadow map");
        return;
    }

    _shadowEnabled = enabled;
    applyShadowUniforms();
}

bool StylizedMaterial::setMaximumPointLights(uint8_t count)
{
    if (count > 2)
    {
        AXLOGE("StylizedMaterial: at most two point lights are supported");
        return false;
    }
    _maximumPointLights = count;
    return true;
}

uint32_t StylizedMaterial::getShadowProgramType() const noexcept
{
    if (_skinned)
    {
        return _desc.isAlphaCutout() ? rhi::ProgramType::STYLIZED_SHADOW_SKIN_CUTOUT_3D
                                     : rhi::ProgramType::STYLIZED_SHADOW_SKIN_3D;
    }
    return _desc.isAlphaCutout() ? rhi::ProgramType::STYLIZED_SHADOW_CUTOUT_3D : rhi::ProgramType::STYLIZED_SHADOW_3D;
}

float StylizedMaterial::evaluateRimMask(float fresnel,
                                        float lightFacing,
                                        float shadowVisibility,
                                        float mainLightLuminance,
                                        float rimStart,
                                        float rimEnd,
                                        float intensity) noexcept
{
    const float range = std::max(rimEnd - rimStart, kMinimumRimRange);
    const float t     = std::clamp((fresnel - rimStart) / range, 0.0F, 1.0F);
    const float rim   = t * t * (3.0F - 2.0F * t);
    return rim * std::max(lightFacing, 0.0F) * std::clamp(shadowVisibility, 0.0F, 1.0F) *
           std::max(mainLightLuminance, 0.0F) * std::max(intensity, 0.0F);
}

void StylizedMaterial::setDebugView(StylizedDebugView debugView)
{
    if (_debugView == debugView)
        return;
    _debugView = debugView;
    applyMaterialUniforms();
}

void StylizedMaterial::applyMaterialUniforms()
{
    const std::array<Vec4, 7> values = {
        toVec4(_desc.baseColor),
        toVec4(_desc.highlightColor),
        toVec4(_desc.shadowColor),
        toVec4(_desc.diffuseTint),
        toVec4(_desc.rimColor),
        Vec4{_desc.bandThreshold, _desc.bandSoftness, _desc.rimStart, _desc.rimEnd},
        Vec4{_desc.rimIntensity, _desc.alphaCutoff, static_cast<float>(static_cast<uint8_t>(_debugView)), 0.0F},
    };
    const float cosine                    = std::cos(_desc.uvRotation);
    const float sine                      = std::sin(_desc.uvRotation);
    const std::array<Vec4, 2> uvTransform = {
        Vec4{_desc.uvOffset.x, _desc.uvOffset.y, _desc.uvScale.x, _desc.uvScale.y},
        Vec4{cosine, sine, 0.0F, 0.0F},
    };

    for (auto* pass : getTechnique()->getPasses())
    {
        pass->setUniformStylizedMaterial(values.data(), sizeof(values));
        pass->setUniformStylizedUvTransform(uvTransform.data(), sizeof(uvTransform));
    }
}

void StylizedMaterial::applyShadowUniforms()
{
    for (auto* pass : getTechnique()->getPasses())
    {
        pass->setUniformMainShadowMatrix(_worldToShadowTexture.data(), sizeof(_worldToShadowTexture));
        pass->setUniformShadowTexelSize(&_shadowTexelSize, sizeof(_shadowTexelSize));
        pass->setUniformShadowBias(&_shadowBias, sizeof(_shadowBias));
        const Vec4 shadowParams{static_cast<float>(_cascadeCount), _cascadeSplitDistance,
                                static_cast<float>(_pcfTapCount), _shadowNormalBias};
        pass->setUniformShadowParams(&shadowParams, sizeof(shadowParams));
        const float enabled = _shadowEnabled ? 1.0F : 0.0F;
        pass->setUniformShadowEnabled(&enabled, sizeof(enabled));
        // nullptr is a real state transition: it releases the previous atlas
        // binding so a disabled shadow cannot retain a stale context resource.
        pass->setUniformMainShadowMap(_shadowMap);
    }
}

void StylizedMaterial::installContextRestoreListener()
{
#if AX_ENABLE_CONTEXT_LOSS_RECOVERY
    if (_rendererRecreatedListener)
        return;

    _rendererRecreatedListener = CustomEventListener::create(EVENT_RENDERER_RECREATED, [this](CustomEvent*) {
        // RenderTexture recreates its RHI texture after a context loss. Drop
        // the old shadow binding while invalid native handles are being
        // abandoned, then the renderer installs the new atlas next frame.
        _shadowMap     = nullptr;
        _shadowEnabled = false;
        applyShadowUniforms();
    });
    Director::getInstance()->getEventDispatcher()->addEventListenerWithFixedPriority(_rendererRecreatedListener, -3);
#endif
}

}  // namespace ax
