#include <doctest.h>

#include <algorithm>
#include <array>
#include <cstdint>

#include "axmol/base/Director.h"
#include "axmol/math/Vec3.h"
#include "axmol/renderer/CustomCommand.h"
#include "axmol/renderer/ProgramManager.h"
#include "axmol/renderer/RenderTexture.h"
#include "axmol/renderer/RenderTexturePass.h"
#include "axmol/renderer/Renderer.h"
#include "axmol/rhi/GraphicsCore.h"
#include "axmol/rhi/ProgramState.h"
#include "axmol/rhi/RenderContext.h"
#include "axmol/rhi/SamplerCache.h"

using namespace ax;

namespace
{
rhi::SamplerDesc makeShadowSamplerDesc()
{
    rhi::SamplerDesc comparisonSampler{};
    comparisonSampler.minFilter    = rhi::SamplerFilter::MIN_NEAREST;
    comparisonSampler.magFilter    = rhi::SamplerFilter::MAG_NEAREST;
    comparisonSampler.mipFilter    = rhi::SamplerFilter::MIP_DEFAULT;
    comparisonSampler.sAddressMode = rhi::SamplerAddressMode::CLAMP;
    comparisonSampler.tAddressMode = rhi::SamplerAddressMode::CLAMP;
    comparisonSampler.wAddressMode = rhi::SamplerAddressMode::CLAMP;
    comparisonSampler.compareFunc  = rhi::CompareFunc::LESS_EQUAL;
    return comparisonSampler;
}
}  // namespace

TEST_CASE("D24S8 depth-only target and comparison sampler smoke")
{
    const auto comparisonSampler = makeShadowSamplerDesc();

    CHECK(comparisonSampler.minFilter == rhi::SamplerFilter::MIN_NEAREST);
    CHECK(comparisonSampler.magFilter == rhi::SamplerFilter::MAG_NEAREST);
    CHECK(comparisonSampler.mipFilter == rhi::SamplerFilter::MIP_DEFAULT);
    CHECK(comparisonSampler.compareFunc == rhi::CompareFunc::LESS_EQUAL);

    auto* driver = rhi::GraphicsCore::currentDriver();
    if (!driver)
    {
        WARN("Depth-only render target requires an active RHI driver; skipping this environment.");
        return;
    }

    REQUIRE(driver->checkForFeatureSupported(rhi::FeatureType::DEPTH_COMPARISON_SAMPLING));

    auto* target = RenderTexture::createDepthOnly(64, 64);
    REQUIRE(target != nullptr);
    target->retain();

    CHECK_FALSE(target->hasColorAttachment());
    REQUIRE(target->getDepthTexture() != nullptr);
    REQUIRE(target->getRenderTarget() != nullptr);
    CHECK(target->getDepthTexture()->getPixelFormat() == rhi::PixelFormat::D24S8);
    CHECK(target->getDepthTexture()->getRHITexture()->getTextureUsage() == rhi::TextureUsage::RENDER_TARGET);
    CHECK(target->getRenderTarget()->getActiveColorAttachmentCount() == 0);
    CHECK(target->getRenderTarget()->getWidth() == 64);
    CHECK(target->getRenderTarget()->getHeight() == 64);

    CHECK(static_cast<uint64_t>(rhi::SamplerCache::getInstance()->getSampler(comparisonSampler)) != 0);

    auto* renderer = Director::getInstance()->getRenderer();
    REQUIRE(renderer != nullptr);
    auto* context = renderer->getContext();
    REQUIRE(context != nullptr);
    REQUIRE(context->beginFrame());

    auto* pass = RenderTexturePass::obtain(target);
    pass->setCameraOverrideEnabled(false);
    pass->begin();
    pass->clearDepth(0.25F);
    pass->end();

    renderer->clear(ClearFlag::COLOR | ClearFlag::DEPTH_AND_STENCIL, Color(0, 0, 0, 1), 1.0F, 0, 0.0F);
    renderer->render();
    context->endFrame();
    driver->waitForGPU();

    pass->release();
    target->release();
}

