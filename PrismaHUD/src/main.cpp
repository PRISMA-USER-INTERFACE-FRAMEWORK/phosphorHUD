#include "PrismaUI_F4_API.h"
#include "PrismaUI_F4_API_V11.h"
#include "PrismaHUD_F4_API.h"
#include "HUDLayoutManager.h"
#include "HUDDataProvider.h"
#include "QuickLootController.h"
#include "MessageBoxController.h"
#include "SpecialMirrorController.h"
#include "SleepWaitMirrorController.h"
#include "KeyHandler.h"
#include "BindingRegistry.h"
#include "ActivateChoiceRegistry.h"
#include "ControlLabels.h"

#include <RE/U/UI.h>
#include <RE/M/MenuOpenCloseEvent.h>

#include <ShlObj.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <unordered_set>

static constexpr uint32_t kEditorToggleKey = 0x7A;

static PrismaView              g_editorView = 0;
static std::atomic<bool>       g_editorOpen = false;
static PrismaView              g_quickLootView = 0;
static KeyHandlerEvent         g_hideKeyReg = 0;

static PRISMA_UI_API::IVPrismaUI11* g_api = nullptr;

static void RegisterHideKey()
{
    auto* kh = KeyHandler::GetSingleton();
    if (g_hideKeyReg) { kh->Unregister(g_hideKeyReg); g_hideKeyReg = 0; }
    g_hideKeyReg = kh->Register(HUDLayoutManager::GetSingleton()->GetHideKey(),
                                KeyEventType::KEY_DOWN, []() -> bool {
        HUDLayoutManager::GetSingleton()->ToggleMasterHidden();
        return false;
    });
}

static void CbToggleRollover(void*) { QuickLootController::GetSingleton()->ToggleUserHidden(); }

static void ProbeControlMapEncoding()
{
    auto* cm = RE::ControlMap::GetSingleton();
    if (!cm) { logger::warn("[ControlMap probe] ControlMap singleton null"); return; }
    const char* events[] = { "Forward", "Back", "Left", "Right", "Activate", "Jump",
                             "Ready Weapon", "Reload", "Run", "Sneak", "Sprint", "Toggle POV" };
    logger::info("[ControlMap probe] keyboard mappings (inputKey vs its VK-name reading):");
    for (auto* ev : events) {
        uint32_t k = cm->GetMappedKey(ev, RE::INPUT_DEVICE::kKeyboard);
        logger::info("[ControlMap probe]   '{}' -> inputKey=0x{:X}  (as VK: {})",
                     ev, k, BindingRegistry::KeyName(k));
    }
}

extern "C" DLLEXPORT void* RequestPrismaHUDAPI(PRISMA_HUD_API::InterfaceVersion ver)
{
    switch (ver) {
    case PRISMA_HUD_API::InterfaceVersion::V3:
        return static_cast<PRISMA_HUD_API::IVPrismaHUD3*>(ActivateChoiceRegistry::GetSingleton());
    case PRISMA_HUD_API::InterfaceVersion::V2:
        return static_cast<PRISMA_HUD_API::IVPrismaHUD2*>(BindingRegistry::GetSingleton());
    case PRISMA_HUD_API::InterfaceVersion::V1:
        return static_cast<PRISMA_HUD_API::IVPrismaHUD1*>(HUDLayoutManager::GetSingleton());
    default:
        logger::warn("PrismaHUD: unsupported API version {} requested", static_cast<unsigned>(ver));
        return nullptr;
    }
}

static void LogJS(PrismaView, PRISMA_UI_API::ConsoleMessageLevel lvl, const char* msg)
{
    const char* tag = (lvl == PRISMA_UI_API::ConsoleMessageLevel::Error)  ? "[JS ERR]" :
                      (lvl == PRISMA_UI_API::ConsoleMessageLevel::Warning) ? "[JS WRN]" :
                                                                              "[JS LOG]";
    logger::info("{} {}", tag, msg);
}

