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
#include "axmol/3d/StylizedQuality.h"

#include <algorithm>
#include <cmath>

namespace ax
{
namespace
{
constexpr float SAMPLE_ALPHA             = 0.08F;
constexpr float PROMOTION_THRESHOLD_MS   = 22.0F;
constexpr float DEGRADATION_THRESHOLD_MS = 34.0F;
constexpr uint16_t PROMOTION_SAMPLES     = 240;
constexpr uint8_t DEGRADATION_SAMPLES    = 18;
constexpr float SCALE_STEP               = 0.05F;

float clampScale(float value, float minimum, float maximum)
{
    return std::clamp(value, minimum, maximum);
}

float presetMinimumScale(StylizedQualityPreset preset)
{
    switch (preset)
    {
    case StylizedQualityPreset::Quality:
        return 0.85F;
    case StylizedQualityPreset::Balanced:
        return 0.75F;
    case StylizedQualityPreset::Auto30:
    case StylizedQualityPreset::Low:
    default:
        return 0.65F;
    }
}

float presetMaximumScale(StylizedQualityPreset preset)
{
    switch (preset)
    {
    case StylizedQualityPreset::Quality:
    case StylizedQualityPreset::Balanced:
        return 1.0F;
    case StylizedQualityPreset::Auto30:
    case StylizedQualityPreset::Low:
    default:
        return 0.85F;
    }
}
}  // namespace

bool StylizedResolvedQuality::operator==(const StylizedResolvedQuality& rhs) const noexcept
{
    return preset == rhs.preset && shadowResolution == rhs.shadowResolution && cascadeCount == rhs.cascadeCount &&
           pcfTapCount == rhs.pcfTapCount && maximumPointLights == rhs.maximumPointLights &&
           std::abs(renderScale - rhs.renderScale) <= 0.0001F &&
           std::abs(shadowDistanceScale - rhs.shadowDistanceScale) <= 0.0001F;
}

StylizedQualityController::StylizedQualityController(const StylizedRendererConfig& config)
{
    if (!setConfig(config))
        setConfig(StylizedRendererConfig{});
}

bool StylizedQualityController::setConfig(const StylizedRendererConfig& config)
{
    if (!config.isValid())
        return false;

    _config = config;
    reset();
    return true;
}

void StylizedQualityController::reset()
{
    _smoothedGpuMs        = TARGET_FRAME_TIME_MS;
    _smoothedCpuMs        = TARGET_FRAME_TIME_MS;
    _gpuHeadroomSamples   = 0;
    _gpuOverBudgetSamples = 0;
    _cpuOverBudgetSamples = 0;
    applyPreset(_config.qualityPreset == StylizedQualityPreset::Auto30 ? StylizedQualityPreset::Low
                                                                       : _config.qualityPreset);
    _qualityChanged = true;
}

void StylizedQualityController::recordGpuFrameTime(float milliseconds, bool disjoint)
{
    if (!disjoint)
        recordFrameTime(milliseconds, SampleSource::GPU);
}

void StylizedQualityController::recordCpuFrameTime(float milliseconds)
{
    recordFrameTime(milliseconds, SampleSource::CPU);
}

bool StylizedQualityController::consumeQualityChange() noexcept
{
    const bool changed = _qualityChanged;
    _qualityChanged    = false;
    return changed;
}

void StylizedQualityController::applyPreset(StylizedQualityPreset preset)
{
    const StylizedResolvedQuality previous = _resolved;
    _resolved.preset                       = preset;
    _resolved.shadowDistanceScale          = 1.0F;

    switch (preset)
    {
    case StylizedQualityPreset::Quality:
        _resolved.shadowResolution   = 1024;
        _resolved.cascadeCount       = 2;
        _resolved.pcfTapCount        = 9;
        _resolved.maximumPointLights = std::min<uint8_t>(2, _config.maximumPointLights);
        break;
    case StylizedQualityPreset::Balanced:
        _resolved.shadowResolution   = 1024;
        _resolved.cascadeCount       = 1;
        _resolved.pcfTapCount        = 4;
        _resolved.maximumPointLights = std::min<uint8_t>(2, _config.maximumPointLights);
        break;
    case StylizedQualityPreset::Auto30:
    case StylizedQualityPreset::Low:
    default:
        _resolved.shadowResolution   = 512;
        _resolved.cascadeCount       = 1;
        _resolved.pcfTapCount        = 4;
        _resolved.maximumPointLights = std::min<uint8_t>(1, _config.maximumPointLights);
        break;
    }

    const float minimum   = std::max(_config.minimumRenderScale, presetMinimumScale(preset));
    const float maximum   = std::min(_config.maximumRenderScale, presetMaximumScale(preset));
    _resolved.renderScale = clampScale(maximum, std::min(minimum, maximum), maximum);
    markChanged(previous);
}

void StylizedQualityController::recordFrameTime(float milliseconds, SampleSource source)
{
    if (!std::isfinite(milliseconds) || milliseconds <= 0.0F || milliseconds > 1000.0F)
        return;

    float& smoothed = source == SampleSource::GPU ? _smoothedGpuMs : _smoothedCpuMs;
    smoothed += (milliseconds - smoothed) * SAMPLE_ALPHA;

    if (_config.qualityPreset != StylizedQualityPreset::Auto30)
        return;

    const bool isGpu        = source == SampleSource::GPU;
    auto& overBudgetSamples = isGpu ? _gpuOverBudgetSamples : _cpuOverBudgetSamples;
    const bool overBudget   = smoothed > DEGRADATION_THRESHOLD_MS;
    if (overBudget)
    {
        _gpuHeadroomSamples = 0;
        if (++overBudgetSamples >= DEGRADATION_SAMPLES)
        {
            degrade();
            _gpuOverBudgetSamples = 0;
            _cpuOverBudgetSamples = 0;
        }
        return;
    }

    overBudgetSamples = 0;
    if (isGpu && smoothed < PROMOTION_THRESHOLD_MS)
    {
        if (++_gpuHeadroomSamples >= PROMOTION_SAMPLES)
        {
            promote();
            _gpuHeadroomSamples = 0;
        }
    }
    else if (isGpu)
    {
        _gpuHeadroomSamples = 0;
    }
}

void StylizedQualityController::promote()
{
    if (_resolved.preset == StylizedQualityPreset::Low)
        applyPreset(StylizedQualityPreset::Balanced);
    else if (_resolved.preset == StylizedQualityPreset::Balanced)
        applyPreset(StylizedQualityPreset::Quality);
}

void StylizedQualityController::degrade()
{
    const StylizedResolvedQuality previous = _resolved;
    const float minimum                    = std::max(_config.minimumRenderScale, presetMinimumScale(_resolved.preset));

    if (_resolved.renderScale > minimum + 0.001F)
    {
        _resolved.renderScale = std::max(minimum, _resolved.renderScale - SCALE_STEP);
    }
    else if (_resolved.pcfTapCount > 4)
    {
        _resolved.pcfTapCount = 4;
    }
    else if (_resolved.shadowDistanceScale > 0.51F)
    {
        _resolved.shadowDistanceScale = std::max(0.5F, _resolved.shadowDistanceScale * 0.8F);
        if (_resolved.shadowResolution > 512 && _resolved.shadowDistanceScale <= 0.64F)
        {
            _resolved.shadowResolution = static_cast<uint16_t>(_resolved.shadowResolution / 2);
            _resolved.cascadeCount     = 1;
        }
    }
    else if (_resolved.maximumPointLights > 1)
    {
        _resolved.maximumPointLights = 1;
    }
    else if (_resolved.preset == StylizedQualityPreset::Quality)
    {
        _resolved.preset           = StylizedQualityPreset::Balanced;
        _resolved.cascadeCount     = 1;
        _resolved.pcfTapCount      = 4;
        _resolved.shadowResolution = std::min<uint16_t>(_resolved.shadowResolution, 1024);
        _resolved.maximumPointLights =
            std::min<uint8_t>(_resolved.maximumPointLights, std::min<uint8_t>(2, _config.maximumPointLights));

        const float balancedMinimum =
            std::max(_config.minimumRenderScale, presetMinimumScale(StylizedQualityPreset::Balanced));
        const float balancedMaximum =
            std::min(_config.maximumRenderScale, presetMaximumScale(StylizedQualityPreset::Balanced));
        _resolved.renderScale =
            clampScale(_resolved.renderScale, std::min(balancedMinimum, balancedMaximum), balancedMaximum);
    }
    else if (_resolved.preset == StylizedQualityPreset::Balanced)
    {
        _resolved.preset             = StylizedQualityPreset::Low;
        _resolved.shadowResolution   = std::min<uint16_t>(_resolved.shadowResolution, 512);
        _resolved.cascadeCount       = 1;
        _resolved.pcfTapCount        = 4;
        _resolved.maximumPointLights = std::min<uint8_t>(_resolved.maximumPointLights, 1);

        const float lowMinimum = std::max(_config.minimumRenderScale, presetMinimumScale(StylizedQualityPreset::Low));
        const float lowMaximum = std::min(_config.maximumRenderScale, presetMaximumScale(StylizedQualityPreset::Low));
        _resolved.renderScale  = clampScale(_resolved.renderScale, std::min(lowMinimum, lowMaximum), lowMaximum);
    }

    markChanged(previous);
}

void StylizedQualityController::markChanged(const StylizedResolvedQuality& previous)
{
    _qualityChanged |= previous != _resolved;
}

}  // namespace ax
