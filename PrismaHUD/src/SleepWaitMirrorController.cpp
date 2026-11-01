#include "SleepWaitMirrorController.h"

#include <RE/M/MenuOpenCloseEvent.h>
#include <RE/S/SleepWaitMenu.h>
#include <RE/U/UI.h>
#include <Scaleform/G/GFx_Movie.h>
#include <Scaleform/G/GFx_Value.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <optional>
#include <string_view>

namespace
{
    struct NumericField
    {
        std::optional<int> value;
        std::string path;
    };

    RE::SleepWaitMenu* GetLiveMenu()
    {
        auto* ui = RE::UI::GetSingleton();
        if (!ui) return nullptr;
        auto menu = ui->GetMenu<RE::SleepWaitMenu>();
        return menu && menu->OnStack() ? menu.get() : nullptr;
    }

    std::optional<int> AsInt(const Scaleform::GFx::Value& value)
    {
        if (value.IsInt()) return value.GetInt();
        if (value.IsUInt()) return static_cast<int>(value.GetUInt());
        if (value.IsNumber()) return static_cast<int>(value.GetNumber());
        return std::nullopt;
    }

    NumericField ReadFirstNumeric(Scaleform::GFx::Movie& movie, std::initializer_list<const char*> paths)
    {
        for (const char* path : paths) {
            Scaleform::GFx::Value value;
            if (!movie.GetVariable(std::addressof(value), path)) continue;
            if (auto parsed = AsInt(value)) return { parsed, path };
        }
        return {};
    }

    bool InterestingName(std::string_view name)
    {
        std::string lower(name);
        std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return lower.find("hour") != std::string::npos ||
               lower.find("sleep") != std::string::npos ||
               lower.find("wait") != std::string::npos ||
               lower.find("time") != std::string::npos;
    }

    void DumpInteresting(const char* name, const Scaleform::GFx::Value& value, int depth)
    {
        if (!name || depth > 2) return;
        if (InterestingName(name)) {
            if (auto number = AsInt(value)) {
                logger::info("[SleepWait SWF] {}{} = {}", std::string(depth * 2, ' '), name, *number);
            } else if (value.IsBoolean()) {
                logger::info("[SleepWait SWF] {}{} = {}", std::string(depth * 2, ' '), name, value.GetBoolean());
            } else if (value.IsString()) {
                logger::info("[SleepWait SWF] {}{} = '{}'", std::string(depth * 2, ' '), name,
                             value.GetString() ? value.GetString() : "");
            } else {
                logger::info("[SleepWait SWF] {}{} (object/other)", std::string(depth * 2, ' '), name);
            }
        }

        if (depth >= 2 || (!value.IsObject() && !value.IsDisplayObject())) return;
        try {
            value.VisitMembers([depth](const char* childName, const Scaleform::GFx::Value& child) {
                DumpInteresting(childName, child, depth + 1);
            });
        } catch (...) {}
    }

    void OpenDeferred(void*)
    {
        SleepWaitMirrorController::GetSingleton()->OnMenuOpenVerified();
    }

    void CloseDeferred(void*)
    {
        SleepWaitMirrorController::GetSingleton()->OnMenuCloseVerified();
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
            if (event.menuName != RE::SleepWaitMenu::MENU_NAME) {
                return RE::BSEventNotifyControl::kContinue;
            }

            auto* api = PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI11>();
            if (!api) return RE::BSEventNotifyControl::kContinue;
            auto callback = event.opening ? OpenDeferred : CloseDeferred;
            if (!api->DispatchToGameThread(callback, nullptr)) {
                logger::warn("SleepWaitMirror: verified {} dispatch failed; vanilla remains untouched",
                             event.opening ? "open" : "close");
            }
            return RE::BSEventNotifyControl::kContinue;
        }
    };
}

SleepWaitMirrorController* SleepWaitMirrorController::GetSingleton()
{
    static SleepWaitMirrorController singleton;
    return std::addressof(singleton);
}

void SleepWaitMirrorController::SetView(PrismaView view) noexcept
{
    if (m_view == view) return;
    m_view = view;
    m_domReady = false;
    m_last.clear();
}

void SleepWaitMirrorController::OnDomReady(PrismaView view)
{
    if (!m_api || !m_api->IsGameThread() || view != m_view || !m_api->IsValid(view)) return;

    m_domReady = true;
    m_last.clear();
    if (!m_open) return;

    m_api->Show(m_view);
    OnRequestUpdate();
}