static void OpenEditor()
{
    if (!g_api || !g_api->IsValid(g_editorView)) return;

    auto* lm = HUDLayoutManager::GetSingleton();
    lm->ShowAllWidgets();
    lm->SetEditorPreview(true);

    auto payload = lm->BuildEditorPayload();
    g_api->InteropCall(g_editorView, "initWidgets", payload.c_str());

    auto slots = lm->GetSlotsPayload();
    g_api->InteropCall(g_editorView, "initSlots", slots.c_str());

    auto imports = lm->ListImportFiles();
    g_api->InteropCall(g_editorView, "initImports", imports.c_str());

    auto presets = lm->GetPresetsPayload();
    g_api->InteropCall(g_editorView, "initPresets", presets.c_str());

    auto icons = lm->GetIconsPayload();
    g_api->InteropCall(g_editorView, "initIcons", icons.c_str());

    auto settings = nlohmann::json{
        {"hideKey",   lm->GetHideKey()},
        {"combatOnly", lm->GetCombatOnly()},
        {"manualHidden", lm->GetManualHidden()},
    }.dump();
    g_api->InteropCall(g_editorView, "initSettings", settings.c_str());

    g_api->Show(g_editorView);
    if (!g_api->Focus(g_editorView, false, false))
    {
        g_api->Hide(g_editorView);
        lm->SetEditorPreview(false);
        lm->RestoreWidgetVisibility();
        g_editorOpen = false;
        logger::error("PrismaHUD: editor Focus failed; rolled back closed");
        return;
    }

    g_editorOpen = true;
    logger::info("PrismaHUD: editor opened");
}

static void CloseEditor()
{
    if (g_api && g_editorView && g_api->IsValid(g_editorView))
    {
        g_api->Unfocus(g_editorView);
        g_api->Hide(g_editorView);
    }

    auto* lm = HUDLayoutManager::GetSingleton();
    lm->SetEditorPreview(false);
    lm->RestoreWidgetVisibility();
    g_editorOpen = false;
    logger::info("PrismaHUD: editor closed");
}