TEST_CASE("D24S8 comparison sampling produces expected pixels")
{
    auto* driver = rhi::GraphicsCore::currentDriver();
    if (!driver)
    {
        WARN("D24S8 comparison sampling requires AX_UNIT_TEST_RHI=1; skipping this environment.");
        return;
    }

    REQUIRE(driver->checkForFeatureSupported(rhi::FeatureType::DEPTH_COMPARISON_SAMPLING));

    auto* depthTarget = RenderTexture::createDepthOnly(64, 64);
    REQUIRE(depthTarget != nullptr);
    depthTarget->retain();

    auto* resultTarget = RenderTexture::create(64, 64, rhi::PixelFormat::RGBA8);
    REQUIRE(resultTarget != nullptr);
    resultTarget->retain();

    auto* depthProgram = ProgramManager::getInstance()->loadProgram(
        "custom/rhiDepthWrite_vs", "custom/rhiDepthWrite_fs", rhi::VertexLayoutKind::Pos);
    REQUIRE(depthProgram != nullptr);

    auto* depthProgramState = new rhi::ProgramState(depthProgram);

    CustomCommand depthCommand;
    depthCommand.setOwnPSVL(depthProgramState, depthProgramState->getVertexLayout(), RenderCommand::ADOPT_FLAG_PS);
    depthCommand.setDrawType(CustomCommand::DrawType::ARRAY);
    depthCommand.setPrimitiveType(rhi::PrimitiveType::TRIANGLE);
    depthCommand.createVertexBuffer(sizeof(Vec3), 6, rhi::BufferUsage::STATIC);
    const Vec3 depthVertices[] = {
        {-1.0F, 0.0F, 0.25F}, {1.0F, 0.0F, 0.25F}, {0.0F, 1.0F, 0.25F},
        {0.0F, 1.0F, 0.25F},  {1.0F, 0.0F, 0.25F}, {-1.0F, 0.0F, 0.25F},
    };
    depthCommand.updateVertexBuffer(depthVertices, sizeof(depthVertices));
    depthCommand.init(0.0F);
    depthCommand.set3D(true);
    depthCommand.setTransparent(false);

    auto* compareProgram = ProgramManager::getInstance()->loadProgram(
        "custom/rhiDepthCompare_vs", "custom/rhiDepthCompare_fs", rhi::VertexLayoutKind::Pos);
    REQUIRE(compareProgram != nullptr);

    auto* programState       = new rhi::ProgramState(compareProgram);
    const auto depthLocation = programState->getUniformLocation("u_depth");
    REQUIRE(depthLocation);
    programState->setTexture(depthLocation, 0, depthTarget->getDepthTexture()->getRHITexture());

    CustomCommand command;
    command.setOwnPSVL(programState, programState->getVertexLayout(), RenderCommand::ADOPT_FLAG_PS);
    command.setDrawType(CustomCommand::DrawType::ARRAY);
    command.setPrimitiveType(rhi::PrimitiveType::TRIANGLE_STRIP);
    command.createVertexBuffer(sizeof(Vec3), 4, rhi::BufferUsage::STATIC);
    const Vec3 vertices[] = {
        {-1.0F, -1.0F, 0.0F},
        {1.0F, -1.0F, 0.0F},
        {-1.0F, 1.0F, 0.0F},
        {1.0F, 1.0F, 0.0F},
    };
    command.updateVertexBuffer(vertices, sizeof(vertices));
    command.init(0.0F);

    auto* renderer = Director::getInstance()->getRenderer();
    REQUIRE(renderer != nullptr);
    auto* context = renderer->getContext();
    REQUIRE(context != nullptr);
    REQUIRE(context->beginFrame());

    auto* depthPass = RenderTexturePass::obtain(depthTarget);
    depthPass->setCameraOverrideEnabled(false);
    depthPass->begin();
    depthPass->clearDepth(1.0F);
    renderer->addCommand(&depthCommand);
    depthPass->end();

    auto* resultPass = RenderTexturePass::obtain(resultTarget);
    resultPass->setCameraOverrideEnabled(false);
    resultPass->begin();
    resultPass->clearColor(Color(1, 0, 1, 1));
    renderer->addCommand(&command);
    resultPass->end();

    renderer->render();

    bool receivedPixels = false;
    std::array<uint8_t, 8> sampledPixels{};
    renderer->readPixels(resultTarget->getRenderTarget(), [&](const rhi::PixelBufferDesc& pixels) {
        REQUIRE(pixels);
        REQUIRE(pixels._width == 64);
        REQUIRE(pixels._height == 64);

        constexpr uint32_t y      = 32;
        constexpr uint32_t leftX  = 16;
        constexpr uint32_t rightX = 48;
        const auto* data          = pixels._data.getBytes();
        const size_t leftOffset   = (y * pixels._width + leftX) * 4;
        const size_t rightOffset  = (y * pixels._width + rightX) * 4;
        std::copy_n(data + leftOffset, 4, sampledPixels.begin());
        std::copy_n(data + rightOffset, 4, sampledPixels.begin() + 4);
        receivedPixels = true;
    });

    context->endFrame();
    driver->waitForGPU();

    REQUIRE(receivedPixels);
    CHECK(sampledPixels[0] <= 5);
    CHECK(sampledPixels[1] <= 5);
    CHECK(sampledPixels[2] <= 5);
    CHECK(sampledPixels[4] >= 250);
    CHECK(sampledPixels[5] >= 250);
    CHECK(sampledPixels[6] >= 250);

    resultPass->release();
    depthPass->release();
    resultTarget->release();
    depthTarget->release();
}
