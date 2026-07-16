#include "axmol/rhi/opengl/OpenGLState.h"
#include <memory>
namespace ax::rhi::gl
{

static std::unique_ptr<OpenGLState> g_defaultOpenGLState;
static bool g_nativeObjectsInvalidated = false;

AX_DLL OpenGLState* __state{nullptr};

void OpenGLState::reset()
{
    g_defaultOpenGLState = std::make_unique<OpenGLState>();
    __state              = g_defaultOpenGLState.get();
}

void OpenGLState::setNativeObjectsInvalidated(bool invalidated)
{
    g_nativeObjectsInvalidated = invalidated;
}

bool OpenGLState::areNativeObjectsInvalidated()
{
    return g_nativeObjectsInvalidated;
}

OpenGLState::OpenGLState() {}

OpenGLState::~OpenGLState() {}

}  // namespace ax::rhi::gl