static void OnEditorDomReady(PrismaView )
{
    if (!g_api || !g_editorView) return;

    auto* lm = HUDLayoutManager::GetSingleton();

    if (lm->EditorListenersRegistered()) return;
    lm->SetEditorListenersRegistered();

    g_api->BindUIEvent(g_editorView, "moveWidget", [](const char* json) {
        HUDLayoutManager::GetSingleton()->LiveMove(json);
    });
    g_api->BindUIEvent(g_editorView, "saveLayout", [](const char* json) {
        HUDLayoutManager::GetSingleton()->SaveFromJS(json);
    });
    g_api->BindUIEvent(g_editorView, "closeEditor", [](const char*) {
        CloseEditor();
    });
    g_api->BindUIEvent(g_editorView, "setWidgetVisible", [](const char* json) {
        try {
            auto j = nlohmann::json::parse(json);
            auto id  = j.at("id").get<std::string>();
            bool vis = j.at("visible").get<bool>();
            HUDLayoutManager::GetSingleton()->SetWidgetVisible(id.c_str(), vis);
        } catch (...) {}
    });
    g_api->BindUIEvent(g_editorView, "loadSlot", [](const char* json) {
        try {
            auto j  = nlohmann::json::parse(json);
            int idx = j.at("index").get<int>();
            HUDLayoutManager::GetSingleton()->LoadSlot(idx);
        } catch (...) {}
    });
    g_api->BindUIEvent(g_editorView, "saveSlot", [](const char* json) {
        try {
            auto j    = nlohmann::json::parse(json);
            int  idx  = j.at("index").get<int>();
            auto name = j.at("name").get<std::string>();
            HUDLayoutManager::GetSingleton()->SaveSlot(idx, name.c_str());
        } catch (...) {}
    });
    g_api->BindUIEvent(g_editorView, "renameSlot", [](const char* json) {
        try {
            auto j    = nlohmann::json::parse(json);
            int  idx  = j.at("index").get<int>();
            auto name = j.at("name").get<std::string>();
            HUDLayoutManager::GetSingleton()->RenameSlot(idx, name.c_str());
        } catch (...) {}
    });
    g_api->BindUIEvent(g_editorView, "exportLayout", [](const char* json) {
        try {
            auto j    = nlohmann::json::parse(json);
            auto name = j.at("name").get<std::string>();
            auto* lm  = HUDLayoutManager::GetSingleton();
            lm->ExportLayout(name.c_str());
            auto files = lm->ListImportFiles();
            if (g_api && g_editorView) g_api->InteropCall(g_editorView, "initImports", files.c_str());
        } catch (...) {}
    });
    g_api->BindUIEvent(g_editorView, "importLayout", [](const char* json) {
        try {
            auto j    = nlohmann::json::parse(json);
            auto name = j.at("name").get<std::string>();
            HUDLayoutManager::GetSingleton()->ImportLayout(name.c_str());
        } catch (...) {}
    });
    g_api->BindUIEvent(g_editorView, "setWidgetIcon", [](const char* json) {
        try {
            auto j    = nlohmann::json::parse(json);
            auto id   = j.at("id").get<std::string>();
            auto file = j.at("icon").get<std::string>();
            HUDLayoutManager::GetSingleton()->SetWidgetIcon(id.c_str(), file.c_str());
        } catch (...) {}
    });
    g_api->BindUIEvent(g_editorView, "applyPreset", [](const char* json) {
        try {
            auto j    = nlohmann::json::parse(json);
            auto name = j.at("name").get<std::string>();
            auto* lm  = HUDLayoutManager::GetSingleton();
            lm->ApplyPreset(name.c_str());
            auto payload = lm->BuildEditorPayload();
            if (g_api && g_editorView) g_api->InteropCall(g_editorView, "initWidgets", payload.c_str());
        } catch (...) {}
    });
    g_api->BindUIEvent(g_editorView, "requestWidgets", [](const char*) {
        auto payload = HUDLayoutManager::GetSingleton()->BuildEditorPayload();
        if (g_api && g_editorView) g_api->InteropCall(g_editorView, "initWidgets", payload.c_str());
    });
    g_api->BindUIEvent(g_editorView, "listImports", [](const char*) {
        auto files = HUDLayoutManager::GetSingleton()->ListImportFiles();
        if (g_api && g_editorView) g_api->InteropCall(g_editorView, "initImports", files.c_str());
    });
    g_api->BindUIEvent(g_editorView, "setHideKey", [](const char* json) {
        try {
            auto j = nlohmann::json::parse(json);
            HUDLayoutManager::GetSingleton()->SetHideKey(j.at("vk").get<std::uint32_t>());
            RegisterHideKey();
        } catch (...) {}
    });
    g_api->BindUIEvent(g_editorView, "setCombatOnly", [](const char* json) {
        try {
            auto j = nlohmann::json::parse(json);
            HUDLayoutManager::GetSingleton()->SetCombatOnly(j.at("on").get<bool>());
        } catch (...) {}
    });

    g_api->RegisterConsoleCallback(g_editorView, LogJS);
    logger::info("PrismaHUD: editor DOM ready");
}

static PrismaView        g_keybindsView = 0;
static std::atomic<bool> g_keybindsOpen = false;

static PrismaView g_messageBoxView      = 0;
static PrismaView g_specialView         = 0;
static PrismaView g_sleepWaitMirrorView = 0;

static void OnSpecialDomReady(PrismaView v)
{
    if (!g_api) return;
    g_api->BindUIEvent(v, "requestUpdate", [](const char*) {
        SpecialMirrorController::GetSingleton()->OnRequestUpdate();
    });
}

static void OnSleepWaitMirrorDomReady(PrismaView v)
{
    if (!g_api) return;
    g_api->SetViewRole(v, PRISMA_UI_API::ViewRole::kWidget);
    if (!g_api->BindGameThreadUIEvent(v, "requestSleepWaitMirrorUpdate", [](const char*, void*) {
            SleepWaitMirrorController::GetSingleton()->OnRequestUpdate();
        }, nullptr)) {
        logger::error("SleepWaitMirror: verified requestUpdate binding failed; mirror remains blank");
        return;
    }
    SleepWaitMirrorController::GetSingleton()->OnDomReady(v);
}

