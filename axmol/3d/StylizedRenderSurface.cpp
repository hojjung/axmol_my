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

#include "axmol/3d/StylizedRenderSurface.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

#include "axmol/base/Director.h"
#include "axmol/renderer/CallbackCommand.h"
#include "axmol/renderer/GroupCommand.h"
#include "axmol/renderer/ProgramManager.h"
#include "axmol/renderer/RenderTexture.h"
#include "axmol/renderer/Renderer.h"
#include "axmol/renderer/VertexLayoutManager.h"
#include "axmol/rhi/ProgramState.h"
#include "axmol/rhi/VertexLayout.h"
#include "axmol/scene/Camera.h"

namespace ax
{
namespace
{
constexpr float WORLD_BEGIN_ORDER  = std::numeric_limits<float>::lowest();
constexpr float WORLD_FINISH_ORDER = std::numeric_limits<float>::max();

struct CompositeVertex
{
    Vec2 position;
    Vec2 texCoord;
};

constexpr std::array<CompositeVertex, 4> COMPOSITE_VERTICES = {
    CompositeVertex{Vec2{-1.0F, -1.0F}, Vec2{0.0F, 0.0F}},
    CompositeVertex{Vec2{1.0F, -1.0F}, Vec2{1.0F, 0.0F}},
    CompositeVertex{Vec2{-1.0F, 1.0F}, Vec2{0.0F, 1.0F}},
    CompositeVertex{Vec2{1.0F, 1.0F}, Vec2{1.0F, 1.0F}},
};
constexpr std::array<uint16_t, 6> COMPOSITE_INDICES = {0, 1, 2, 2, 1, 3};
}  // namespace

struct StylizedRenderSurface::FrameBinding
{
    rhi::RenderTarget* renderTarget = nullptr;
    Viewport viewport{};
};

StylizedRenderSurface::~StylizedRenderSurface()
{
    invalidate();
}

bool StylizedRenderSurface::submit(Renderer& renderer, const Camera& camera, float renderScale)
{
    if (_activeRenderer || !std::isfinite(renderScale))
        return false;

    // The zero-order outer group isolates every main-world subqueue. The
    // renderer's directional shadow group remains in the parent queue at a
    // negative order, so even extreme per-node globalZ values cannot execute
    // before the shadow pass.
    auto* worldGroup = renderer.getNextGroupCommand();
    worldGroup->init(0.0F);
    renderer.addCommand(worldGroup);
    _worldQueueId   = worldGroup->getRenderQueueID();
    _activeRenderer = &renderer;
    renderer.pushGroup(_worldQueueId);

    // Camera-owned targets already form an explicit offscreen pass. A scale
    // of one still keeps the queue isolation above, but needs no composite.
    if (camera.getTargetTexture() != nullptr || renderScale >= 0.999F)
        return true;

    const auto viewport = renderer.getViewport();
    if (viewport.width == 0 || viewport.height == 0)
        return true;

    renderScale             = std::clamp(renderScale, 0.25F, 1.0F);
    const auto scaledWidth  = std::max(1U, static_cast<unsigned int>(std::lround(viewport.width * renderScale)));
    const auto scaledHeight = std::max(1U, static_cast<unsigned int>(std::lround(viewport.height * renderScale)));
    if (!ensureTarget(scaledWidth, scaledHeight) || !ensureCompositeResources())
        return true;

    updateCompositeTexture();

    _frameBinding      = std::make_shared<FrameBinding>();
    auto frame         = _frameBinding;
    auto* beginCommand = renderer.nextCallbackCommand();
    beginCommand->init(WORLD_BEGIN_ORDER);
    beginCommand->func = [frame, rendererPtr = &renderer, target = RefPtr<RenderTexture>(_target)]() {
        frame->renderTarget = rendererPtr->getRenderTarget();
        frame->viewport     = rendererPtr->getViewport();
        rendererPtr->setRenderTarget(target->getRenderTarget());
        rendererPtr->setViewport(0, 0, static_cast<unsigned int>(target->getRenderSize().width),
                                 static_cast<unsigned int>(target->getRenderSize().height));
    };
    renderer.addCommand(beginCommand);

    renderer.clear(ClearFlag::COLOR | ClearFlag::DEPTH_AND_STENCIL, Director::getInstance()->getClearColor(), 1.0F, 0,
                   WORLD_BEGIN_ORDER);

    _scaledFrame = true;
    return true;
}

bool StylizedRenderSurface::finish(Renderer& renderer)
{
    if (!_activeRenderer)
        return false;

    Renderer* activeRenderer    = _activeRenderer;
    const bool matchingRenderer = activeRenderer == &renderer;
    if (_scaledFrame)
    {
        auto frame       = _frameBinding;
        auto* endCommand = activeRenderer->nextCallbackCommand();
        endCommand->init(WORLD_FINISH_ORDER);
        endCommand->func = [frame, rendererPtr = activeRenderer]() {
            rendererPtr->setRenderTarget(frame->renderTarget);
            rendererPtr->setViewport(frame->viewport.x, frame->viewport.y, frame->viewport.width,
                                     frame->viewport.height);
        };
        activeRenderer->addCommand(endCommand);

        _compositeCommand.init(WORLD_FINISH_ORDER);
        activeRenderer->addCommand(&_compositeCommand);
    }

    activeRenderer->popGroup();
    _frameBinding.reset();
    _activeRenderer = nullptr;
    _worldQueueId   = -1;
    _scaledFrame    = false;
    return matchingRenderer;
}

void StylizedRenderSurface::invalidate()
{
    AXASSERT(!_activeRenderer, "Cannot invalidate a StylizedRenderSurface during scene traversal");
    AX_SAFE_RELEASE_NULL(_target);
    AX_SAFE_RELEASE_NULL(_program);
    AX_SAFE_RELEASE_NULL(_layout);
    _compositeCommand.setVertexBuffer(nullptr);
    _compositeCommand.setIndexBuffer(nullptr, CustomCommand::IndexFormat::U_SHORT);
    _compositeCommand.releasePSVL();
    _width = _height = 0;
}

Vec2 StylizedRenderSurface::getRenderSize() const noexcept
{
    return {static_cast<float>(_width), static_cast<float>(_height)};
}

bool StylizedRenderSurface::ensureTarget(unsigned int width, unsigned int height)
{
    if (_target && _width == width && _height == height)
        return true;

    AX_SAFE_RELEASE_NULL(_target);
    _target = RenderTexture::create(static_cast<int>(width), static_cast<int>(height), rhi::PixelFormat::RGBA8,
                                    rhi::PixelFormat::D24S8);
    if (!_target)
    {
        _width = _height = 0;
        return false;
    }

    _target->retain();
    _target->setAntiAliasTexParameters();
    _width  = width;
    _height = height;
    return true;
}

bool StylizedRenderSurface::ensureCompositeResources()
{
    if (_program && _layout && _compositeCommand.getVertexBuffer() && _compositeCommand.getIndexBuffer())
        return true;

    auto* shader = axpm->loadProgram("stylizedUpscale_vs", "stylizedUpscale_fs");
    if (!shader)
        return false;

    _program = new rhi::ProgramState(shader);

    auto desc = axvlm->allocateVertexLayoutDesc();
    desc.startLayout(2);
    desc.addAttrib("a_position", shader->getVertexInputDesc("a_position"), rhi::VertexElementType::FLOAT2, 0, false);
    desc.addAttrib("a_texCoord", shader->getVertexInputDesc("a_texCoord"), rhi::VertexElementType::FLOAT2, sizeof(Vec2),
                   false);
    desc.endLayout(sizeof(CompositeVertex));
    Object::assign(_layout, axvlm->getVertexLayout(std::move(desc)));
    if (!_layout)
        return false;

    _compositeCommand.setWeakPSVL(_program, _layout);
    _compositeCommand.createVertexBuffer(sizeof(CompositeVertex), COMPOSITE_VERTICES.size(),
                                         CustomCommand::BufferUsage::STATIC);
    _compositeCommand.createIndexBuffer(CustomCommand::IndexFormat::U_SHORT, COMPOSITE_INDICES.size(),
                                        CustomCommand::BufferUsage::STATIC);
    _compositeCommand.updateVertexBuffer(COMPOSITE_VERTICES.data(), sizeof(COMPOSITE_VERTICES));
    _compositeCommand.updateIndexBuffer(COMPOSITE_INDICES.data(), sizeof(COMPOSITE_INDICES));
    _compositeCommand.setDrawType(CustomCommand::DrawType::ELEMENT);
    _compositeCommand.setPrimitiveType(CustomCommand::PrimitiveType::TRIANGLE);
    _compositeCommand.setIndexDrawInfo(0, COMPOSITE_INDICES.size());
    _compositeCommand.blendDesc().blendEnabled = false;
    return true;
}

void StylizedRenderSurface::updateCompositeTexture()
{
    const auto location = _program->getProgram()->getUniformLocation("u_tex0");
    _program->setTexture(location, 0, _target->getRHITexture());
}

}  // namespace ax
