#include "SleepWaitMenuController.h"

#include <RE/C/Calendar.h>
#include <RE/M/MenuOpenCloseEvent.h>
#include <RE/S/SleepWaitMenu.h>
#include <RE/U/UI.h>
#include <RE/U/UIMessageQueue.h>
#include <Scaleform/G/GFx_Movie.h>
#include <Scaleform/G/GFx_Value.h>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <nlohmann/json.hpp>
#include <optional>
#include <string_view>

namespace
{
constexpr const char* kHoursPaths[] = {
    "root1.Menu_mc.iHours",
    "root1.iHours",
    "_root.Menu_mc.iHours",
    "_root.iHours",
    "iHours",
};
constexpr const char* kMaxHoursPaths[] = {
    "root1.Menu_mc.iMaxHours",
    "root1.iMaxHours",
    "_root.Menu_mc.iMaxHours",
    "_root.iMaxHours",
    "iMaxHours",
};
constexpr const char* kProcessUserEventPaths[] = {
    "root1.Menu_mc.ProcessUserEvent",
    "root1.ProcessUserEvent",
    "_root.Menu_mc.ProcessUserEvent",
    "_root.ProcessUserEvent",
    "ProcessUserEvent",
};

struct NumericField
{
    std::optional<int> value;
    std::string path;
};

RE::SleepWaitMenu* GetLiveSleepWaitMenu()
{
    auto* ui = RE::UI::GetSingleton();
    if (!ui) return nullptr;
    auto menu = ui->GetMenu<RE::SleepWaitMenu>();
    return menu && menu->OnStack() && menu->uiMovie ? menu.get() : nullptr;
}

std::optional<int> AsInt(const Scaleform::GFx::Value& value)
{
    if (value.IsInt()) return value.GetInt();
    if (value.IsUInt()) return static_cast<int>(value.GetUInt());
    if (value.IsNumber()) return static_cast<int>(value.GetNumber());
    return std::nullopt;
}

NumericField ReadFirstNumeric(Scaleform::GFx::Movie& movie, const char* const* paths, std::size_t count)
{
    for (std::size_t i = 0; i < count; ++i) {
        Scaleform::GFx::Value value;
        if (!movie.GetVariable(std::addressof(value), paths[i])) continue;
        if (auto parsed = AsInt(value)) return { parsed, paths[i] };
    }
    return {};
}

bool ParseInt(std::string_view text, int& value)
{
    value = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    return result.ec == std::errc{} && result.ptr == text.data() + text.size();
}

class SleepWaitMenuSink final : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
{
public:
    static SleepWaitMenuSink* GetSingleton()
    {
        static SleepWaitMenuSink sink;
        return std::addressof(sink);
    }