void SleepWaitMirrorController::InstallMenuSink()
{
    static bool installed = false;
    if (installed || !m_api) return;

    auto* ui = RE::UI::GetSingleton();
    if (!ui) {
        logger::warn("SleepWaitMirror: UI unavailable; mirror sink not installed");
        return;
    }
    ui->RegisterSink(SleepWaitMenuSink::GetSingleton());
    installed = true;
    logger::info("SleepWaitMirror: read-only SleepWaitMenu mirror registered; vanilla remains authoritative");
}

void SleepWaitMirrorController::ResetForLoad()
{
    m_open = false;
    m_last.clear();
    HideMirror();
}

void SleepWaitMirrorController::HideMirror()
{
    if (!m_api || !m_view || !m_api->IsValid(m_view)) return;
    if (m_domReady) {
        m_api->InteropCall(m_view, "updateSleepWaitMirror", R"({"visible":false})");
    }
    m_api->Hide(m_view);
}

void SleepWaitMirrorController::OnMenuOpenVerified()
{
    if (!m_api || !m_api->IsGameThread()) {
        logger::error("SleepWaitMirror: open reached unverified thread; ignoring");
        return;
    }
    if (!GetLiveMenu()) return;

    m_open = true;
    m_last.clear();
    if (!m_dumped) {
        m_dumped = true;
        DumpLiveMovieOnce();
    }
    if (!m_domReady) return;
    if (m_view && m_api->IsValid(m_view)) m_api->Show(m_view);
    OnRequestUpdate();
}

void SleepWaitMirrorController::OnMenuCloseVerified()
{
    if (!m_api || !m_api->IsGameThread()) return;
    m_open = false;
    m_last.clear();
    HideMirror();
}

std::string SleepWaitMirrorController::BuildSnapshot()
{
    auto* menu = GetLiveMenu();
    if (!menu || !menu->uiMovie) return R"({"visible":false})";

    auto hours = ReadFirstNumeric(*menu->uiMovie, {
        "root1.Menu_mc.iHours", "root1.iHours", "_root.Menu_mc.iHours", "_root.iHours", "iHours"
    });
    auto maxHours = ReadFirstNumeric(*menu->uiMovie, {
        "root1.Menu_mc.iMaxHours", "root1.iMaxHours", "_root.Menu_mc.iMaxHours", "_root.iMaxHours", "iMaxHours"
    });

    nlohmann::json payload{
        { "visible", true },
        { "mode", menu->waitMode ? "wait" : "sleep" },
        { "nativeActive", menu->isActive },
        { "appPaused", menu->appPaused },
        { "hours", hours.value ? nlohmann::json(*hours.value) : nlohmann::json(nullptr) },
        { "maxHours", maxHours.value ? nlohmann::json(*maxHours.value) : nlohmann::json(nullptr) },
        { "hoursPath", hours.path },
        { "maxHoursPath", maxHours.path },
        { "readOnly", true },
    };
    return payload.dump();
}

void SleepWaitMirrorController::OnRequestUpdate()
{
    if (!m_api || !m_api->IsGameThread() || !m_domReady || !m_open || !m_view || !m_api->IsValid(m_view)) return;

    const auto payload = BuildSnapshot();
    if (payload == m_last) return;
    m_last = payload;
    m_api->InteropCall(m_view, "updateSleepWaitMirror", payload.c_str());
}

void SleepWaitMirrorController::DumpLiveMovieOnce()
{
    auto* menu = GetLiveMenu();
    if (!menu || !menu->uiMovie) {
        logger::warn("[SleepWait SWF] live menu/movie unavailable for probe");
        return;
    }

    logger::info("[SleepWait SWF] one-shot probe begins (interesting names only, depth <= 2)");
    for (const char* path : { "root1.Menu_mc", "root1", "_root.Menu_mc", "_root" }) {
        Scaleform::GFx::Value root;
        if (!menu->uiMovie->GetVariable(std::addressof(root), path)) continue;
        logger::info("[SleepWait SWF] probing root '{}'", path);
        DumpInteresting(path, root, 0);
    }

    const auto snapshot = BuildSnapshot();
    logger::info("[SleepWait SWF] initial mirror snapshot: {}", snapshot);
    logger::info("[SleepWait SWF] probe ends; vanilla menu was not modified");
}