static void OnMessageBoxDomReady(PrismaView v)
{
    if (!g_api) return;
    g_api->BindUIEvent(v, "mbTick", [](const char*) {
        MessageBoxController::GetSingleton()->OnTick();
    });
    g_api->BindUIEvent(v, "mbAnswer", [](const char* json) {
        try {
            auto j = nlohmann::json::parse(json);
            MessageBoxController::GetSingleton()->Answer(j.at("index").get<std::uint32_t>());
        } catch (const std::exception& e) {
            logger::error("PrismaHUD: bad mbAnswer payload: {}", e.what());
        }
    });
}

static void PushKeybindList()
{
    if (!g_api || !g_api->IsValid(g_keybindsView)) return;
    g_api->InteropCall(g_keybindsView, "initKeybinds",
                       BindingRegistry::GetSingleton()->BuildListJSON().c_str());
}

static void OpenKeybinds()
{
    if (!g_api || !g_api->IsValid(g_keybindsView)) return;
    PushKeybindList();
    g_api->InteropCall(g_keybindsView, "setVisible", "1");
    g_api->Show(g_keybindsView);
    g_api->Focus(g_keybindsView, false, false);
    g_keybindsOpen = true;
    logger::info("PrismaHUD: keybind menu opened");
}

static void CloseKeybinds()
{
    if (!g_api || !g_api->IsValid(g_keybindsView)) return;
    BindingRegistry::GetSingleton()->SetCaptureMode(false);
    g_api->InteropCall(g_keybindsView, "setVisible", "0");
    g_api->Unfocus(g_keybindsView);
    g_api->Hide(g_keybindsView);
    g_keybindsOpen = false;
    logger::info("PrismaHUD: keybind menu closed");
}

static void OnKeybindsDomReady(PrismaView v)
{
    if (!g_api) return;
    g_api->BindUIEvent(v, "rebindBegin",  [](const char*) { BindingRegistry::GetSingleton()->SetCaptureMode(true); });
    g_api->BindUIEvent(v, "rebindCancel", [](const char*) { BindingRegistry::GetSingleton()->SetCaptureMode(false); });
    g_api->BindUIEvent(v, "rebindApply", [](const char* json) {
        try {
            auto     j     = nlohmann::json::parse(json);
            auto     id    = j.at("id").get<std::string>();
            uint32_t vk    = j.at("vk").get<uint32_t>();
            bool     force = j.value("force", false);
            auto*    reg   = BindingRegistry::GetSingleton();
            reg->SetCaptureMode(false);
            if (!force) {
                auto conflict = reg->CheckRebindConflict(id.c_str(), vk);
                if (!conflict.empty()) {
                    auto payload = nlohmann::json{ {"id", id}, {"vk", vk},
                        {"keyName", BindingRegistry::KeyName(vk)}, {"conflict", conflict} }.dump();
                    g_api->InteropCall(g_keybindsView, "rebindConflict", payload.c_str());
                    return;
                }
            }
            reg->SetKey(id.c_str(), vk);
            PushKeybindList();
        } catch (...) {}
    });
    g_api->BindUIEvent(v, "resetKeybinds", [](const char* json) {
        std::string id;
        try { auto j = nlohmann::json::parse(json); if (j.is_string()) id = j.get<std::string>(); } catch (...) {}
        BindingRegistry::GetSingleton()->ResetToDefault(id.c_str());
        PushKeybindList();
    });
    g_api->BindUIEvent(v, "closeKeybinds", [](const char*) { CloseKeybinds(); });
    g_api->RegisterConsoleCallback(v, LogJS);
    logger::info("PrismaHUD: keybind menu DOM ready");
}

constexpr int kHudViewOrder = 1'000'000;

