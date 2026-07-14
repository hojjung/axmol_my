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

#include <cstdint>

#include "axmol/3d/StylizedRenderer.h"

namespace ax
{

/** Fully resolved frame settings. A shadow resolution is per cascade. */
struct AX_DLL StylizedResolvedQuality
{
    StylizedQualityPreset preset = StylizedQualityPreset::Low;
    uint16_t shadowResolution    = 512;
    uint8_t cascadeCount         = 1;
    uint8_t pcfTapCount          = 4;
    uint8_t maximumPointLights   = 1;
    float renderScale            = 0.85F;
    float shadowDistanceScale    = 1.0F;

    [[nodiscard]] bool operator==(const StylizedResolvedQuality& rhs) const noexcept;
    [[nodiscard]] bool operator!=(const StylizedResolvedQuality& rhs) const noexcept { return !(*this == rhs); }
};

/**
 * Hysteretic 30 fps policy. Auto30 starts at Low, promotes only from valid GPU
 * samples, and degrades in this order: render scale, PCF, shadow budget, points.
 */
class AX_DLL StylizedQualityController final
{
public:
    static constexpr float TARGET_FRAME_TIME_MS = 1000.0F / 30.0F;

    explicit StylizedQualityController(const StylizedRendererConfig& config = {});

    bool setConfig(const StylizedRendererConfig& config);
    void reset();

    /** Submit one completed GPU timer result. Invalid/disjoint samples are ignored. */
    void recordGpuFrameTime(float milliseconds, bool disjoint = false);

    /** CPU frame time is a conservative degradation fallback and never promotes. */
    void recordCpuFrameTime(float milliseconds);

    [[nodiscard]] const StylizedRendererConfig& getConfig() const noexcept { return _config; }
    [[nodiscard]] const StylizedResolvedQuality& getResolvedQuality() const noexcept { return _resolved; }

    /** Returns true once after a target/shader budget change. */
    bool consumeQualityChange() noexcept;

private:
    enum class SampleSource : uint8_t
    {
        CPU,
        GPU,
    };

    void applyPreset(StylizedQualityPreset preset);
    void recordFrameTime(float milliseconds, SampleSource source);
    void promote();
    void degrade();
    void markChanged(const StylizedResolvedQuality& previous);

    StylizedRendererConfig _config{};
    StylizedResolvedQuality _resolved{};
    float _smoothedGpuMs          = TARGET_FRAME_TIME_MS;
    float _smoothedCpuMs          = TARGET_FRAME_TIME_MS;
    uint16_t _gpuHeadroomSamples  = 0;
    uint8_t _gpuOverBudgetSamples = 0;
    uint8_t _cpuOverBudgetSamples = 0;
    bool _qualityChanged          = true;
};

}  // namespace ax
