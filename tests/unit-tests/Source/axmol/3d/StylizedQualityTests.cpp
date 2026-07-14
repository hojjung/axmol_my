#include <doctest.h>

#include "axmol/3d/StylizedQuality.h"

using namespace ax;

TEST_CASE("Stylized quality presets match the mobile 30 fps budgets")
{
    StylizedRendererConfig config;

    config.qualityPreset = StylizedQualityPreset::Low;
    StylizedQualityController low(config);
    CHECK(low.getResolvedQuality().cascadeCount == 1);
    CHECK(low.getResolvedQuality().shadowResolution == 512);
    CHECK(low.getResolvedQuality().pcfTapCount == 4);
    CHECK(low.getResolvedQuality().maximumPointLights == 1);
    CHECK(low.getResolvedQuality().renderScale == doctest::Approx(0.85F));

    config.qualityPreset = StylizedQualityPreset::Balanced;
    StylizedQualityController balanced(config);
    CHECK(balanced.getResolvedQuality().cascadeCount == 1);
    CHECK(balanced.getResolvedQuality().shadowResolution == 1024);
    CHECK(balanced.getResolvedQuality().pcfTapCount == 4);
    CHECK(balanced.getResolvedQuality().maximumPointLights == 2);

    config.qualityPreset = StylizedQualityPreset::Quality;
    StylizedQualityController quality(config);
    CHECK(quality.getResolvedQuality().cascadeCount == 2);
    CHECK(quality.getResolvedQuality().shadowResolution == 1024);
    CHECK(quality.getResolvedQuality().pcfTapCount == 9);
    CHECK(quality.getResolvedQuality().maximumPointLights == 2);
}

TEST_CASE("Auto30 promotes only from GPU samples and degrades render scale first")
{
    StylizedRendererConfig config;
    config.qualityPreset = StylizedQualityPreset::Auto30;
    StylizedQualityController controller(config);

    CHECK(controller.getResolvedQuality().preset == StylizedQualityPreset::Low);
    for (int sample = 0; sample < 600; ++sample)
        controller.recordCpuFrameTime(10.0F);
    CHECK(controller.getResolvedQuality().preset == StylizedQualityPreset::Low);

    for (int sample = 0; sample < 600; ++sample)
        controller.recordGpuFrameTime(10.0F);
    CHECK(controller.getResolvedQuality().preset != StylizedQualityPreset::Low);

    for (int sample = 0; sample < 1000; ++sample)
    {
        const StylizedResolvedQuality previous = controller.getResolvedQuality();
        controller.recordGpuFrameTime(60.0F);
        const StylizedResolvedQuality& current = controller.getResolvedQuality();
        CHECK(current.renderScale <= previous.renderScale);
        CHECK(current.shadowDistanceScale <= previous.shadowDistanceScale);
        CHECK(current.shadowResolution <= previous.shadowResolution);
        CHECK(current.cascadeCount <= previous.cascadeCount);
        CHECK(current.pcfTapCount <= previous.pcfTapCount);
        CHECK(current.maximumPointLights <= previous.maximumPointLights);
    }

    controller.reset();
    const float initialScale = controller.getResolvedQuality().renderScale;
    for (int sample = 0; sample < 60; ++sample)
        controller.recordGpuFrameTime(60.0F);
    CHECK(controller.getResolvedQuality().renderScale < initialScale);
    CHECK(controller.getResolvedQuality().pcfTapCount == 4);
    CHECK(controller.getResolvedQuality().maximumPointLights == 1);
}

TEST_CASE("Stylized quality rejects invalid renderer limits")
{
    StylizedQualityController controller;
    StylizedRendererConfig invalid;
    invalid.minimumRenderScale = 1.0F;
    invalid.maximumRenderScale = 0.75F;

    CHECK_FALSE(controller.setConfig(invalid));
    CHECK(controller.getConfig().isValid());
}

TEST_CASE("Auto30 normal samples do not reset the other source budget streak")
{
    StylizedRendererConfig config;
    config.qualityPreset = StylizedQualityPreset::Auto30;
    StylizedQualityController controller(config);

    for (int sample = 0; sample < 700 && controller.getResolvedQuality().preset == StylizedQualityPreset::Low; ++sample)
    {
        controller.recordGpuFrameTime(10.0F);
        controller.recordCpuFrameTime(33.0F);
    }
    CHECK(controller.getResolvedQuality().preset == StylizedQualityPreset::Balanced);

    controller.reset();
    const float initialScale = controller.getResolvedQuality().renderScale;
    for (int sample = 0; sample < 80 && controller.getResolvedQuality().renderScale == initialScale; ++sample)
    {
        controller.recordGpuFrameTime(60.0F);
        controller.recordCpuFrameTime(10.0F);
    }
    CHECK(controller.getResolvedQuality().renderScale < initialScale);

    controller.reset();
    const float cpuLimitedScale = controller.getResolvedQuality().renderScale;
    for (int sample = 0; sample < 80 && controller.getResolvedQuality().renderScale == cpuLimitedScale; ++sample)
    {
        controller.recordCpuFrameTime(60.0F);
        controller.recordGpuFrameTime(30.0F);
    }
    CHECK(controller.getResolvedQuality().renderScale < cpuLimitedScale);
}

TEST_CASE("Auto30 CPU over-budget samples cancel pending GPU promotion")
{
    StylizedRendererConfig config;
    config.qualityPreset = StylizedQualityPreset::Auto30;
    StylizedQualityController controller(config);

    // The smoothed GPU time crosses below the promotion threshold on sample 8,
    // so sample 246 leaves exactly 239 consecutive headroom samples pending.
    for (int sample = 0; sample < 246; ++sample)
        controller.recordGpuFrameTime(10.0F);
    CHECK(controller.getResolvedQuality().preset == StylizedQualityPreset::Low);

    controller.recordCpuFrameTime(60.0F);
    controller.recordGpuFrameTime(10.0F);
    CHECK(controller.getResolvedQuality().preset == StylizedQualityPreset::Low);

    for (int sample = 0; sample < 239; ++sample)
        controller.recordGpuFrameTime(10.0F);
    CHECK(controller.getResolvedQuality().preset == StylizedQualityPreset::Balanced);
}