static PrismaView MakeView(const char* html,
                           PRISMA_UI_API::OnDomReadyCallback onReady,
                           bool hideOnCreate = true)
{
    if (!g_api) return 0;
    auto view = g_api->CreateView(html, onReady);
    if (!view) {
        logger::error("PrismaHUD: CreateView failed for '{}' -- widget will not appear", html);
        return 0;
    }
    g_api->SetOrder(view, kHudViewOrder);
    if (hideOnCreate)
        g_api->Hide(view);
    g_api->RegisterConsoleCallback(view, LogJS);
    return view;
}

static void OnQuickLootDomReady(PrismaView v)
{
    auto* qlc = QuickLootController::GetSingleton();
    qlc->SetView(v);

    HUDDataProvider::GetSingleton()->BindPositionReport(v, "prisma_quickloot");
    if (!g_api) return;

    g_api->BindUIEvent(v, "requestUpdate", [](const char*) {
        HUDDataProvider::PollCombatThrottled();
        QuickLootController::GetSingleton()->OnRequestUpdate();
    });
    g_api->BindUIEvent(v, "activateChoice", [](const char* json) {
        try {
            auto j   = nlohmann::json::parse(json);
            auto idx = j.at("index").get<std::uint32_t>();
            QuickLootController::GetSingleton()->TriggerActivateChoice(idx);
        } catch (...) {}
    });
    g_api->BindUIEvent(v, "reportVisibility", [](const char* json) {
        try {
            auto j   = nlohmann::json::parse(json);
            auto id  = j.at("id").get<std::string>();
            bool vis = j.at("visible").get<bool>();
            HUDLayoutManager::GetSingleton()->SetRuntimeVisible(id.c_str(), vis);
        } catch (...) {}
    });
    HUDLayoutManager::GetSingleton()->ApplyLayout(v);
}

static void CreateBuiltinViews()
{
    if (!g_quickLootView || !g_api->IsValid(g_quickLootView)) {
        g_quickLootView = MakeView("hud_quickloot.html", OnQuickLootDomReady, true);
        if (g_quickLootView)
            HUDLayoutManager::GetSingleton()->RegisterWidget(g_quickLootView,
                {"prisma_quickloot", "Quick Loot", 1480.f/1920.f, 360.f/1080.f, 440.f/1920.f, 380.f/1080.f});
    }
}

static void DestroyBuiltinViews()
{
    if (!g_api) return;
    if (g_quickLootView && g_api->IsValid(g_quickLootView)) g_api->Destroy(g_quickLootView);
    g_quickLootView = 0;
    logger::info("PrismaHUD: builtin HUD views destroyed (vanilla HUD closing)");
}

namespace {
    class HUDVisibilitySink : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
    {
    public:
        RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent& a_event,
                                               RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
        {
            if (!(a_event.menuName == "HUDMenu"sv)) return RE::BSEventNotifyControl::kContinue;

            if (a_event.opening) {
                if (g_api) CreateBuiltinViews();
                logger::info("PrismaHUD: HUDMenu reopened -- builtin views recreated");
            } else {
                DestroyBuiltinViews();
            }
            return RE::BSEventNotifyControl::kContinue;
        }

        static HUDVisibilitySink* GetSingleton()
        {
            static HUDVisibilitySink singleton;
            return std::addressof(singleton);
        }
    };
}

static void InstallHUDVisibilitySink()
{
    if (auto* ui = RE::UI::GetSingleton()) {
        ui->RegisterSink(HUDVisibilitySink::GetSingleton());
        logger::info("PrismaHUD: HUDMenu open/close sink registered (kill builtin HUD on hide)");
    } else {
        logger::warn("PrismaHUD: RE::UI singleton unavailable -- HUDMenu kill-on-hide not installed");
    }
}

namespace {
    std::unordered_set<std::string> g_openSuppressingMenus;

    bool IsSuppressingMenuName(std::string_view name)
    {
        static constexpr std::string_view kNames[] = {
            "WorkshopMenu", "TerminalMenu", "ChemistryMenu", "CookingMenu", "LockpickingMenu",
            "PauseMenu", "PipboyMenu",
            "MessageBoxMenu", "BarterMenu", "ExamineMenu", "ExamineConfirmMenu", "SitWaitMenu"
        };
        for (auto n : kNames) if (n == name) return true;
        return false;
    }

