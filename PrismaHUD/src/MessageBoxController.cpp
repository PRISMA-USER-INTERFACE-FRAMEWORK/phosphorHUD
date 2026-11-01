#include "MessageBoxController.h"
#include "ControlLabels.h"

#include <RE/I/IMessageBoxCallback.h>
#include <RE/M/MessageBoxData.h>
#include <RE/M/MessageBoxMenu.h>

#include <cstdlib>
#include <string>

namespace {
std::string ResolveLocalized(const char* raw)
{
    if (!raw || !*raw) return {};
    if (raw[0] != '$') return raw;

    auto* sm = RE::BSScaleformManager::GetSingleton();
    auto* tr = sm ? sm->GetTranslator() : nullptr;
    if (!tr) return raw;

    const auto lookup = [&](const char* key) -> std::string {
        const int wlen = ::MultiByteToWideChar(CP_UTF8, 0, key, -1, nullptr, 0);
        if (wlen <= 1) return {};
        std::wstring wkey(static_cast<std::size_t>(wlen - 1), L' ');
        ::MultiByteToWideChar(CP_UTF8, 0, key, -1, wkey.data(), wlen);

        const RE::BSFixedStringWCS wk{ wkey.c_str() };
        const auto it = tr->translator.translationMap.find(wk);
        if (it == tr->translator.translationMap.end()) return {};

        const wchar_t* val = it->second.c_str();
        if (!val || !*val) return {};
        const int blen = ::WideCharToMultiByte(CP_UTF8, 0, val, -1, nullptr, 0, nullptr, nullptr);
        if (blen <= 1) return {};
        std::string out(static_cast<std::size_t>(blen - 1), ' ');
        ::WideCharToMultiByte(CP_UTF8, 0, val, -1, out.data(), blen, nullptr, nullptr);
        return out;
    };

    try {
        if (auto hit = lookup(raw); !hit.empty()) return hit;
        if (auto hit = lookup(raw + 1); !hit.empty()) return hit;
    } catch (...) {}

    return raw;
}

RE::MessageBoxMenu* GetLiveMenu()
{
    auto* ui = RE::UI::GetSingleton();
    if (!ui) return nullptr;
    auto menu = ui->GetMenu(RE::MessageBoxMenu::MENU_NAME);
    if (!menu || !menu->OnStack()) return nullptr;
    return static_cast<RE::MessageBoxMenu*>(menu.get());
}

void SetVanillaVisible(bool visible)
{
    auto* ui = RE::UI::GetSingleton();
    if (!ui) return;
    auto menu = ui->GetMenu(RE::MessageBoxMenu::MENU_NAME);
    if (menu && menu->uiMovie) menu->uiMovie->SetVisible(visible);
}

class MessageBoxSink : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
{
public:
    RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent& event,
                                          RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
    {
        if (event.menuName != RE::MessageBoxMenu::MENU_NAME)
            return RE::BSEventNotifyControl::kContinue;

        auto* ctl = MessageBoxController::GetSingleton();
        if (event.opening) ctl->OnMessageBoxOpen();
        else ctl->OnMessageBoxClose();
        return RE::BSEventNotifyControl::kContinue;
    }

    static MessageBoxSink* GetSingleton()
    {
        static MessageBoxSink sink;
        return &sink;
    }
};

constexpr const char* kReadyPrefix = "__PRISMA_MESSAGEBOX_READY__:";
}

MessageBoxController* MessageBoxController::GetSingleton()
{
    static MessageBoxController instance;
    return &instance;
}

void MessageBoxController::SetView(PrismaView view)
{
    if (m_view.load() == view) return;
    m_view.store(view);
    m_domReady.store(false);
    m_domProbeInFlight.store(false);
    m_bindingsReady.store(false);
    m_active.store(false);
    m_answered.store(false);
}

void MessageBoxController::InstallMenuSink()
{
    static bool registered = false;
    if (registered) return;

    auto* ui = RE::UI::GetSingleton();
    if (!ui) {
        logger::warn("PrismaHUD: RE::UI unavailable -- message-box sink not installed");
        return;
    }

    ui->RegisterSink(MessageBoxSink::GetSingleton());
    registered = true;
    logger::info("PrismaHUD: message-box sink registered");
}

void MessageBoxController::DispatchOpen(void* userdata)
{
    static_cast<MessageBoxController*>(userdata)->OpenOnGameThread();
}

