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

#include <functional>
#if !defined(AX_HEADLESS_SERVER) || !AX_HEADLESS_SERVER
#    include <set>
#endif
#include <string>
#include <unordered_map>
#include <vector>

#include "axmol/platform/PlatformMacros.h"
#include "axmol/base/EventListener.h"
#include "axmol/base/Event.h"
#if !defined(AX_HEADLESS_SERVER) || !AX_HEADLESS_SERVER
#    include "axmol/base/PointerEvent.h"
#    include "axmol/base/WeakPtr.h"
#endif
#include "axmol/platform/StdC.h"
#include "axmol/tlx/hlookup.hpp"

/**
 * @addtogroup base
 * @{
 */

namespace ax
{

class Event;
class CustomEvent;
class CustomEventListener;
#if !defined(AX_HEADLESS_SERVER) || !AX_HEADLESS_SERVER
class PointerEvent;
class Node;
class PointerEventListener;
class Camera;
#endif

/** @class EventDispatcher
* @brief This class manages event listener subscriptions
and event dispatching.

The EventListener list is managed in such a way that
event listeners can be added and removed even
from within an EventListener, while events are being
dispatched.
*/
class AX_DLL EventDispatcher : public Object
{
public:
    // Adds event listener.

#if !defined(AX_HEADLESS_SERVER) || !AX_HEADLESS_SERVER
    /** Adds a event listener for a specified event with the priority of scene graph.
     *  @param listener The listener of a specified event.
     *  @param node The priority of the listener is based on the draw order of this node.
     *  @note  The priority of scene graph will be fixed value 0. So the order of listener item
     *          in the vector will be ' <0, scene graph (0 priority), >0'.
     */
    void addEventListenerWithSceneGraphPriority(EventListener* listener, Node* node);
#endif

    /** Adds a event listener for a specified event with the fixed priority.
     *  @param listener The listener of a specified event.
     *  @param fixedPriority The fixed priority of the listener.
     *  @note A lower priority will be called before the ones that have a higher value.
     *        0 priority is forbidden for fixed priority since it's used for scene graph based priority.
     */
    void addEventListenerWithFixedPriority(EventListener* listener, int fixedPriority);

    /** Adds a Custom event listener.
     It will use a fixed priority of 1.
     * @param eventName A given name of the event.
     * @param callback A given callback method that associated the event name.
     * @return the generated event. Needed in order to remove the event from the dispatcher
     */
    CustomEventListener* addCustomEventListener(std::string_view eventName,
                                                const std::function<void(CustomEvent*)>& callback,
                                                int priority = 1);

    /////////////////////////////////////////////

    // Removes event listener

    /** Remove a listener.
     *
     *  @param listener The specified event listener which needs to be removed.
     */
    void removeEventListener(EventListener* listener);

    /** Removes all listeners with the same event listener type.
     *
     * @param listenerType A given event listener type which needs to be removed.
     */
    void removeEventListenersForType(EventListener::Type listenerType);

#if !defined(AX_HEADLESS_SERVER) || !AX_HEADLESS_SERVER
    /** Removes all listeners which are associated with the specified target.
     *
     * @param target A given target node.
     * @param recursive True if remove recursively, the default value is false.
     */
    void removeEventListenersForTarget(Node* target, bool recursive = false);
#endif

    /** Removes all custom listeners with the same event name.
     *
     * @param customEventName A given event listener name which needs to be removed.
     */
    void removeCustomEventListeners(std::string_view customEventName);

    /** Removes all listeners.
     */
    void removeAllEventListeners();

    /////////////////////////////////////////////

    // Pauses / Resumes event listener

#if !defined(AX_HEADLESS_SERVER) || !AX_HEADLESS_SERVER
    /** Pauses all listeners which are associated the specified target.
     *
     * @param target A given target node.
     * @param recursive True if pause recursively, the default value is false.
     */
    void pauseEventListenersForTarget(Node* target, bool recursive = false);

    /** Resumes all listeners which are associated the specified target.
     *
     * @param target A given target node.
     * @param recursive True if resume recursively, the default value is false.
     */
    void resumeEventListenersForTarget(Node* target, bool recursive = false);
#endif

    /////////////////////////////////////////////

    /** Sets listener's priority with fixed value.
     *
     * @param listener A given listener.
     * @param fixedPriority The fixed priority value.
     */
    void setPriority(EventListener* listener, int fixedPriority);