    class MenuSuppressSink : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
    {
    public:
        RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent& a_event,
                                              RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
        {
            std::string name = a_event.menuName.c_str() ? a_event.menuName.c_str() : "";
            if (!IsSuppressingMenuName(name)) return RE::BSEventNotifyControl::kContinue;

            if (a_event.opening) g_openSuppressingMenus.insert(name);
            else                 g_openSuppressingMenus.erase(name);
            HUDLayoutManager::GetSingleton()->SetMenuSuppressed(!g_openSuppressingMenus.empty());
            return RE::BSEventNotifyControl::kContinue;
        }

        static MenuSuppressSink* GetSingleton() { static MenuSuppressSink s; return &s; }
    };
}

static void InstallMenuSuppressSink()
{
    if (auto* ui = RE::UI::GetSingleton()) {
        ui->RegisterSink(MenuSuppressSink::GetSingleton());
        logger::info("PrismaHUD: menu-suppress sink registered (Workshop/Terminal/Crafting/Lockpicking hide the HUD)");
    } else {
        logger::warn("PrismaHUD: RE::UI singleton unavailable -- menu-suppress sink not installed");
    }
}

static void CreateEditorView()
{
    if (g_editorView && g_api->IsValid(g_editorView)) {
        OnEditorDomReady(g_editorView);
        return;
    }
    HUDLayoutManager::GetSingleton()->ResetEditorListeners();
    g_editorView = MakeView("hud_editor.html", OnEditorDomReady, true);
    logger::info("PrismaHUD: editor view created (id={})", g_editorView);
}

static bool g_diagRolloverSuppressed = true;

static void LogQuickContainerState(const char* a_when)
{
    try {
        auto* fav = RE::FavoritesManager::GetSingleton();
        if (!fav) { logger::info("PrismaHUD DIAG [{}]: FavoritesManager null", a_when); return; }
        auto& cached = fav->quickContainerMode.eventDataStruct;
        if (!cached.optionalValue.has_value()) {
            logger::info("PrismaHUD DIAG [{}]: quick-container state NOT POPULATED (optionalValue empty)",
                         a_when);
            return;
        }
        const auto& qc = *cached.optionalValue;
        logger::info("PrismaHUD DIAG [{}]: populated | mode={} perkText='{}' perkEnabled={} "
                     "container='{}' aButton='{}' locked={}",
                     a_when, static_cast<int>(qc.mode.underlying()), qc.perkButtonText.c_str(),
                     qc.perkButtonEnabled, qc.containerName.c_str(), qc.aButtonText.c_str(),
                     qc.isLocked);
    } catch (...) {}
}

static void RegisterRolloverDiagToggle()
{
    KeyHandler::GetSingleton()->Register(0x78 , KeyEventType::KEY_DOWN, []() -> bool {
        if (!InputMods::ShiftDown()) return false;
        if (!g_api) return false;

        LogQuickContainerState(g_diagRolloverSuppressed ? "BEFORE flip, suppress was ON"
                                                        : "BEFORE flip, suppress was OFF");
        g_diagRolloverSuppressed = !g_diagRolloverSuppressed;
        const bool ok = g_api->SuppressHUDWidget("HUDRollover", g_diagRolloverSuppressed);
        logger::info("PrismaHUD DIAG: HUDRollover suppression -> {} (SuppressHUDWidget returned {}) -- "
                     "look away from the bench and back, then check whether Manage is present",
                     g_diagRolloverSuppressed ? "ON" : "OFF", ok);
        LogQuickContainerState(g_diagRolloverSuppressed ? "AFTER flip, suppress now ON"
                                                        : "AFTER flip, suppress now OFF");
        return false;
    });
}

static std::atomic<std::uint64_t> g_loadGeneration{0};

