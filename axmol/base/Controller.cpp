/****************************************************************************
 Copyright (c) 2014 cocos2d-x.org
 Copyright (c) 2014-2016 Chukong Technologies Inc.
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

#include "axmol/base/Controller.h"

#if (AX_TARGET_PLATFORM == AX_PLATFORM_ANDROID || AX_TARGET_PLATFORM == AX_PLATFORM_IOS ||                  \
     AX_TARGET_PLATFORM == AX_PLATFORM_MAC || AX_TARGET_PLATFORM == AX_PLATFORM_LINUX || defined(_WIN32) || \
     AX_TARGET_PLATFORM == AX_PLATFORM_WASM)

#    include "axmol/base/EventDispatcher.h"
#    include "axmol/base/ControllerEvent.h"
#    include "axmol/base/Director.h"

namespace ax
{

std::vector<Controller*> Controller::s_allController;

Controller* Controller::getControllerByTag(int tag)
{
    for (auto&& controller : Controller::s_allController)
    {
        if (controller->_controllerTag == tag)
        {
            return controller;
        }
    }
    return nullptr;
}

Controller* Controller::getControllerByDeviceId(int deviceId)
{
    for (auto&& controller : Controller::s_allController)
    {
        if (controller->_deviceId == deviceId)
        {
            return controller;
        }
    }
    return nullptr;
}

void Controller::init()
{
    _eventDispatcher = Director::getInstance()->getEventDispatcher();
    _connectEvent    = new ControllerEvent(ControllerEvent::ControllerEventType::CONNECTION, this, false);
    _keyEvent        = new ControllerEvent(ControllerEvent::ControllerEventType::BUTTON_STATUS_CHANGED, this, 0);
    _axisEvent       = new ControllerEvent(ControllerEvent::ControllerEventType::AXIS_STATUS_CHANGED, this, 0);
}

const Controller::KeyStatus& Controller::getKeyStatus(int keyCode)
{
    static constexpr KeyStatus EMPTY_STATUS{false, 0.0F, false};
    if (keyCode < Key::JOYSTICK_LEFT_X || keyCode >= Key::KEY_MAX)
        return EMPTY_STATUS;

    return _allKeyStatus[static_cast<std::size_t>(keyCode - Key::JOYSTICK_LEFT_X)];
}

void Controller::onConnected()
{
    _connectEvent->setConnectStatus(true);
    _eventDispatcher->dispatchEvent(_connectEvent);
}

void Controller::onDisconnected()
{
    _connectEvent->setConnectStatus(false);
    _eventDispatcher->dispatchEvent(_connectEvent);

    delete this;
}

void Controller::onButtonEvent(int keyCode, bool isPressed, float value, bool isAnalog)
{
    AXASSERT(keyCode >= Key::JOYSTICK_LEFT_X && keyCode < Key::KEY_MAX, "Controller key code is out of range");
    const auto index                    = static_cast<std::size_t>(keyCode - Key::JOYSTICK_LEFT_X);
    _allKeyPrevStatus[index]            = _allKeyStatus[index];
    _allKeyStatus[index].isPressed      = isPressed;
    _allKeyStatus[index].value          = value;
    _allKeyStatus[index].isAnalog       = isAnalog;

    _keyEvent->setKeyCode(keyCode);
    _eventDispatcher->dispatchEvent(_keyEvent);
}

void Controller::onAxisEvent(int axisCode, float value, bool isAnalog)
{
    AXASSERT(axisCode >= Key::JOYSTICK_LEFT_X && axisCode < Key::KEY_MAX, "Controller axis code is out of range");
    const auto index               = static_cast<std::size_t>(axisCode - Key::JOYSTICK_LEFT_X);
    _allKeyPrevStatus[index]       = _allKeyStatus[index];
    _allKeyStatus[index].value     = value;
    _allKeyStatus[index].isAnalog  = isAnalog;

    _axisEvent->setKeyCode(axisCode);
    _eventDispatcher->dispatchEvent(_axisEvent);
}

}  // namespace ax

#endif  // (AX_TARGET_PLATFORM == AX_PLATFORM_ANDROID || AX_TARGET_PLATFORM == AX_PLATFORM_IOS || AX_TARGET_PLATFORM ==
        // AX_PLATFORM_MAC || AX_TARGET_PLATFORM == AX_PLATFORM_LINUX || defined(_WIN32) || AX_TARGET_PLATFORM ==
        // AX_PLATFORM_WASM)
