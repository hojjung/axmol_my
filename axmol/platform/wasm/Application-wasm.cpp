/****************************************************************************
Copyright (c) 2011      Laschweinski
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

#include "axmol/platform/PlatformConfig.h"
#if AX_TARGET_PLATFORM == AX_PLATFORM_WASM

#    include "axmol/platform/wasm/Application-wasm.h"
#    if AX_WASM_ENABLE_DEVTOOLS
#        include "axmol/platform/wasm/devtools-wasm.h"
#    endif
#    include <unistd.h>
#    include <sys/time.h>
#    include <string>
#    include "axmol/base/Director.h"
#    include "axmol/base/Utils.h"
#    include "axmol/platform/GL.h"
#    include "axmol/platform/FileUtils.h"
#    include "axmol/platform/Device.h"
#    include "axmol/tlx/utility.hpp"
#    include <algorithm>
#    include <array>
#    include <emscripten/emscripten.h>
#    include <emscripten/html5_webgl.h>
#    if defined(AX_ENABLE_3D) && AX_ENABLE_3D
#        include "axmol/3d/StylizedRenderer.h"
#        include "axmol/scene/Scene.h"
#    endif

extern void _axmolPerformFrameBoundaryTasks();

extern void axmol_wasm_app_exit();

namespace
{
constexpr size_t WEBGL_GPU_QUERY_COUNT = 4;

class WebGLGpuFrameTimer final
{
public:
    void initialize()
    {
        abandon();
        const auto context = emscripten_webgl_get_current_context();
        if (context <= 0 || !emscripten_webgl_enable_extension(context, "EXT_disjoint_timer_query_webgl2"))
            return;

        std::array<GLuint, WEBGL_GPU_QUERY_COUNT> ids{};
        glGenQueries(static_cast<GLsizei>(ids.size()), ids.data());
        for (size_t index = 0; index < ids.size(); ++index)
            _queries[index].id = ids[index];
        _supported = std::all_of(ids.begin(), ids.end(), [](GLuint id) { return id != 0; });
    }

    void begin()
    {
        if (!_supported || _activeQuery != INVALID_QUERY)
            return;

        consumeCompletedQueries();
        for (size_t offset = 0; offset < _queries.size(); ++offset)
        {
            const size_t index = (_nextQuery + offset) % _queries.size();
            if (_queries[index].pending)
                continue;

            glBeginQuery(GL_TIME_ELAPSED_EXT, _queries[index].id);
            _activeQuery = index;
            _nextQuery   = (index + 1) % _queries.size();
            return;
        }
    }

    void end()
    {
        if (_activeQuery == INVALID_QUERY)
            return;
        glEndQuery(GL_TIME_ELAPSED_EXT);
        _queries[_activeQuery].pending = true;
        _activeQuery                   = INVALID_QUERY;
    }

    // A lost WebGL context deletes query objects. Do not call glDeleteQueries
    // after the loss event; just forget their now-invalid numeric names.
    void abandon() noexcept
    {
        _queries     = {};
        _activeQuery = INVALID_QUERY;
        _nextQuery   = 0;
        _supported   = false;
    }

private:
    struct Query
    {
        GLuint id    = 0;
        bool pending = false;
    };

    static constexpr size_t INVALID_QUERY = static_cast<size_t>(-1);

    void consumeCompletedQueries()
    {
        GLint disjoint = GL_FALSE;
        glGetIntegerv(GL_GPU_DISJOINT_EXT, &disjoint);
        if (disjoint != GL_FALSE)
        {
            for (auto& query : _queries)
                query.pending = false;
            return;
        }

        for (auto& query : _queries)
        {
            if (!query.pending)
                continue;

            GLuint available = GL_FALSE;
            glGetQueryObjectuiv(query.id, GL_QUERY_RESULT_AVAILABLE, &available);
            if (available == GL_FALSE)
                continue;

            GLuint elapsedNanoseconds = 0;
            glGetQueryObjectuiv(query.id, GL_QUERY_RESULT, &elapsedNanoseconds);
            query.pending = false;

#    if defined(AX_ENABLE_3D) && AX_ENABLE_3D
            auto* director = ax::Director::getInstance();
            auto* scene    = director ? director->getRunningScene() : nullptr;
            auto* renderer = scene ? ax::StylizedRenderer::get(*scene) : nullptr;
            if (renderer)
                renderer->recordGpuFrameTime(static_cast<float>(elapsedNanoseconds) * 1.0e-6F);
#    endif
        }
    }

    std::array<Query, WEBGL_GPU_QUERY_COUNT> _queries{};
    size_t _activeQuery = INVALID_QUERY;
    size_t _nextQuery   = 0;
    bool _supported     = false;
};

WebGLGpuFrameTimer s_gpuFrameTimer;
bool s_webglContextLost = false;
}  // namespace

extern "C" {
//
EMSCRIPTEN_KEEPALIVE void axmol_hdoc_visibilitychange(bool hidden)
{
    ax::CustomEvent event(hidden ? EVENT_COME_TO_BACKGROUND : EVENT_COME_TO_FOREGROUND);
    ax::Director::getInstance()->getEventDispatcher()->dispatchEvent(&event, true);
}

// webglcontextlost
EMSCRIPTEN_KEEPALIVE void axmol_webglcontextlost()
{
    AXLOGI("receive event: webglcontextlost");
    s_webglContextLost = true;
    s_gpuFrameTimer.abandon();
}

// webglcontextrestored
EMSCRIPTEN_KEEPALIVE void axmol_webglcontextrestored()
{
    AXLOGI("receive event: webglcontextrestored");

    auto director = ax::Director::getInstance();
    axdrv->resetState();
    ax::CustomEvent recreatedEvent(EVENT_RENDERER_RECREATED);
    director->getEventDispatcher()->dispatchEvent(&recreatedEvent, true);
    director->setRenderDefaults();
#    if AX_ENABLE_CONTEXT_LOSS_RECOVERY
    ax::VolatileTextureMgr::reloadAllTextures();
#    endif
    s_gpuFrameTimer.initialize();
    s_webglContextLost = false;
}

#    if AX_WASM_ENABLE_DEVTOOLS
EMSCRIPTEN_KEEPALIVE void axmol_dev_pause()
{
    ax::DevToolsImpl::getInstance()->pause();
}

EMSCRIPTEN_KEEPALIVE void axmol_dev_resume()
{
    ax::DevToolsImpl::getInstance()->resume();
}

EMSCRIPTEN_KEEPALIVE void axmol_dev_step()
{
    ax::DevToolsImpl::getInstance()->step();
}
#    endif
}

namespace ax
{

static int64_t NANOSECONDSPERSECOND      = 1000000000LL;
static int64_t NANOSECONDSPERMICROSECOND = 1000000LL;
static int64_t FPS_CONTROL_THRESHOLD     = static_cast<int64_t>(1.0f / 1200.0f * NANOSECONDSPERSECOND);

static Director* __director;

static int s_targetFPS        = 0;    // 0 = follow browser refresh rate
static double s_lastFrameTime = 0.0;  // ms
static double s_accumulator   = 0.0;  // ms

static void stepFrame();

static void updateFrame()
{
    double now = emscripten_get_now();  // current time in ms
    _axmolPerformFrameBoundaryTasks();  // Perform any pending frame boundary tasks before processing the next frame.

    // Browsers keep requestAnimationFrame alive while a WebGL context is lost.
    // Skip engine rendering until the restored event has rebuilt GPU resources.
    if (s_webglContextLost) [[unlikely]]
    {
        s_lastFrameTime = now;
        s_accumulator   = 0.0;
        return;
    }

    // First frame: render immediately
    if (s_lastFrameTime <= 0.0) [[unlikely]]
    {
        stepFrame();
        s_lastFrameTime = now;
        return;
    }

    double delta    = now - s_lastFrameTime;
    s_lastFrameTime = now;

    // Protect against long suspension (e.g. tab inactive for seconds/minutes)
    if (delta > 1000.0) [[unlikely]]
    {
        s_accumulator = 0.0;
        stepFrame();
        return;
    }

    s_accumulator += delta;

    // If targetFPS is 0, follow browser refresh rate directly
    if (s_targetFPS <= 0)
    {
        stepFrame();
        return;
    }

    // Frame skipping logic based on accumulator
    double targetInterval = 1000.0 / s_targetFPS;
    if (s_accumulator >= targetInterval)
    {
        stepFrame();
        s_accumulator -= targetInterval;
        if (s_accumulator < 0.0) [[unlikely]]  // floating-point safety
            s_accumulator = 0.0;
    }  // else onIdle
}

static void stepFrame()
{
    auto director   = __director;
    auto renderView = director->getRenderView();

    s_gpuFrameTimer.begin();
    director->stepFrame();
    s_gpuFrameTimer.end();

    if (renderView->windowShouldClose())
    {
        AXLOGI("shuting down axmol wasm app ...");
        emscripten_cancel_main_loop();  // Cancel current loop and set the cleanup one.

        if (renderView->isGfxContextReady())
        {
            director->end();
            director->stepFrame();
        }
        renderView->release();

        axmol_wasm_app_exit();
    }
}

static void getCurrentLangISO2(char buf[16])
{
    // clang-format off
    EM_ASM_ARGS(
        {
            var lang = window.localStorage.getItem('localization_language');
            if (lang == null)
            {
                stringToUTF8(window.navigator.language.replace(/-.*/, ""), $0, 16);
            }
            else
            {
                stringToUTF8(lang, $0, 16);
            }
        },
        buf);
    // clang-format on
}