void MessageBoxController::DispatchClose(void* userdata)
{
    static_cast<MessageBoxController*>(userdata)->CloseOnGameThread();
}

void MessageBoxController::DispatchRetryOpen(void* userdata)
{
    static_cast<MessageBoxController*>(userdata)->OpenOnGameThread();
}

void MessageBoxController::OnMessageBoxOpen()
{
    m_openRequested.store(true);
    m_answered.store(false);

    auto* api = m_api;
    if (!api || !api->DispatchToGameThread(&MessageBoxController::DispatchOpen, this)) {
        logger::warn("PrismaHUD: message-box open dispatch rejected -- leaving vanilla visible");
    }
}

void MessageBoxController::OnMessageBoxClose()
{
    m_openRequested.store(false);

    auto* api = m_api;
    if (!api || !api->DispatchToGameThread(&MessageBoxController::DispatchClose, this)) {
        logger::warn("PrismaHUD: message-box close dispatch rejected");
    }
}

void MessageBoxController::ProbeDomReady()
{
    auto* api = m_api;
    const auto view = m_view.load();
    if (!api || !view || !api->IsValid(view)) return;
    if (m_domProbeInFlight.exchange(true)) return;

    const std::string script =
        "(typeof window.showMessageBox==='function' && typeof window.hideMessageBox==='function' && "
        "typeof window.mbAnswer==='function' && typeof window.mbTick==='function') ? "
        "'__PRISMA_MESSAGEBOX_READY__:" + std::to_string(view) + "' : "
        "'__PRISMA_MESSAGEBOX_NOT_READY__:" + std::to_string(view) + "'";

    api->Invoke(view, script.c_str(), &MessageBoxController::DomReadyProbeResult);
}

void MessageBoxController::DomReadyProbeResult(const char* result)
{
    auto* ctl = GetSingleton();
    ctl->m_domProbeInFlight.store(false);
    if (!result) return;

    const std::string text{ result };
    const auto pos = text.find(kReadyPrefix);
    if (pos == std::string::npos) return;

    const char* digits = text.c_str() + pos + std::char_traits<char>::length(kReadyPrefix);
    char* end = nullptr;
    const auto readyView = static_cast<PrismaView>(std::strtoull(digits, &end, 10));
    if (!readyView || readyView != ctl->m_view.load()) return;

    ctl->m_domReady.store(true);
    if (!ctl->m_openRequested.load()) return;

    auto* api = ctl->m_api;
    if (!api || !api->DispatchToGameThread(&MessageBoxController::DispatchRetryOpen, ctl)) {
        logger::warn("PrismaHUD: message-box DOM-ready retry dispatch rejected -- using vanilla");
    }
}

void MessageBoxController::MessageBoxTickEvent(const char*, void* userdata)
{
    static_cast<MessageBoxController*>(userdata)->OnTick();
}

void MessageBoxController::MessageBoxAnswerEvent(const char* argument, void* userdata)
{
    auto* ctl = static_cast<MessageBoxController*>(userdata);
    if (!argument) return;

    try {
        const auto json = nlohmann::json::parse(argument);
        ctl->Answer(json.at("index").get<std::uint32_t>());
    } catch (const std::exception& e) {
        logger::error("PrismaHUD: bad mbAnswer payload: {}", e.what());
    }
}

bool MessageBoxController::BindVerifiedEvents()
{
    auto* api = m_api;
    const auto view = m_view.load();
    if (!api || !view || !api->IsValid(view)) {
        m_bindingsReady.store(false);
        return false;
    }

    const bool tick = api->BindGameThreadUIEvent(
        view, "mbTick", &MessageBoxController::MessageBoxTickEvent, this);
    const bool answer = api->BindGameThreadUIEvent(
        view, "mbAnswer", &MessageBoxController::MessageBoxAnswerEvent, this);
    const bool ready = tick && answer;
    m_bindingsReady.store(ready);
    return ready;
}