    RE::BSEventNotifyControl ProcessEvent(
        const RE::MenuOpenCloseEvent& event,
        RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
    {
        if (event.menuName == RE::SleepWaitMenu::MENU_NAME) {
            SleepWaitMenuController::GetSingleton()->OnRawMenuEvent(event.opening);
        }
        return RE::BSEventNotifyControl::kContinue;
    }
};
}

SleepWaitMenuController* SleepWaitMenuController::GetSingleton()
{
    static SleepWaitMenuController controller;
    return std::addressof(controller);
}

void SleepWaitMenuController::Initialize(PRISMA_UI_API::IVPrismaUI11* api)
{
    if (!api) return;
    m_api = api;
    InstallMenuSink();
}

void SleepWaitMenuController::InstallMenuSink()
{
    if (m_sinkInstalled || !m_api) return;
    auto* ui = RE::UI::GetSingleton();
    if (!ui) return;
    ui->RegisterSink(SleepWaitMenuSink::GetSingleton());
    m_sinkInstalled = true;
    logger::info("PrismaHUD: SleepWaitMenu replacement observer registered");
}

void SleepWaitMenuController::DispatchOpen(void* userdata)
{
    static_cast<SleepWaitMenuController*>(userdata)->OnMenuOpenVerified();
}

void SleepWaitMenuController::DispatchClose(void* userdata)
{
    static_cast<SleepWaitMenuController*>(userdata)->OnMenuCloseVerified();
}

void SleepWaitMenuController::OnRawMenuEvent(bool opening)
{
    m_open = opening;
    auto* api = m_api;
    if (!api) return;
    const auto callback = opening ? &SleepWaitMenuController::DispatchOpen
                                  : &SleepWaitMenuController::DispatchClose;
    if (!api->DispatchToGameThread(callback, this)) {
        logger::warn("PrismaHUD Sleep/Wait: verified {} dispatch rejected; vanilla remains authoritative",
                     opening ? "open" : "close");
    }
}

bool SleepWaitMenuController::EnsureView()
{
    if (!m_api) return false;
    if (m_view && m_api->IsValid(m_view)) return true;

    m_view = 0;
    m_domReady = false;
    m_last.clear();
    const auto view = m_api->CreateView("hud_sleepwait.html", &SleepWaitMenuController::DomReadyCallback);
    if (!view) return false;

    m_view = view;
    m_api->SetViewRole(view, PRISMA_UI_API::ViewRole::kPanel);
    m_api->SetViewOwnsEscape(view, true);
    m_api->SetOrder(view, 1'000'000);
    m_api->Hide(view);
    return true;
}

bool SleepWaitMenuController::ActivateIfNeeded()
{
    if (!m_api || !m_open || m_active || !m_domReady || !m_view || !m_api->IsValid(m_view)) return m_active;
    auto* menu = GetLiveSleepWaitMenu();
    if (!menu || !menu->uiMovie) return false;

    auto hours = ReadFirstNumeric(*menu->uiMovie, kHoursPaths, std::size(kHoursPaths));
    auto maxHours = ReadFirstNumeric(*menu->uiMovie, kMaxHoursPaths, std::size(kMaxHoursPaths));
    if (!hours.value || !maxHours.value || *maxHours.value < 1) {
        logger::warn("PrismaHUD Sleep/Wait: live iHours/iMaxHours unresolved; keeping vanilla menu");
        return false;
    }
    m_selectedHours = std::clamp(*hours.value, 1, *maxHours.value);
    m_maxHours = std::max(1, *maxHours.value);

    PushSnapshot();
    m_api->Show(m_view);
    if (!m_api->FocusOverlay(m_view, false, false)) {
        m_api->Hide(m_view);
        logger::warn("PrismaHUD Sleep/Wait: overlay focus failed; keeping vanilla menu");
        return false;
    }

    menu = GetLiveSleepWaitMenu();
    if (!menu || !m_open || !menu->uiMovie) {
        HideReplacement();
        return false;
    }

    menu->uiMovie->SetVisible(false);
    m_active = true;
    logger::info("PrismaHUD: Sleep/Wait replacement active with Bethesda time backend");
    return true;
}

void SleepWaitMenuController::RestoreVanilla()
{
    if (auto* menu = GetLiveSleepWaitMenu(); menu && menu->uiMovie) {
        menu->uiMovie->SetVisible(true);
    }
}

void SleepWaitMenuController::HideReplacement()
{
    if (m_api && m_view && m_api->IsValid(m_view)) {
        m_api->Unfocus(m_view);
        m_api->Hide(m_view);
    }
    m_active = false;
}

void SleepWaitMenuController::OnMenuOpenVerified()
{
    if (!m_api || !m_api->IsGameThread() || !GetLiveSleepWaitMenu()) return;
    m_open = true;
    m_last.clear();
    if (!EnsureView()) return;
    (void)ActivateIfNeeded();
}

void SleepWaitMenuController::OnMenuCloseVerified()
{
    if (!m_api || !m_api->IsGameThread()) return;
    m_open = false;
    m_last.clear();
    RestoreVanilla();
    HideReplacement();
}

void SleepWaitMenuController::DomReadyCallback(PrismaView view)
{
    GetSingleton()->OnDomReady(view);
}

void SleepWaitMenuController::ActionEvent(const char* argument, void* userdata)
{
    static_cast<SleepWaitMenuController*>(userdata)->OnAction(argument);
}

void SleepWaitMenuController::TickEvent(const char*, void* userdata)
{
    static_cast<SleepWaitMenuController*>(userdata)->OnTick();
}

void SleepWaitMenuController::OnDomReady(PrismaView view)
{
    if (!m_api || view != m_view || !m_api->IsValid(view)) return;
    const bool action = m_api->BindGameThreadUIEvent(
        view, "sleepWaitAction", &SleepWaitMenuController::ActionEvent, this);
    const bool tick = m_api->BindGameThreadUIEvent(
        view, "sleepWaitTick", &SleepWaitMenuController::TickEvent, this);
    if (!action || !tick) {
        logger::error("PrismaHUD Sleep/Wait: V11 event binding failed");
        m_api->Hide(view);
        return;
    }
    m_domReady = true;
    (void)ActivateIfNeeded();
}

bool SleepWaitMenuController::WriteHours(int hours)
{
    auto* menu = GetLiveSleepWaitMenu();
    if (!menu || !menu->uiMovie) return false;
    hours = std::clamp(hours, 1, std::max(1, m_maxHours));
    const Scaleform::GFx::Value value(static_cast<std::int32_t>(hours));
    bool wrote = false;
    for (const char* path : kHoursPaths) {
        wrote = menu->uiMovie->SetVariable(path, value, Scaleform::GFx::Movie::SetVarType::kNormal) || wrote;
    }
    if (wrote) m_selectedHours = hours;
    return wrote;
}

bool SleepWaitMenuController::InvokeVanillaUserEvent(const char* eventName)
{
    auto* menu = GetLiveSleepWaitMenu();
    if (!menu || !menu->uiMovie || !eventName || !eventName[0]) return false;
    const Scaleform::GFx::Value arg(eventName);
    for (const char* path : kProcessUserEventPaths) {
        if (menu->uiMovie->Invoke(path, nullptr, std::addressof(arg), 1)) {
            logger::debug("PrismaHUD Sleep/Wait: forwarded {} through {}", eventName, path);
            return true;
        }
    }
    return false;
}

std::string SleepWaitMenuController::BuildSnapshot()
{
    auto* menu = GetLiveSleepWaitMenu();
    if (!menu || !menu->uiMovie) return R"({"visible":false})";

    auto hours = ReadFirstNumeric(*menu->uiMovie, kHoursPaths, std::size(kHoursPaths));
    auto maxHours = ReadFirstNumeric(*menu->uiMovie, kMaxHoursPaths, std::size(kMaxHoursPaths));
    if (maxHours.value && *maxHours.value > 0) m_maxHours = *maxHours.value;
    if (hours.value) m_selectedHours = std::clamp(*hours.value, 1, std::max(1, m_maxHours));

    double currentHour = 0.0;
    int year = 2287;
    int month = 10;
    int day = 23;
    if (auto* calendar = RE::Calendar::GetSingleton()) {
        if (calendar->gameHour) currentHour = calendar->gameHour->value;
        if (calendar->gameYear) year = static_cast<int>(calendar->gameYear->value);
        if (calendar->gameMonth) month = static_cast<int>(calendar->gameMonth->value);
        if (calendar->gameDay) day = static_cast<int>(calendar->gameDay->value);
    }
    const double targetRaw = currentHour + static_cast<double>(m_selectedHours);
    const int dayOffset = static_cast<int>(std::floor(targetRaw / 24.0));
    double targetHour = std::fmod(targetRaw, 24.0);
    if (targetHour < 0.0) targetHour += 24.0;

    return nlohmann::json{
        { "visible", true },
        { "mode", menu->waitMode ? "wait" : "sleep" },
        { "nativeActive", menu->isActive },
        { "appPaused", menu->appPaused },
        { "sleepTimer", menu->sleepTimer },
        { "hours", m_selectedHours },
        { "maxHours", m_maxHours },
        { "hoursPath", hours.path },
        { "maxHoursPath", maxHours.path },
        { "currentHour", currentHour },
        { "targetHour", targetHour },
        { "targetDayOffset", dayOffset },
        { "date", { { "year", year }, { "month", month }, { "day", day } } },
        { "bethesdaBackend", true },
    }.dump();
}

void SleepWaitMenuController::PushSnapshot()
{
    if (!m_api || !m_domReady || !m_view || !m_api->IsValid(m_view)) return;
    const auto payload = BuildSnapshot();
    if (payload == m_last) return;
    m_last = payload;
    m_api->InteropCall(m_view, "setSleepWait", payload.c_str());
}

void SleepWaitMenuController::OnTick()
{
    if (!m_api || !m_api->IsGameThread()) return;
    if (!m_open || !GetLiveSleepWaitMenu()) {
        HideReplacement();
        return;
    }
    if (!m_active) {
        (void)ActivateIfNeeded();
        return;
    }
    PushSnapshot();
}

void SleepWaitMenuController::OnAction(const char* raw)
{
    if (!raw || !raw[0] || !m_api || !m_api->IsGameThread() || !m_open || !m_active) return;
    const std::string_view action{ raw };

    if (action == "cancel") {
        RestoreVanilla();
        HideReplacement();
        if (auto* queue = RE::UIMessageQueue::GetSingleton()) {
            queue->AddMessage(RE::BSFixedString(RE::SleepWaitMenu::MENU_NAME.data()), RE::UI_MESSAGE_TYPE::kHide);
        }
        return;
    }

    if (action == "accept") {
        if (!WriteHours(m_selectedHours) || !InvokeVanillaUserEvent("Accept")) {
            logger::error("PrismaHUD Sleep/Wait: vanilla Accept bridge failed; restoring vanilla menu");
            RestoreVanilla();
            HideReplacement();
            return;
        }
        PushSnapshot();
        return;
    }

    const auto split = action.find('|');
    if (split == std::string_view::npos) return;
    const auto verb = action.substr(0, split);
    int value = 0;
    if (!ParseInt(action.substr(split + 1), value)) return;

    int target = m_selectedHours;
    if (verb == "set") target = value;
    else if (verb == "step") target += value;
    else return;

    target = std::clamp(target, 1, std::max(1, m_maxHours));
    if (!WriteHours(target)) {
        logger::warn("PrismaHUD Sleep/Wait: failed to write iHours; restoring vanilla menu");
        RestoreVanilla();
        HideReplacement();
        return;
    }
    m_last.clear();
    PushSnapshot();
}
