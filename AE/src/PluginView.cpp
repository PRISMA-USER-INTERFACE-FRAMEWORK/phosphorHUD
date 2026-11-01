#include "PluginState.h"

namespace Phosphor::Plugin {
PrismaView g_view = 0;
bool domReady = false, dataReady = false, session = false, displaying = false;
bool realMenuPaused = false, replayPaused = false, settingsDirty = true;
std::atomic<bool> toggleRequested{false};
std::atomic<unsigned> generation{0};
Phosphor::Replay replay;

namespace {
bool hookedMenus = false;
bool waitingForPrismaLogged = false;
Clock::time_point nextViewAttempt{};
std::shared_ptr<spdlog::logger> messageLog;
std::jthread scheduler;
}

bool ViewValid() { return api && g_view && api->IsValid(g_view); }

// No exception may unwind across the game/framework callback boundary.
void Guard(const std::function<void()>& task) noexcept {
    try { task(); }
    catch (const std::exception& e) { spdlog::error("Phosphor task failed: {}", e.what()); }
    catch (...) { spdlog::error("Phosphor task failed with an unknown exception"); }
}
bool Queue(std::function<void()> task) {
    if (!api) return false;
    const auto* tasks = F4SE::GetTaskInterface();
    if (!tasks) {
        spdlog::error("F4SE task interface unavailable; cannot schedule HUD work");
        return false;
    }
    tasks->AddTask([task = std::move(task)] { Guard(task); });
    return true;
}

void Send(const Json& message) {
    if (!domReady || !ViewValid()) return;
    const auto serialized = message.dump(-1, ' ', false, Json::error_handler_t::replace);
    api->InteropCall(g_view, "hudUpdate", serialized.c_str());
    if (messageLog) messageLog->debug("{}", serialized);
}

namespace {
constexpr const char* kReplacedWidgets[] = {"HUDPlayerHealthMeter", "HUDActionPointMeter", "HUDRadiationMeter",
    "HUDExperienceMeter", "HUDCompass", "HUDAmmoCounter", "HUDExplosiveAmmoCounter", "HUDActiveEffectsDisplay",
    "HUDQuickContainer", "HUDEnemyHealthMeter", "HUDStealthMeter"};
constexpr const char* kQuestWidgets[] = {"HUDQuestUpdates", "HUDObjectiveUpdates", "HUDQuestVaultBoy"};
bool suppressed = false;
bool questsSuppressed = false;
void SetWidgets(const char* const* list, std::size_t count, bool on, int bit0) {
    for (std::size_t i = 0; i < count; ++i) {
        if ((options.keepWidgetMask >> (bit0 + static_cast<int>(i))) & 1) continue;
        if (std::string_view{list[i]} == "HUDCompass") continue;
#if defined(PHOSPHOR_AE)
        const std::string_view widget{list[i]};
        if ((widget == "HUDCompass" && !Phosphor::AE::Enabled("bCompass")) ||
            (widget == "HUDActiveEffectsDisplay" && !Phosphor::AE::Enabled("bEffects")) ||
            (widget == "HUDQuickContainer" && !Phosphor::AE::Enabled("bQuickLoot")) ||
            (widget == "HUDEnemyHealthMeter" && !Phosphor::AE::Enabled("bTarget"))) continue;
#endif
        const bool ok = api->SuppressHUDWidget(list[i], on);
        spdlog::info("{} vanilla {}: {}", on ? "Hiding" : "Restoring", list[i], ok ? "ok" : "refused");
    }
}
}

void SetSuppressed(bool on, bool quests) {
#if defined(PHOSPHOR_AE)
    AE_GATE("bHideVanilla", );
#endif
    on = on && options.hideVanilla && options.hideWidgets;
    quests = quests && on;
    if (!api) return;
    if (on != suppressed) { SetWidgets(kReplacedWidgets, std::size(kReplacedWidgets), on, 0); suppressed = on; }
    if (quests != questsSuppressed) { SetWidgets(kQuestWidgets, std::size(kQuestWidgets), quests, static_cast<int>(std::size(kReplacedWidgets))); questsSuppressed = quests; }
}

namespace {
void HideReplay() {
    replay.Stop();
    displaying = false;
    replayPaused = false;
    realMenuPaused = false;
    toggleRequested.store(false);
    if (ViewValid()) {
        api->Hide(g_view);
        api->Destroy(g_view);
    }
    g_view = 0;
    domReady = false;
    if (dataReady) CreateView();
}
}

void RestoreNativeHud() {
    Phosphor::Live::ResetNativeHud();
    nativeHudActive = false;
    nativeRestorePending = true;
    Phosphor::Live::RequestNativeHud({});
}
void EndSession() {
    generation.fetch_add(1);
    session = false;
    StopLive();
    SetSuppressed(false);
    RestoreNativeHud();
    messagesBaseline = false;
    HideReplay();
}

namespace {
void ConsoleMessage(PrismaView source, PRISMA_UI_API::ConsoleMessageLevel level, const char* message) noexcept {
    try {
        if (level == PRISMA_UI_API::ConsoleMessageLevel::Error) {
            spdlog::error("Prisma view {}: {}", source, message ? message : "");
        }
    } catch (...) {}
}

void DomReady(PrismaView readyView) noexcept {
    try {
        Queue([readyView] {
            if (readyView != g_view || !ViewValid() || !dataReady) return;
            domReady = true;
            StopLive();
            api->SetViewRole(g_view, PRISMA_UI_API::ViewRole::kWidget);
            api->RegisterConsoleCallback(g_view, ConsoleMessage);
            api->Hide(g_view);
            SendSettings();
            spdlog::info("HUD DOM ready; Ctrl+F10 replays after loading a save");
        });
    } catch (...) {}
}
}

void CreateView() {
    if (!dataReady || ViewValid()) return;
    const auto now = Clock::now();
    if (now < nextViewAttempt) return;
    nextViewAttempt = now + std::chrono::seconds(1);
    domReady = false;
    displaying = false;
    replay.Stop();
    g_view = api->CreateView("PhosphorHUD/hud.html", DomReady);
    if (!g_view) {
        if (!waitingForPrismaLogged) {
            spdlog::info("Waiting for PrismaUI initialization before creating HUD view");
            waitingForPrismaLogged = true;
        }
        return;
    }
    waitingForPrismaLogged = false;
    api->SetViewRole(g_view, PRISMA_UI_API::ViewRole::kWidget);
    api->Hide(g_view);
    spdlog::info("HUD view created: {}", g_view);
}

bool BlockingMenu() {
    const auto* ui = RE::UI::GetSingleton();
    if (!ui) return true;
    constexpr const char* names[] = {"MainMenu", "LoadingMenu", "FaderMenu", "PauseMenu", "Console", "PipboyMenu",
        "ContainerMenu", "BarterMenu", "TerminalMenu", "ExamineMenu", "LockpickingMenu", "LevelUpMenu",
        "SleepWaitMenu", "LooksMenu", "BookMenu", "SPECIALMenu", "CreditsMenu", "CookingMenu",
        "PowerArmorModMenu"};
    for (const auto* name : names) if (ui->GetMenuOpen(RE::BSFixedString(name))) return true;
    return api->IsAnyPanelVisible(g_view);
}

void ToggleReplay() {
    if (!session || !domReady || !ViewValid() || BlockingMenu()) return;
    if (displaying) {
        HideReplay();
        spdlog::info("Replay hidden");
        return;
    }
    const auto path = "Data/F4SE/Plugins/PhosphorHUD/replay.jsonl";
    std::ifstream file(path, std::ios::binary);
    if (!file) { spdlog::error("Replay file unavailable: {}", path); return; }
    const auto bytes = std::filesystem::file_size(path);
    if (bytes > 4 * 1024 * 1024) { spdlog::error("Replay exceeds 4 MiB limit"); return; }
    std::ifstream schemaFile("Data/F4SE/Plugins/PhosphorHUD/hud-messages.schema.json");
    if (!schemaFile) { spdlog::error("Replay schema unavailable; reinstall Phosphor HUD"); return; }
    const auto schema = Json::parse(schemaFile, nullptr, false);
    if (!schema.is_object()) { spdlog::error("Replay schema is invalid; reinstall Phosphor HUD"); return; }
    auto loaded = Phosphor::ReadReplay(file, schema);
    if (!loaded.errors.empty()) {
        for (const auto& error : loaded.errors) spdlog::error("Replay rejected: {}", error);
        return;
    }
    spdlog::info("Starting replay: {} messages", loaded.messages.size());
    Send(Json{{"reset", true}, {"paused", false}});
    SendSettings();
    replayPaused = false;
    realMenuPaused = false;
    replay.Start(std::move(loaded.messages), Clock::now());
    displaying = true;
    api->Show(g_view);
}

namespace {
class MenuWatch final : public RE::BSTEventSink<RE::MenuOpenCloseEvent> {
    RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent& event,
                                         RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override {
        Guard([&] {
            const std::string name(event.menuName.c_str());
            const bool opening = event.opening;
            if (opening && name == "LoadingMenu") {
                Phosphor::Live::ResetNativeHud();
                Phosphor::Mods::SetEventCapture(false);
                Phosphor::Mines::SetEventCapture(false);
            }
            if (opening && (name == "LoadingMenu" || name == "FaderMenu") && ViewValid()) {
                api->Hide(g_view);
                liveShown = false;
                realMenuPaused = true;
            }
            Queue([name, opening] {
                if (name == "PauseMenu" && !opening) settingsDirty = true;
                if (name == "SleepWaitMenu" && !opening) Phosphor::Live::ClearSurvivalMessage("rest");
                if (name == "MainMenu" && opening) EndSession();
            });
        });
        return RE::BSEventNotifyControl::kContinue;
    }
} menuWatch;
}

void HandleMessage(F4SE::MessagingInterface::Message* message) {
    if (!message) return;
    Guard([&] {
        using Messaging = F4SE::MessagingInterface;
        if (message->type == Messaging::kPostLoad) {
            const auto ini = ReadIni("Data/F4SE/Plugins/PhosphorHUD.ini");
            if (const auto it = ini.find("bLiveHud"); it != ini.end()) options.live = it->second != 0;
            if (const auto it = ini.find("bHideVanillaMeters"); it != ini.end()) options.hideVanilla = it->second != 0;
            if (const auto it = ini.find("iLocationMarkerMeters"); it != ini.end()) options.locationMeters = std::max(0, it->second);
            if (const auto it = ini.find("bHideVanillaWidgets"); it != ini.end()) options.hideWidgets = it->second != 0;
            if (const auto it = ini.find("bHideVanillaText"); it != ini.end()) options.hideText = it->second != 0;
            if (const auto it = ini.find("bHideVanillaCrosshair"); it != ini.end()) options.hideCrosshair = it->second != 0;
            if (const auto it = ini.find("bHideVanillaFlashlight"); it != ini.end()) options.hideFlashlight = it->second != 0;
            if (const auto it = ini.find("iKeepVanillaWidgetMask"); it != ini.end()) options.keepWidgetMask = std::max(0, it->second);
            if (const auto it = ini.find("bEngineQuestData"); it != ini.end()) options.engineQuests = it->second != 0;
            if (const auto it = ini.find("bEngineProbe"); it != ini.end()) options.engineProbe = it->second != 0;
            if (const auto it = ini.find("bMessageLog"); it != ini.end()) options.messageLog = it->second != 0;
            spdlog::info("Options: live HUD {}, hide vanilla meters {} (widgets {}, text {}, crosshair {}, flashlight {}, keep mask {})",
                         options.live, options.hideVanilla, options.hideWidgets, options.hideText, options.hideCrosshair,
                         options.hideFlashlight, options.keepWidgetMask);
            if (options.messageLog) {
                messageLog = spdlog::rotating_logger_mt("PhosphorMessages", "Data/F4SE/Plugins/PhosphorHUD/messages.jsonl", 2 * 1024 * 1024, 2);
                messageLog->set_pattern("%v");
                messageLog->set_level(spdlog::level::debug);
                messageLog->flush_on(spdlog::level::debug);
            }
            api = PRISMA_UI_API::RequestPluginAPI<API>();
            if (!api) spdlog::warn("PrismaUI V12 unavailable; Phosphor HUD disabled");
            else spdlog::info("Connected to PrismaUI V12 (ABI 139)");
            return;
        }
        if (!api) return;
        switch (message->type) {
        case Messaging::kGameDataReady: {
            const bool ready = message->data != nullptr;
            Queue([ready] {
                dataReady = ready;
                if (!ready) {
                    EndSession();
                    if (ViewValid()) api->Destroy(g_view);
                    g_view = 0;
                    domReady = false;
                    return;
                }
                if (!hookedMenus) {
                    if (auto* ui = RE::UI::GetSingleton()) {
                        ui->RegisterSink<RE::MenuOpenCloseEvent>(&menuWatch);
                        RegisterHitWatch();
                        hookedMenus = true;
                    }
                }
                Phosphor::Live::LogXpSettings();
                Phosphor::Mods::Resolve();
                Phosphor::QuickLootFirst::DataReady();
                Phosphor::Mods::RegisterSinks();
                Phosphor::Mines::RegisterSinks();
                Phosphor::Pipboy::Start("Data/F4SE/Plugins/PhosphorHUD/pipboy-tree.json");
                CreateView();
                if (!scheduler.joinable()) scheduler = std::jthread(Poll);
            });
            break;
        }
        case Messaging::kPreLoadGame:
            Phosphor::Live::SetNativeHudLoading(true);
            generation.fetch_add(1);
            Phosphor::Mods::SetEventCapture(false);
            Phosphor::Mines::SetEventCapture(false);
            Queue(EndSession);
            break;
        case Messaging::kPostLoadGame: {
            const bool success = message->data != nullptr;
            Queue([success] {
                EndSession();
                session = success;
                Phosphor::Live::SetNativeHudLoading(!success);
                if (session) CreateView();
                spdlog::info("Save load {}; replay {}", success ? "succeeded" : "failed", success ? "available" : "disabled");
            });
            break;
        }
        case Messaging::kNewGame:
            Queue([] { EndSession(); session = true; Phosphor::Live::SetNativeHudLoading(false); CreateView(); });
            break;
        default: break;
        }
    });
}
}