static void HandleLoadedGameOnVerifiedThread()
{
    if (!g_api || !g_api->IsGameThread()) {
        logger::critical("PrismaHUD: load initialization reached an unverified thread; refusing engine/UI mutation");
        return;
    }

    if (g_editorOpen.load()) CloseEditor();
    if (g_keybindsOpen.load()) CloseKeybinds();
    g_editorOpen.store(false);
    g_keybindsOpen.store(false);
    BindingRegistry::GetSingleton()->SetCaptureMode(false);
    g_openSuppressingMenus.clear();

    auto* layout = HUDLayoutManager::GetSingleton();
    layout->ResetTransientForLoad();
    QuickLootController::GetSingleton()->ResetTransientForLoad();
    SleepWaitMirrorController::GetSingleton()->ResetForLoad();

    if (g_editorView && !g_api->IsValid(g_editorView))
        g_editorView = 0;
    CreateBuiltinViews();
    CreateEditorView();

    if (g_keybindsView && !g_api->IsValid(g_keybindsView)) g_keybindsView = 0;
    if (!g_keybindsView)
        g_keybindsView = MakeView("hud_keybinds.html", OnKeybindsDomReady, true);

    if (g_messageBoxView && !g_api->IsValid(g_messageBoxView)) g_messageBoxView = 0;
    if (!g_messageBoxView)
        g_messageBoxView = MakeView("hud_messagebox.html", OnMessageBoxDomReady, true);
    MessageBoxController::GetSingleton()->SetView(g_messageBoxView);

    if (g_specialView && !g_api->IsValid(g_specialView)) g_specialView = 0;
    if (!g_specialView)
        g_specialView = MakeView("hud_special.html", OnSpecialDomReady, true);
    SpecialMirrorController::GetSingleton()->SetView(g_specialView);

    if (g_sleepWaitMirrorView && !g_api->IsValid(g_sleepWaitMirrorView)) g_sleepWaitMirrorView = 0;
    if (!g_sleepWaitMirrorView)
        g_sleepWaitMirrorView = MakeView("hud_sleepwait_mirror.html", OnSleepWaitMirrorDomReady, true);
    SleepWaitMirrorController::GetSingleton()->SetView(g_sleepWaitMirrorView);

    layout->LoadAndApply();
    QuickLootInput::Install();
    QuickLootController::GetSingleton()->InstallPickRefSink();
}

static void RunVerifiedLoadInit(void* userdata)
{
    const auto generation = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(userdata));
    if (generation == 0 || generation != g_loadGeneration.load(std::memory_order_acquire)) {
        logger::debug("PrismaHUD: discarded stale load initialization generation {}", generation);
        return;
    }
    HandleLoadedGameOnVerifiedThread();
}

