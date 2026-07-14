/****************************************************************************
 Copyright (c) 2013-2016 Chukong Technologies Inc.
 Copyright (c) 2017-2018 Xiamen Yaji Software Co., Ltd.

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

#include "axmol/renderer/MeshCommand.h"
#include "axmol/base/Macros.h"
#include "axmol/base/Environment.h"
#include "axmol/base/Director.h"
#include "axmol/base/CustomEvent.h"
#include "axmol/base/CustomEventListener.h"
#include "axmol/base/EventDispatcher.h"
#include "axmol/base/EventType.h"
#include "axmol/2d/Light.h"
#include "axmol/scene/Camera.h"
#include "axmol/renderer/Renderer.h"
#include "axmol/renderer/TextureAtlas.h"
#include "axmol/renderer/Texture2D.h"
#include "axmol/renderer/Technique.h"
#include "axmol/renderer/Material.h"
#include "axmol/renderer/Pass.h"
#include "xxhash/xxhash.h"

#include <algorithm>
#include <cstring>

namespace ax
{

MeshCommand::MeshCommand()
#if AX_ENABLE_CONTEXT_LOSS_RECOVERY
    : _rendererRecreatedListener(nullptr)
#endif
{
    _type = RenderCommand::Type::MESH_COMMAND;
    _is3D = true;
#if AX_ENABLE_CONTEXT_LOSS_RECOVERY
    // listen the event that renderer was recreated on Android/WP8
    _rendererRecreatedListener = CustomEventListener::create(EVENT_RENDERER_RECREATED,
                                                             AX_CALLBACK_1(MeshCommand::listenRendererRecreated, this));
    Director::getInstance()->getEventDispatcher()->addEventListenerWithFixedPriority(_rendererRecreatedListener, -1);
#endif
}

void MeshCommand::init(float globalZOrder)
{
    CustomCommand::init(globalZOrder);
    _hasViewProjectionOverride = false;
}

void MeshCommand::init(float globalZOrder, const Mat4& transform)
{
    CustomCommand::init(globalZOrder);
    _hasViewProjectionOverride = false;
    if (Camera::getVisitingCamera())
    {
        _depth = Camera::getVisitingCamera()->getDepthInView(transform);
    }
    _mv = transform;
}

bool MeshCommand::captureProgramState(const rhi::ProgramState& programState)
{
    const auto& uniformBuffer = programState.getUniformBuffer();
    _uniformSnapshot.resize(uniformBuffer.size());
    if (!uniformBuffer.empty())
        std::memcpy(_uniformSnapshot.data(), uniformBuffer.data(), uniformBuffer.size());

    size_t textureCount = 0;
    for (const auto& [location, bindingSet] : programState.getTextureBindingSets())
        textureCount += std::min(bindingSet.slots.size(), bindingSet.texs.size());

    if (textureCount > MAX_SNAPSHOT_TEXTURE_BINDINGS)
    {
        clearProgramStateSnapshot();
        return false;
    }

    for (uint8_t index = 0; index < _textureSnapshotCount; ++index)
        _textureSnapshot[index].texture.reset();
    _textureSnapshotCount = 0;
    for (const auto& [location, bindingSet] : programState.getTextureBindingSets())
    {
        const size_t count = std::min(bindingSet.slots.size(), bindingSet.texs.size());
        for (size_t index = 0; index < count; ++index)
        {
            auto& snapshot                    = _textureSnapshot[_textureSnapshotCount++];
            snapshot.location.location        = static_cast<int16_t>(location);
            snapshot.location.runtimeLocation = static_cast<int16_t>(bindingSet.runtimeLocation);
            snapshot.slot                     = bindingSet.slots[index];
            snapshot.texture                  = bindingSet.texs[index];
        }
    }

    _hasProgramStateSnapshot = true;
    return true;
}

bool MeshCommand::restoreProgramState(rhi::ProgramState& programState) const
{
    if (!_hasProgramStateSnapshot ||
        !programState.restoreUniformBuffer(_uniformSnapshot.data(), _uniformSnapshot.size()))
    {
        return false;
    }

    for (uint8_t index = 0; index < _textureSnapshotCount; ++index)
    {
        const auto& snapshot = _textureSnapshot[index];
        programState.setTexture(snapshot.location, snapshot.slot, snapshot.texture.get());
    }
    return true;
}

void MeshCommand::clearProgramStateSnapshot() noexcept
{
    for (uint8_t index = 0; index < _textureSnapshotCount; ++index)
        _textureSnapshot[index].texture.reset();
    _textureSnapshotCount    = 0;
    _hasProgramStateSnapshot = false;
}

MeshCommand::~MeshCommand()
{
#if AX_ENABLE_CONTEXT_LOSS_RECOVERY
    Director::getInstance()->getEventDispatcher()->removeEventListener(_rendererRecreatedListener);
#endif
}

#if AX_ENABLE_CONTEXT_LOSS_RECOVERY
void MeshCommand::listenRendererRecreated(CustomEvent*)
{
    clearProgramStateSnapshot();
}
#endif

}  // namespace ax
