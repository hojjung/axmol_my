#include "axmol/base/EventDispatcher.h"

#include "axmol/base/CustomEvent.h"
#include "axmol/base/CustomEventListener.h"
#include "axmol/base/Macros.h"

#include <algorithm>

namespace
{
class DispatchGuard final
{
public:
    explicit DispatchGuard(int& depth) noexcept : depth_(depth) { ++depth_; }
    ~DispatchGuard() { --depth_; }

private:
    int& depth_;
};
}  // namespace

namespace ax
{
EventDispatcher::EventListenerVector::EventListenerVector() : _fixedListeners(nullptr) {}

EventDispatcher::EventListenerVector::~EventListenerVector()
{
    AX_SAFE_DELETE(_fixedListeners);
}

size_t EventDispatcher::EventListenerVector::size() const
{
    size_t result = 0;
    if (_fixedListeners)
        result += _fixedListeners->size();
    return result;
}

bool EventDispatcher::EventListenerVector::empty() const
{
    return _fixedListeners == nullptr || _fixedListeners->empty();
}

void EventDispatcher::EventListenerVector::emplace_back(EventListener* listener)
{
    if (listener->getFixedPriority() == 0)
        return;

    if (_fixedListeners == nullptr)
    {
        _fixedListeners = new std::vector<EventListener*>();
        _fixedListeners->reserve(32);
    }
    _fixedListeners->emplace_back(listener);
}

void EventDispatcher::EventListenerVector::clearFixedListeners()
{
    AX_SAFE_DELETE(_fixedListeners);
}

void EventDispatcher::EventListenerVector::clear()
{
    clearFixedListeners();
}

EventDispatcher::EventDispatcher() : _inDispatch(0), _isEnabled(false)
{
    _toAddedListeners.reserve(32);
    _toRemovedListeners.reserve(32);
}

EventDispatcher::~EventDispatcher()
{
    removeAllEventListeners();
}

void EventDispatcher::addEventListener(EventListener* listener)
{
    if (_inDispatch == 0)
        forceAddEventListener(listener);
    else
        _toAddedListeners.emplace_back(listener);
    listener->retain();
}

void EventDispatcher::forceAddEventListener(EventListener* listener)
{
    const auto listenerId          = listener->getListenerID();
    auto found                     = _listenerMap.find(listenerId);
    EventListenerVector* listeners = nullptr;
    if (found == _listenerMap.end())
    {
        listeners = new EventListenerVector();
        _listenerMap.emplace(listenerId, listeners);
    }
    else
    {
        listeners = found->second;
    }

    listeners->emplace_back(listener);
    setDirty(listenerId, DirtyFlag::FIXED_PRIORITY);
}

void EventDispatcher::addEventListenerWithFixedPriority(EventListener* listener, int fixedPriority)
{
    if (listener == nullptr || listener->isAttached() || fixedPriority == 0 || !listener->checkAvailable())
        return;

    listener->setAssociatedNode(nullptr);
    listener->setFixedPriority(fixedPriority);
    listener->setAttached(true);
    listener->setPaused(false);
    addEventListener(listener);
}

CustomEventListener* EventDispatcher::addCustomEventListener(std::string_view eventName,
                                                             const std::function<void(CustomEvent*)>& callback,
                                                             int priority)
{
    if (priority == 0)
        return nullptr;

    auto* listener = new CustomEventListener();
    if (!listener->init(eventName, callback))
    {
        delete listener;
        return nullptr;
    }
    addEventListenerWithFixedPriority(listener, priority);
    listener->release();
    return listener;
}

void EventDispatcher::removeEventListener(EventListener* listener)
{
    if (listener == nullptr ||
        std::find(_toRemovedListeners.begin(), _toRemovedListeners.end(), listener) != _toRemovedListeners.end())
        return;

    bool foundListener = false;
    for (auto mapIt = _listenerMap.begin(); mapIt != _listenerMap.end() && !foundListener;)
    {
        auto* listeners = mapIt->second;
        auto* fixed     = listeners->getFixedPriorityListeners();
        if (fixed)
        {
            const auto found = std::find(fixed->begin(), fixed->end(), listener);
            if (found != fixed->end())
            {
                listener->retain();
                listener->setAttached(false);
                if (_inDispatch == 0)
                {
                    fixed->erase(found);
                    releaseListener(listener);
                }
                else
                {
                    _toRemovedListeners.emplace_back(listener);
                }
                foundListener = true;
            }
        }

        if (listeners->empty())
        {
            _priorityDirtyFlagMap.erase(listener->getListenerID());
            delete listeners;
            mapIt = _listenerMap.erase(mapIt);
        }
        else
        {
            ++mapIt;
        }
    }

    if (foundListener)
    {
        releaseListener(listener);
        return;
    }

    const auto pending = std::find(_toAddedListeners.begin(), _toAddedListeners.end(), listener);
    if (pending != _toAddedListeners.end())
    {
        listener->setAttached(false);
        releaseListener(listener);
        _toAddedListeners.erase(pending);
    }
}

void EventDispatcher::removeEventListenersForType(EventListener::Type listenerType)
{
    if (listenerType == EventListener::Type::CUSTOM)
        removeAllEventListeners();
}

void EventDispatcher::setPriority(EventListener* listener, int fixedPriority)
{
    if (listener == nullptr || fixedPriority == 0)
        return;

    for (auto& entry : _listenerMap)
    {
        auto* fixed = entry.second->getFixedPriorityListeners();
        if (fixed && std::find(fixed->begin(), fixed->end(), listener) != fixed->end())
        {
            if (listener->getFixedPriority() != fixedPriority)
            {
                listener->setFixedPriority(fixedPriority);
                setDirty(listener->getListenerID(), DirtyFlag::FIXED_PRIORITY);
            }
            return;
        }
    }
}

void EventDispatcher::dispatchEventToListeners(EventListenerVector* listeners,
                                               const std::function<bool(EventListener*)>& onEvent)
{
    auto* fixed = listeners->getFixedPriorityListeners();
    if (fixed == nullptr)
        return;

    for (EventListener* listener : *fixed)
    {
        if (listener->isEnabled() && !listener->isPaused() && listener->isAttached() && onEvent(listener))
            break;
    }
}

void EventDispatcher::dispatchEvent(Event* event, bool forced)
{
    if (event == nullptr || (!_isEnabled && !forced) || event->getType() != Event::Type::CUSTOM)
        return;

    DispatchGuard guard(_inDispatch);
    const auto* customEvent = static_cast<CustomEvent*>(event);
    const auto listenerId   = customEvent->getEventName();
    sortEventListeners(listenerId);

    const auto found = _listenerMap.find(listenerId);
    try
    {
        if (found != _listenerMap.end())
        {
            dispatchEventToListeners(found->second, [event](EventListener* listener) {
                event->setCurrentTarget(nullptr);
                listener->_onEvent(event);
                return event->isStopped();
            });
        }
    }
    catch (...)
    {
        updateListeners(event);
        throw;
    }
    updateListeners(event);
}

void EventDispatcher::dispatchCustomEvent(std::string_view eventName, void* userData, bool forced)
{
    CustomEvent event(eventName);
    event.setUserData(userData);
    dispatchEvent(&event, forced);
}

bool EventDispatcher::hasEventListener(std::string_view listenerId) const
{
    return getListeners(listenerId) != nullptr;
}

void EventDispatcher::updateListeners(Event* event)
{
    if (_inDispatch > 1)
        return;

    static_cast<void>(event);
    for (auto mapIt = _listenerMap.begin(); mapIt != _listenerMap.end();)
    {
        auto* listeners = mapIt->second;
        auto* fixed     = listeners->getFixedPriorityListeners();
        if (fixed)
        {
            for (auto it = fixed->begin(); it != fixed->end();)
            {
                EventListener* listener = *it;
                if (!listener->isAttached())
                {
                    it                 = fixed->erase(it);
                    const auto removed = std::find(_toRemovedListeners.begin(), _toRemovedListeners.end(), listener);
                    if (removed != _toRemovedListeners.end())
                        _toRemovedListeners.erase(removed);
                    releaseListener(listener);
                }
                else
                {
                    ++it;
                }
            }
            if (fixed->empty())
                listeners->clearFixedListeners();
        }

        if (listeners->empty())
        {
            _priorityDirtyFlagMap.erase(mapIt->first);
            delete listeners;
            mapIt = _listenerMap.erase(mapIt);
        }
        else
        {
            ++mapIt;
        }
    }

    for (EventListener* listener : _toAddedListeners)
        forceAddEventListener(listener);
    _toAddedListeners.clear();
    cleanToRemovedListeners();
}

void EventDispatcher::sortEventListeners(std::string_view listenerId)
{
    const auto dirty = _priorityDirtyFlagMap.find(listenerId);
    if (dirty == _priorityDirtyFlagMap.end() || dirty->second == DirtyFlag::NONE)
        return;
    dirty->second = DirtyFlag::NONE;
    sortEventListenersOfFixedPriority(listenerId);
}

void EventDispatcher::sortEventListenersOfFixedPriority(std::string_view listenerId)
{
    auto* listeners = getListeners(listenerId);
    auto* fixed     = listeners ? listeners->getFixedPriorityListeners() : nullptr;
    if (fixed == nullptr)
        return;

    std::stable_sort(fixed->begin(), fixed->end(), [](const EventListener* lhs, const EventListener* rhs) {
        return lhs->getFixedPriority() < rhs->getFixedPriority();
    });
}

EventDispatcher::EventListenerVector* EventDispatcher::getListeners(std::string_view listenerId) const
{
    const auto found = _listenerMap.find(listenerId);
    return found == _listenerMap.end() ? nullptr : found->second;
}

void EventDispatcher::removeEventListenersForListenerID(std::string_view listenerId)
{
    const auto found = _listenerMap.find(listenerId);
    if (found != _listenerMap.end())
    {
        auto* listeners = found->second;
        auto* fixed     = listeners->getFixedPriorityListeners();
        if (fixed)
        {
            for (auto it = fixed->begin(); it != fixed->end();)
            {
                EventListener* listener = *it;
                listener->setAttached(false);
                if (_inDispatch == 0)
                {
                    it = fixed->erase(it);
                    releaseListener(listener);
                }
                else
                {
                    ++it;
                }
            }
        }
        _priorityDirtyFlagMap.erase(listenerId);
        if (_inDispatch == 0)
        {
            listeners->clear();
            delete listeners;
            _listenerMap.erase(found);
        }
    }

    for (auto it = _toAddedListeners.begin(); it != _toAddedListeners.end();)
    {
        EventListener* listener = *it;
        if (listener->getListenerID() == listenerId)
        {
            listener->setAttached(false);
            releaseListener(listener);
            it = _toAddedListeners.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void EventDispatcher::removeCustomEventListeners(std::string_view eventName)
{
    removeEventListenersForListenerID(eventName);
}

void EventDispatcher::removeAllEventListeners()
{
    std::vector<std::string> listenerIds;
    listenerIds.reserve(_listenerMap.size());
    for (const auto& entry : _listenerMap)
        listenerIds.emplace_back(entry.first);
    for (const std::string& listenerId : listenerIds)
        removeEventListenersForListenerID(listenerId);
}

void EventDispatcher::setEnabled(bool enabled)
{
    _isEnabled = enabled;
}

bool EventDispatcher::isEnabled() const
{
    return _isEnabled;
}

void EventDispatcher::setDirty(std::string_view listenerId, DirtyFlag flag)
{
    const auto found = _priorityDirtyFlagMap.find(listenerId);
    if (found == _priorityDirtyFlagMap.end())
        _priorityDirtyFlagMap.emplace(listenerId, flag);
    else
        found->second = static_cast<DirtyFlag>(static_cast<int>(found->second) | static_cast<int>(flag));
}

void EventDispatcher::cleanToRemovedListeners()
{
    for (EventListener* listener : _toRemovedListeners)
    {
        const auto mapIt = _listenerMap.find(listener->getListenerID());
        if (mapIt == _listenerMap.end())
        {
            releaseListener(listener);
            continue;
        }

        auto* listeners = mapIt->second;
        auto* fixed     = listeners->getFixedPriorityListeners();
        if (fixed != nullptr)
        {
            const auto found = std::find(fixed->begin(), fixed->end(), listener);
            if (found != fixed->end())
            {
                fixed->erase(found);
                releaseListener(listener);
                if (fixed->empty())
                    listeners->clearFixedListeners();
                continue;
            }
        }
        releaseListener(listener);
    }
    _toRemovedListeners.clear();

    for (auto it = _listenerMap.begin(); it != _listenerMap.end();)
    {
        if (it->second->empty())
        {
            _priorityDirtyFlagMap.erase(it->first);
            delete it->second;
            it = _listenerMap.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void EventDispatcher::releaseListener(EventListener* listener)
{
    AX_SAFE_RELEASE(listener);
}
}  // namespace ax