static void F4SEMessageHandler(F4SE::MessagingInterface::Message* message)
{
    switch (message->type) {
    case F4SE::MessagingInterface::kGameDataReady: {
        g_api = PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI11>();
        if (!g_api) {
            logger::critical("PrismaHUD: PrismaUI V11 is required for verified Fallout window-thread "
                             "dispatch; disabling HUD initialization rather than mutating engine state "
                             "through an older unverified contract.");
            return;
        }

        ControlLabels::SetFrameworkAPI(g_api);

        HUDLayoutManager::GetSingleton()->SetAPI(g_api);
        HUDDataProvider::GetSingleton()->SetAPI(g_api);
        QuickLootController::GetSingleton()->SetAPI(g_api);
        QuickLootController::GetSingleton()->InstallMenuSink();
        QuickLootController::InstallQuickContainerTakeHook();

        MessageBoxController::GetSingleton()->SetAPI(g_api);
        MessageBoxController::GetSingleton()->InstallMenuSink();

        SpecialMirrorController::GetSingleton()->SetAPI(g_api);
        SpecialMirrorController::GetSingleton()->InstallMenuSink();

        SleepWaitMirrorController::GetSingleton()->SetAPI(g_api);
        SleepWaitMirrorController::GetSingleton()->InstallMenuSink();

        InstallVanillaHUDSuppression(g_api);
        InstallHUDVisibilitySink();
        InstallMenuSuppressSink();
        g_api->EnableActivateChoiceFilter(true, false);

        HUDLayoutManager::GetSingleton()->LoadSettings();

        KeyHandler::RegisterSink();
        [[maybe_unused]] auto _editorKey = KeyHandler::GetSingleton()->Register(kEditorToggleKey, KeyEventType::KEY_DOWN, []() -> bool {
            if (!InputMods::ShiftDown()) return false;
            if (g_editorOpen.load()) CloseEditor(); else OpenEditor();
            return true;
        });
        RegisterHideKey();

        {
            auto* reg = BindingRegistry::GetSingleton();
            reg->LoadFromDisk();
            reg->RegisterBinding({ "rollover_hide", "Hide Loot Rollover", "PrismaHUD",
                                   0x5A , false, false, false },
                                 CbToggleRollover, nullptr, nullptr, nullptr);
        }
        [[maybe_unused]] auto _kbKey = KeyHandler::GetSingleton()->Register(
            0x79 , KeyEventType::KEY_DOWN, []() -> bool {
                if (!InputMods::ShiftDown()) return false;
                if (g_keybindsOpen.load()) CloseKeybinds(); else OpenKeybinds();
                return true;
            });

        ProbeControlMapEncoding();

        RegisterRolloverDiagToggle();
        logger::info("PrismaHUD: ready (Shift+F11 = editor, Shift+F10 = keybinds, Shift+F9 = HUDRollover "
                     "suppression A/B toggle [diagnostic], hide-HUD key 0x{:X})",
                     HUDLayoutManager::GetSingleton()->GetHideKey());
        break;
    }
    case F4SE::MessagingInterface::kPreLoadGame:
        g_loadGeneration.fetch_add(1, std::memory_order_acq_rel);
        break;
    case F4SE::MessagingInterface::kPostLoadGame:
    case F4SE::MessagingInterface::kNewGame: {
        if (!g_api) {
            logger::error("PrismaHUD: load event arrived without PrismaUI V11; HUD load init skipped");
            break;
        }
        const auto generation = g_loadGeneration.fetch_add(1, std::memory_order_acq_rel) + 1;
        if (!g_api->DispatchToGameThread(&RunVerifiedLoadInit,
                                         reinterpret_cast<void*>(static_cast<std::uintptr_t>(generation)))) {
            logger::critical("PrismaHUD: verified window-thread dispatch rejected load generation {}; "
                             "HUD load initialization was not run", generation);
        }
        break;
    }
    }
}

extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Query(const F4SE::QueryInterface* a_f4se,
                                                    F4SE::PluginInfo*           a_info)
{
    std::filesystem::path logPath;
    {
        PWSTR docs = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents, 0, nullptr, &docs)) && docs)
            logPath = docs;
        if (docs) CoTaskMemFree(docs);
    }
    if (logPath.empty()) logPath = std::filesystem::current_path();
    logPath /= L"My Games"; logPath /= L"Fallout4"; logPath /= L"F4SE";
    std::error_code _ec; std::filesystem::create_directories(logPath, _ec);
    logPath /= "PrismaHUD_F4.log";
    auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logPath.string(), true);
    auto log  = std::make_shared<spdlog::logger>("global log"s, std::move(sink));
    log->set_level(spdlog::level::info);
    log->flush_on(spdlog::level::info);
    spdlog::set_default_logger(std::move(log));
    spdlog::set_pattern("[%Y-%m-%d %T.%e] [%l] [%t] [%s:%#] %v");

    logger::info("PrismaHUD_F4 v1.0.0");

    a_info->infoVersion = F4SE::PluginInfo::kVersion;
    a_info->name        = "PrismaHUD_F4";
    a_info->version     = 1;

    if (a_f4se->IsEditor()) { logger::critical("Loaded in editor — aborting."); return false; }

    if (a_f4se->RuntimeVersion() < F4SE::RUNTIME_1_10_162) {
        logger::critical("Unsupported runtime");
        return false;
    }
    return true;
}

extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Load(const F4SE::LoadInterface* a_f4se)
{
    F4SE::Init(a_f4se);
    const auto* messaging = F4SE::GetMessagingInterface();
    if (!messaging) { logger::critical("No messaging interface"); return false; }
    messaging->RegisterListener(F4SEMessageHandler);
    return true;
}
