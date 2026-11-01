#include "ControlLabels.h"
#include "KeyHandler.h"

#include <exception>
#include <vector>

KeyHandler* KeyHandler::GetSingleton()
{
    static KeyHandler instance;
    return &instance;
}

void KeyHandler::RegisterSink()
{
    auto* menuControls = RE::MenuControls::GetSingleton();
    if (menuControls) {
        menuControls->RegisterHandler(GetSingleton());
        logger::info("PrismaHUD KeyHandler: registered (MenuControls::RegisterHandler)");
    } else {
        logger::error("PrismaHUD KeyHandler: MenuControls null");
    }
}

KeyHandlerEvent KeyHandler::Register(uint32_t bsButtonCode, KeyEventType eventType, KeyCallback callback)
{
    std::unique_lock lock(_mutex);
    KeyHandlerEvent handle = _nextHandle++;
    _handleMap[handle] = { bsButtonCode, eventType };
    auto& cbs = _registeredCallbacks[bsButtonCode];
    if (eventType == KeyEventType::KEY_DOWN) cbs.down[handle] = std::move(callback);
    else                                     cbs.up[handle]   = std::move(callback);
    return handle;
}

void KeyHandler::Unregister(KeyHandlerEvent handle)
{
    std::unique_lock lock(_mutex);
    auto it = _handleMap.find(handle);
    if (it == _handleMap.end()) return;
    auto& cbs = _registeredCallbacks[it->second.key];
    if (it->second.type == KeyEventType::KEY_DOWN) cbs.down.erase(handle);
    else                                            cbs.up.erase(handle);
    _handleMap.erase(it);
}

bool KeyHandler::ShouldHandleEvent(const RE::InputEvent* a_event)
{
    if (!a_event) return false;

    if (a_event->eventType == RE::INPUT_EVENT_TYPE::kButton) {
        ControlLabels::NoteInputDevice(a_event->device.get());
    }
    if (a_event->eventType != RE::INPUT_EVENT_TYPE::kButton) return false;

    const auto device = a_event->device.get();
    return device == RE::INPUT_DEVICE::kKeyboard || device == RE::INPUT_DEVICE::kGamepad;
}

void KeyHandler::OnButtonEvent(const RE::ButtonEvent* a_event)
{
    if (!a_event) return;
    const uint32_t key = static_cast<uint32_t>(a_event->GetBSButtonCode());

    const bool pressed = a_event->QJustPressed();
    const bool released = !pressed && a_event->QAnalogValue() == 0.0f;
    if (!pressed && !released) return;

    std::vector<KeyCallback> callbacks;
    {
        std::shared_lock lock(_mutex);
        auto it = _registeredCallbacks.find(key);
        if (it == _registeredCallbacks.end()) return;
        const auto& selected = pressed ? it->second.down : it->second.up;
        callbacks.reserve(selected.size());
        for (const auto& [handle, cb] : selected) {
            (void)handle;
            if (cb) callbacks.push_back(cb);
        }
    }

    bool consume = false;
    for (auto& cb : callbacks) {
        try {
            consume |= cb();
        } catch (const std::exception& e) {
            logger::error("PrismaHUD KeyHandler callback threw: {}", e.what());
        } catch (...) {
            logger::error("PrismaHUD KeyHandler callback threw an unknown exception");
        }
    }

    if (consume)
        const_cast<RE::ButtonEvent*>(a_event)->handled = RE::InputEvent::HANDLED_RESULT::kStop;
}