void MessageBoxController::OpenOnGameThread()
{
    if (!m_openRequested.load()) return;

    m_active.store(false);
    m_answered.store(false);

    SetVanillaVisible(true);

    auto* api = m_api;
    const auto view = m_view.load();
    if (!api || !view || !api->IsValid(view)) {
        logger::warn("PrismaHUD: message-box view unavailable -- using vanilla");
        return;
    }

    if (!m_domReady.load()) {
        ProbeDomReady();
        logger::debug("PrismaHUD: message-box DOM not proven ready -- using vanilla for now");
        return;
    }

    if (!BindVerifiedEvents()) {
        logger::warn("PrismaHUD: V11 message-box bindings unavailable -- using vanilla");
        return;
    }

    auto* menu = GetLiveMenu();
    if (!menu || !menu->currentMessage) {
        logger::warn("PrismaHUD: message box has no readable MessageBoxData -- using vanilla");
        return;
    }

    const auto* data = menu->currentMessage;
    nlohmann::json buttons = nlohmann::json::array();
    for (const auto& button : data->buttonText) {
        buttons.push_back(button.empty() ? std::string{} : ResolveLocalized(button.c_str()));
    }
    if (buttons.empty()) {
        logger::warn("PrismaHUD: message box has zero buttons -- using vanilla");
        return;
    }

    const std::string header = data->headerText.empty() ? std::string{} : ResolveLocalized(data->headerText.c_str());
    const std::string body = data->bodyText.empty() ? std::string{} : ResolveLocalized(data->bodyText.c_str());

    const bool pad = ControlLabels::UsingGamepad();
    const bool ps = ControlLabels::GetStyle() == ControlLabels::Style::kPlayStation;
    const auto payload = nlohmann::json{
        { "header", header },
        { "body", body },
        { "buttons", buttons },
        { "controllerStyle", ps ? "playstation" : "xbox" },
        { "usingGamepad", pad },
        { "navHint", pad ? "gp:DPad" : "\xE2\x87\x85" },
        { "confirmHint", pad ? "gp:A" : "Enter" },
        { "cancelHint", pad ? "gp:B" : "Esc" },
    }.dump();

    api->InteropCall(view, "showMessageBox", payload.c_str());
    api->Show(view);

    if (!api->Focus(view, false, false)) {
        api->Hide(view);
        SetVanillaVisible(true);
        logger::error("PrismaHUD: message-box Focus failed -- rolled back to vanilla");
        return;
    }

    if (!m_openRequested.load()) {
        api->Unfocus(view);
        api->Hide(view);
        SetVanillaVisible(true);
        return;
    }

    SetVanillaVisible(false);
    m_active.store(true);
    logger::info("PrismaHUD: took over message box ('{}', {} buttons)", header, buttons.size());
}

void MessageBoxController::CloseOnGameThread()
{
    SetVanillaVisible(true);

    auto* api = m_api;
    const auto view = m_view.load();
    if (api && view && api->IsValid(view)) {
        api->InteropCall(view, "hideMessageBox", "");
        api->Unfocus(view);
        api->Hide(view);
    }

    m_active.store(false);
    m_answered.store(false);
}

void MessageBoxController::OnTick()
{
    if (!m_openRequested.load()) {
        CloseOnGameThread();
        return;
    }
    if (!m_active.load() || m_answered.load()) return;

    auto* ui = RE::UI::GetSingleton();
    if (!ui) return;
    auto menu = ui->GetMenu(RE::MessageBoxMenu::MENU_NAME);
    if (!menu || !menu->uiMovie) return;

    if (menu->uiMovie->GetVisible()) {
        menu->uiMovie->SetVisible(false);
    }
}

void MessageBoxController::Answer(std::uint32_t index)
{
    if (!m_active.load()) return;
    if (m_answered.exchange(true)) {
        logger::warn("PrismaHUD: message box already answered -- ignoring button {}", index);
        return;
    }

    auto* menu = GetLiveMenu();
    if (!menu || !menu->currentMessage) {
        logger::error("PrismaHUD: message box vanished before answer -- restoring vanilla");
        m_openRequested.store(false);
        CloseOnGameThread();
        return;
    }

    auto* data = menu->currentMessage;
    if (index >= data->buttonText.size()) {
        logger::error("PrismaHUD: message-box button {} out of range ({})", index, data->buttonText.size());
        m_answered.store(false);
        return;
    }

    auto callback = data->callback;
    m_openRequested.store(false);

    auto* api = m_api;
    const auto view = m_view.load();
    if (api && view && api->IsValid(view)) {
        api->Unfocus(view);
        api->Hide(view);
    }
    SetVanillaVisible(true);
    m_active.store(false);

    if (auto* queue = RE::UIMessageQueue::GetSingleton()) {
        queue->AddMessage(RE::MessageBoxMenu::MENU_NAME, RE::UI_MESSAGE_TYPE::kHide);
    }

    if (callback) {
        (*callback)(static_cast<std::uint8_t>(index));
    } else {
        logger::info("PrismaHUD: message box had no callback -- closed");
    }
}