Application::Application()
{
    AX_ASSERT(!s_axmolApp);
    s_axmolApp = this;
}

Application::~Application()
{
    AX_ASSERT(this == s_axmolApp);
    s_axmolApp = nullptr;
}

int Application::run()
{
    applicationWillLaunch();
    // Initialize instance and axmol.
    if (!applicationDidFinishLaunching())
    {
        return 1;
    }

    __director = Director::getInstance();
    s_gpuFrameTimer.initialize();

    // Retain glview to avoid glview being released in the while loop
    __director->getRenderView()->retain();

    /*
    The JavaScript environment will call that function at a specified number
    of frames per second. If called on the main browser thread, setting 0 or
    a negative value as the fps will use the browser’s requestAnimationFrame mechanism
    o call the main loop function. This is HIGHLY recommended if you are doing rendering,
    as the browser’s requestAnimationFrame will make sure you render at a proper smooth rate
    that lines up properly with the browser and monitor.
    */
#    if AX_WASM_TIMING_USE_TIMEOUT
    emscripten_set_main_loop(updateFrame, s_targetFPS, false);
#    else
    emscripten_set_main_loop(updateFrame, -1, false);
#    endif

    return 0;
}

void Application::setAnimationInterval(float interval)
{
    if (interval <= 0.0) [[unlikely]]
    {
        // 0 or negative means follow browser refresh rate
        s_targetFPS = 0;
        return;
    }
    const auto desiredFPS = static_cast<int>(std::lround(1 / interval));
    s_targetFPS           = std::clamp(desiredFPS, Device::MIN_REFRESH_RATE, Device::MAX_REFRESH_RATE);
}

Application::Platform Application::getTargetPlatform()
{
    return Platform::Wasm;
}

std::string Application::getVersion()
{
    return "";
}

bool Application::openURL(std::string_view url)
{
    EM_ASM_ARGS({ window.open(UTF8ToString($0)); }, url.data());

    return true;
}

const char* Application::getCurrentLanguageCode()
{
    static char code[3]    = {0};
    char pLanguageName[16] = {0};
    getCurrentLangISO2(pLanguageName);
    tlx::strlcpy(code, pLanguageName);
    return code;
}

LanguageType Application::getCurrentLanguage()
{
    auto langCode = getCurrentLanguageCode();
    return utils::getLanguageTypeByISO2(langCode);
}

}  // namespace ax

#endif  // AX_TARGET_PLATFORM == AX_PLATFORM_WASM