    /** Whether to enable dispatching events.
     *
     * @param isEnabled  True if enable dispatching events.
     */
    void setEnabled(bool isEnabled);

    /** Checks whether dispatching events is enabled.
     *
     * @return True if dispatching events is enabled.
     */
    bool isEnabled() const;

    /////////////////////////////////////////////

    /** Dispatches the event.
     *  Also removes all EventListeners marked for deletion from the
     *  event dispatcher list.
     *
     * @param event The event needs to be dispatched.
     * @param forced If the event should be sent out regardless of enabled state
     */
    void dispatchEvent(Event* event, bool forced = false);

    /** Dispatches a Custom Event with a event name an optional user data.
     *
     * @param eventName The name of the event which needs to be dispatched.
     * @param optionalUserData The optional user data, it's a void*, the default value is nullptr.
     * @param forced If the event should be sent out regardless of enabled state
     */
    void dispatchCustomEvent(std::string_view eventName, void* optionalUserData = nullptr, bool forced = false);

    /** Query whether the specified event listener id has been added.
     *
     * @param listenerID The listenerID of the event listener id.
     *
     * @return True if dispatching events is exist
     */
    bool hasEventListener(std::string_view listenerID) const;

    /////////////////////////////////////////////

    /** Constructor of EventDispatcher.
     */
    EventDispatcher();
    /** Destructor of EventDispatcher.
     */
    ~EventDispatcher();

#if (!defined(AX_HEADLESS_SERVER) || !AX_HEADLESS_SERVER) && AX_NODE_DEBUG_VERIFY_EVENT_LISTENERS && _AX_DEBUG > 0

    /**
     * To help track down event listener issues in debug builds.
     * Verifies that the node has no event listeners associated with it when destroyed.
     */
    void debugCheckNodeHasNoEventListenersOnDestruction(Node* node);

#endif

protected:
#if !defined(AX_HEADLESS_SERVER) || !AX_HEADLESS_SERVER
    friend class Node;

    /** Sets the dirty flag for a node. */
    void setDirtyForNode(Node* node);
#endif

    /**
     *  The vector to store event listeners with scene graph based priority and fixed priority.
     */
    class EventListenerVector
    {
    public:
        EventListenerVector();
        ~EventListenerVector();
        size_t size() const;
        bool empty() const;

        void emplace_back(EventListener* item);
#if !defined(AX_HEADLESS_SERVER) || !AX_HEADLESS_SERVER
        void clearSceneGraphListeners();
#endif
        void clearFixedListeners();
        void clear();

        std::vector<EventListener*>* getFixedPriorityListeners() const { return _fixedListeners; }
#if !defined(AX_HEADLESS_SERVER) || !AX_HEADLESS_SERVER
        std::vector<EventListener*>* getSceneGraphPriorityListeners() const { return _sceneGraphListeners; }
        ssize_t getGt0Index() const { return _gt0Index; }
        void setGt0Index(ssize_t index) { _gt0Index = index; }
#endif

    private:
        std::vector<EventListener*>* _fixedListeners;
#if !defined(AX_HEADLESS_SERVER) || !AX_HEADLESS_SERVER
        std::vector<EventListener*>* _sceneGraphListeners;
        ssize_t _gt0Index;
#endif
    };

    /** Adds an event listener with item
     *  @note if it is dispatching event, the added operation will be delayed to the end of current dispatch
     *  @see forceAddEventListener
     */
    void addEventListener(EventListener* listener);

    /** Force adding an event listener
     *  @note force add an event listener which will ignore whether it's in dispatching.
     *  @see addEventListener
     */
    void forceAddEventListener(EventListener* listener);

    /** Gets event the listener list for the event listener type. */
    EventListenerVector* getListeners(std::string_view listenerID) const;

#if !defined(AX_HEADLESS_SERVER) || !AX_HEADLESS_SERVER
    /** Update dirty flag */
    void updateDirtyFlagForSceneGraph();
#endif

    /** Removes all listeners with the same event listener ID */
    void removeEventListenersForListenerID(std::string_view listenerID);

    /** Sort event listener */
    void sortEventListeners(std::string_view listenerID);

#if !defined(AX_HEADLESS_SERVER) || !AX_HEADLESS_SERVER
    /** Sorts the listeners of specified type by scene graph priority */
    void sortEventListenersOfSceneGraphPriority(std::string_view listenerID, Node* rootNode);
#endif

    /** Sorts the listeners of specified type by fixed priority */
    void sortEventListenersOfFixedPriority(std::string_view listenerID);

