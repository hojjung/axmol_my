/****************************************************************************
 Copyright (c) 2013-2016 Chukong Technologies Inc.
 Copyright (c) 2017-2018 Xiamen Yaji Software Co., Ltd.
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
#include <unordered_map>
#include <vector>
#include "axmol/base/RefPtr.h"
#include "axmol/renderer/RenderCommand.h"
#include "axmol/renderer/RenderState.h"
#include "axmol/rhi/ProgramState.h"
#include "axmol/rhi/Texture.h"
#include "axmol/renderer/CustomCommand.h"
#include "axmol/math/Math.h"

namespace ax
{

class CustomEventListener;
class CustomEvent;
class Material;

// it is a common mesh
class AX_DLL MeshCommand : public CustomCommand
{
public:
    // using PrimitiveType = rhi::PrimitiveType;
    /**
    Buffer usage of vertex/index buffer. If the contents is not updated every frame,
    then use STATIC, other use DYNAMIC.
    */
    using BufferUsage = rhi::BufferUsage;
    /**
    The index format determine the size for index data. U_SHORT is enough for most
    cases.
    */
    using IndexFormat = rhi::IndexFormat;

    MeshCommand();
    virtual ~MeshCommand();

    MeshCommand(const MeshCommand&) = default;
    MeshCommand(MeshCommand&&)      = default;

    MeshCommand& operator=(MeshCommand&&)      = default;
    MeshCommand& operator=(const MeshCommand&) = default;

    /**
    Init function. The render command will be in 2D mode.
    @param globalZOrder GlobalZOrder of the render command.
    */
    void init(float globalZOrder);

    void init(float globalZOrder, const Mat4& transform);

    void setViewProjectionOverride(const Mat4& viewProjection)
    {
        _viewProjectionOverride    = viewProjection;
        _hasViewProjectionOverride = true;
    }
    [[nodiscard]] const Mat4* getViewProjectionOverride() const noexcept
    {
        return _hasViewProjectionOverride ? &_viewProjectionOverride : nullptr;
    }

    /**
     * Captures mutable ProgramState data at submission time. Stylized meshes
     * share Pass objects, so the renderer restores this copy immediately
     * before executing the command.
     */
    bool captureProgramState(const rhi::ProgramState& programState);
    bool restoreProgramState(rhi::ProgramState& programState) const;
    void clearProgramStateSnapshot() noexcept;
    [[nodiscard]] bool hasProgramStateSnapshot() const noexcept { return _hasProgramStateSnapshot; }

#if AX_ENABLE_CONTEXT_LOSS_RECOVERY
    void listenRendererRecreated(CustomEvent* event);
#endif

protected:
    struct TextureBindingSnapshot
    {
        rhi::UniformLocation location{};
        int slot = -1;
        RefPtr<rhi::Texture> texture;
    };

    static constexpr size_t MAX_SNAPSHOT_TEXTURE_BINDINGS = 4;

    Mat4 _viewProjectionOverride    = Mat4::identity;
    bool _hasViewProjectionOverride = false;
    std::vector<uint8_t> _uniformSnapshot;
    std::array<TextureBindingSnapshot, MAX_SNAPSHOT_TEXTURE_BINDINGS> _textureSnapshot{};
    uint8_t _textureSnapshotCount = 0;
    bool _hasProgramStateSnapshot = false;
#if AX_ENABLE_CONTEXT_LOSS_RECOVERY
    CustomEventListener* _rendererRecreatedListener;
#endif
};

}  // namespace ax
