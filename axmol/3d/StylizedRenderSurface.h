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

#include <memory>

#include "axmol/base/Types.h"
#include "axmol/renderer/CustomCommand.h"

namespace ax
{

class Camera;
class RenderTexture;
class Renderer;

namespace rhi
{
class ProgramState;
class VertexLayout;
}  // namespace rhi

/**
 * One-traversal world/UI split used by the stylized mobile renderer.
 *
 * A zero-order outer GroupCommand contains every main-world queue. When dynamic
 * resolution is active, lowest/highest-order callbacks bind and restore the
 * scaled target inside that group. A separate DEFAULT traversal can then draw
 * native-resolution UI.
 */
class StylizedRenderSurface final
{
public:
    StylizedRenderSurface() = default;
    ~StylizedRenderSurface();

    bool submit(Renderer& renderer, const Camera& camera, float renderScale);
    bool finish(Renderer& renderer);
    void invalidate();

    [[nodiscard]] Vec2 getRenderSize() const noexcept;
    [[nodiscard]] bool isFrameOpen() const noexcept { return _activeRenderer != nullptr; }
    [[nodiscard]] bool isScaledFrame() const noexcept { return _scaledFrame; }
    [[nodiscard]] int getWorldQueueId() const noexcept { return _worldQueueId; }

private:
    struct FrameBinding;

    bool ensureTarget(unsigned int width, unsigned int height);
    bool ensureCompositeResources();
    void updateCompositeTexture();

    RenderTexture* _target      = nullptr;
    rhi::ProgramState* _program = nullptr;
    rhi::VertexLayout* _layout  = nullptr;
    CustomCommand _compositeCommand;
    std::shared_ptr<FrameBinding> _frameBinding;
    Renderer* _activeRenderer = nullptr;
    int _worldQueueId         = -1;
    unsigned int _width       = 0;
    unsigned int _height      = 0;
    bool _scaledFrame         = false;
};

}  // namespace ax