    /** Updates all listeners
     *  1) Removes all listener items that have been marked as 'removed' when dispatching event.
     *  2) Adds all listener items that have been marked as 'added' when dispatching event.
     */
    void updateListeners(Event* event);

#if !defined(AX_HEADLESS_SERVER) || !AX_HEADLESS_SERVER
    void dispatchPointerEvent(PointerEvent* event);
#endif

#if !defined(AX_HEADLESS_SERVER) || !AX_HEADLESS_SERVER
    /** Associates node with event listener */
    void associateNodeAndEventListener(Node* node, EventListener* listener);

    /** Dissociates node with event listener */
    void dissociateNodeAndEventListener(Node* node, EventListener* listener);
#endif

    /** Dispatches event to listeners with a specified listener type */
    void dispatchEventToListeners(EventListenerVector* listeners, const std::function<bool(EventListener*)>& onEvent);

    void releaseListener(EventListener* listener);

#if !defined(AX_HEADLESS_SERVER) || !AX_HEADLESS_SERVER
    void removeCapturedPointerListener(EventListener* listener);
    void removeCapturedPointerListenersForTarget(Node* target);

    static const Camera* findHitCameraForListener(PointerEvent* event,
                                                  PointerEventListener* listener,
                                                  const std::vector<Camera*>& cameras);
#endif

    /// Priority dirty flag
    enum class DirtyFlag
    {
        NONE           = 0,
        FIXED_PRIORITY = 1 << 0,
#if !defined(AX_HEADLESS_SERVER) || !AX_HEADLESS_SERVER
        SCENE_GRAPH_PRIORITY = 1 << 1,
        ALL                  = FIXED_PRIORITY | SCENE_GRAPH_PRIORITY
#else
        ALL = FIXED_PRIORITY
#endif
    };

    /** Sets the dirty flag for a specified listener ID */
    void setDirty(std::string_view listenerID, DirtyFlag flag);

#if !defined(AX_HEADLESS_SERVER) || !AX_HEADLESS_SERVER
    /** Walks though scene graph to get the draw order for each node, it's called before sorting event listener with
     * scene graph priority */
    void visitTarget(Node* node, bool isRootNode);
#endif

    /** Remove all listeners in _toRemoveListeners list and cleanup */
    void cleanToRemovedListeners();

#if !defined(AX_HEADLESS_SERVER) || !AX_HEADLESS_SERVER
    using PointerCaptureId = uint64_t;
    struct PointerCaptureEntry
    {
        WeakPtr<PointerEventListener> listener{nullptr};
        PointerEvent::CaptureBits captureBits{PointerEvent::CAPTURE_NONE};
        WeakPtr<Camera> camera{nullptr};
    };

    bool dispatchCapturedPointerEvent(PointerEvent* event);
    void dispatchUncapturedPointerEvent(PointerEvent* event, PointerCaptureId captureId);
#endif

    /** Listeners map */
    tlx::string_map<EventListenerVector*> _listenerMap;

    /** The map of dirty flag */
    tlx::string_map<DirtyFlag> _priorityDirtyFlagMap;

#if !defined(AX_HEADLESS_SERVER) || !AX_HEADLESS_SERVER
    /** The map of node and event listeners */
    tlx::hash_map<Node*, std::vector<EventListener*>*> _nodeListenersMap;

    /** The map of node and its event priority */
    tlx::hash_map<Node*, int> _nodePriorityMap;

    /** key: Global Z Order, value: Sorted Nodes */
    tlx::hash_map<float, std::vector<Node*>> _globalZOrderNodeMap;

    tlx::hash_map<PointerCaptureId, PointerCaptureEntry> _capturedPointerListeners;
#endif

    /** The listeners to be added after dispatching event */
    std::vector<EventListener*> _toAddedListeners;

    /** The listeners to be removed after dispatching event */
    std::vector<EventListener*> _toRemovedListeners;

#if !defined(AX_HEADLESS_SERVER) || !AX_HEADLESS_SERVER
    /** The nodes were associated with scene graph based priority listeners */
    std::set<Node*> _dirtyNodes;
#endif

    /** Whether the dispatcher is dispatching event */
    int _inDispatch;

    /** Whether to enable dispatching event */
    bool _isEnabled;

#if !defined(AX_HEADLESS_SERVER) || !AX_HEADLESS_SERVER
    int _nodePriorityIndex;
    std::set<std::string> _internalCustomListenerIDs;
#endif
};

}  // namespace ax

// end of base group
/// @}
