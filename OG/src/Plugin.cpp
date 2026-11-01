#include <F4SE/F4SE.h>
#include <RE/Fallout.h>
#include <Windows.h>
#include <timeapi.h>
#include <PrismaUI_F4_API.h>
#include <spdlog/spdlog.h>
#if defined(PHOSPHOR_AE)
#include "../ae/AeRuntime.h"
#endif
#include <spdlog/sinks/rotating_file_sink.h>
#include "Core.h"
#include "Live.h"
#include "EngineProbe.h"
#include "EngineQuests.h"
#include "Mods.h"
#include "Mines.h"
#include "QuickLootFirst.h"
#include <atomic>
#include <cctype>
#include <map>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <format>
#include <functional>
#include <memory>
#include <optional>
#include <thread>

static_assert(static_cast<unsigned>(PRISMA_UI_API::InterfaceVersion::V12) == 139,
              "Use the ABI-safe PrismaUI 2.1.1 mod-root header, not its old F4SE/plugins copy");

namespace {
using Phosphor::Json;
using API = PRISMA_UI_API::IVPrismaUI12;
using Clock = Phosphor::Replay::Clock;
API* api = nullptr; // Assigned at PostLoad before scheduler starts; never changed.
PrismaView g_view = 0;
bool domReady = false, dataReady = false, session = false, displaying = false;
bool realMenuPaused = false, replayPaused = false, settingsDirty = true;
bool hookedMenus = false;
bool waitingForPrismaLogged = false;
Clock::time_point nextViewAttempt{};
std::atomic<bool> tickQueued{false};
std::atomic<bool> toggleRequested{false};
std::atomic<unsigned> generation{0};
std::jthread scheduler;
Phosphor::Replay replay;
std::shared_ptr<spdlog::logger> messageLog;

// Live HUD (Stage 1)
// hideVanilla is the master switch; the four below split it for troubleshooting (e.g. AAF's wizard lives in HUDMenu).
// keepWidgetMask: bit i leaves kReplacedWidgets[i] (then kQuestWidgets from bit 11) visible.
struct PluginOptions {
    bool live = true; bool hideVanilla = true; int locationMeters = 100;
    bool hideWidgets = true, hideText = true, hideCrosshair = true, hideFlashlight = true; int keepWidgetMask = 0;
    bool engineQuests = true;  // bEngineQuestData: quest/map data from the engine (0 = Pip-Boy feed only)
    bool engineProbe = false;  // bEngineProbe: Ctrl+F11 diagnostic dump
    bool messageLog = false;
} options;
bool liveStarted = false, liveShown = false, suppressed = false, pausedSent = false;
bool nativeHudActive = false, nativeRestorePending = false;
Phosphor::Delta delta;
Phosphor::RadiationEvents radiation;
Phosphor::Live::Raw raw;
Clock::time_point lastSlow{}, lastTick{};
// One on-screen notice per launch when the Pip-Boy feed (quest markers, map locations) never connects.
Clock::time_point liveStartedAt{};
bool pipNoticeSent = false;
// Engine probe (diagnostic, bEngineProbe=1 in PhosphorHUD.ini): Ctrl+F11 logs the engine's quest and map data.
std::atomic<bool> probeRequested{false};
double predHour = -1;
float predScale = 20;
int lastLevel = 0;
float lastExp = 0;
// Vanilla widgets Phosphor replaces in Stage 1 (class names confirmed in PrismaUI's DLL).
constexpr const char* kReplacedWidgets[] = {"HUDPlayerHealthMeter", "HUDActionPointMeter", "HUDRadiationMeter",
    "HUDExperienceMeter", "HUDCompass", "HUDAmmoCounter", "HUDExplosiveAmmoCounter", "HUDActiveEffectsDisplay",
    "HUDQuickContainer", "HUDEnemyHealthMeter", "HUDStealthMeter"};
// Key text per ControlMap event for the loot panel, refreshed every couple of seconds.
std::map<std::string, std::string> keyCache;
Clock::time_point keyCacheAt{};
std::string lastLootId;
std::map<std::string, float> illnesses;  // what the HUD currently shows
std::string KeyFor(const char* event) {
    const auto now = Clock::now();
    if (now - keyCacheAt > std::chrono::seconds(2)) { keyCache.clear(); keyCacheAt = now; }
    auto it = keyCache.find(event);
    if (it != keyCache.end()) return it->second;
    char buffer[64] = {};
    std::string key = api && api->GetButtonPrompt(event, buffer, sizeof(buffer)) ? buffer : "";
    keyCache[event] = key;
    return key;
}
Json LootNow() {
    std::string line;
    auto loot = Phosphor::Live::ReadLoot(KeyFor, &line);
    const auto id = line.substr(0, 8);
    if (id != lastLootId) {
        if (!line.empty()) spdlog::info("Quick loot: {}", line);
        lastLootId = id;
    }
    return loot;
}
std::string effectsKey;
float maxEffectSeconds = 2 * 3600.0F;  // MCM iMaxEffectHours (0 = keep all)
Clock::time_point lastEffects{};
bool dropMarkers = true;               // MCM bLegendaryDropMarkers
std::string dialogueRule = "recommended";  // MCM iCtxDialogue as contextRules.dialogue (game subtitles in dialogue)
std::map<std::string, float> affinitySeen;   // companion name -> last affinity
std::optional<double> karmaSeen;
std::optional<std::string> localFactionSeen;
bool localFactionBaseline = false;
std::string targetSeen;
std::size_t targetMutationsSeen = 0;
Clock::time_point lastKarmaChange{}, lastRepEvent{};

bool ViewValid() { return api && g_view && api->IsValid(g_view); }
void Send(const Json& message);

// Vanilla quest pop-ups are only hidden while the Pip-Boy feed supplies Phosphor's quest banners.
constexpr const char* kQuestWidgets[] = {"HUDQuestUpdates", "HUDObjectiveUpdates", "HUDQuestVaultBoy"};
bool questsSuppressed = false;
void SetWidgets(const char* const* list, std::size_t count, bool on, int bit0) {
    for (std::size_t i = 0; i < count; ++i) {
        if ((options.keepWidgetMask >> (bit0 + static_cast<int>(i))) & 1) continue;
        // The compass is shrunk through Scaleform instead (Live::ShrinkVanillaCompass): suppressing the widget breaks AAF.
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
void SetSuppressed(bool on, bool quests = false) {
#if defined(PHOSPHOR_AE)
    AE_GATE("bHideVanilla", );
#endif
    on = on && options.hideVanilla && options.hideWidgets;
    quests = quests && on;
    if (!api) return;
    if (on != suppressed) { SetWidgets(kReplacedWidgets, std::size(kReplacedWidgets), on, 0); suppressed = on; }
    if (quests != questsSuppressed) { SetWidgets(kQuestWidgets, std::size(kQuestWidgets), quests, static_cast<int>(std::size(kReplacedWidgets))); questsSuppressed = quests; }
}

// Last actor the player hit, written by the hit event sink (any thread), read on the game thread.
std::atomic<std::uint32_t> lastHitId{0};
std::atomic<std::int64_t> lastHitAt{0};
std::int64_t NowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now().time_since_epoch()).count();
}
// CommonLibF4-NG's TESHitEvent::GetEventSource has its OG/NG Address Library IDs swapped: its OG ID (989868)
// resolves to data (Fallout4.exe+59D8140) and calling it crashed the game at save load (Buffout logs
// 2026-10-10). 1411899 is the OG ID: it resolves to 0x444670, among the other event-source getters
// (TESActivateEvent 0x441C90, TESDeathEvent 0x442550, cell attach 0x434DB0).
RE::BSTEventSource<RE::TESHitEvent>* HitEventSource() {
#if defined(PHOSPHOR_AE)
    AE_GATE("bTarget", nullptr);
    return RE::TESHitEvent::GetEventSource();
#else
    using func_t = RE::BSTEventSource<RE::TESHitEvent>* (*)();
    static REL::Relocation<func_t> func{ REL::ID(1411899) };
    return func();
#endif
}
class HitWatch final : public RE::BSTEventSink<RE::TESHitEvent> {
    RE::BSEventNotifyControl ProcessEvent(const RE::TESHitEvent& event, RE::BSTEventSource<RE::TESHitEvent>*) override {
#if defined(PHOSPHOR_AE)
        if (!Phosphor::AE::Enabled("bTarget")) return RE::BSEventNotifyControl::kContinue;
#endif
        try {
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (player && event.cause.get() == player && event.target && event.target.get() != player && event.target->As<RE::Actor>()) {
                lastHitId.store(event.target->GetFormID());
                lastHitAt.store(NowMs());
            }
        } catch (...) {}
        return RE::BSEventNotifyControl::kContinue;
    }
} hitWatch;

// Quest banners from changes in the Pip-Boy quest list.
struct QuestSeen { std::string text; std::vector<std::pair<std::string, bool>> objectives; };
std::map<std::uint32_t, QuestSeen> questsSeen;
bool questBaseline = false;
void QuestEvents(const Phosphor::Pipboy::Snapshot& pip, Json& message) {
    if (!pip.connected || pip.quests.empty()) { questBaseline = false; return; }
    std::map<std::uint32_t, QuestSeen> now;
    for (const auto& q : pip.quests) {
        QuestSeen seen{q.text, {}};
        for (const auto& o : q.objectives) seen.objectives.emplace_back(o.text, o.completed);
        now[q.formId] = std::move(seen);
    }
    if (!questBaseline) { questsSeen = std::move(now); questBaseline = true; return; }
    Json banners = Json::array(), lines = Json::array();
    for (const auto& [id, q] : now) {
        const auto before = questsSeen.find(id);
        const auto firstOpen = [&]() -> std::string {
            for (const auto& [text, done] : q.objectives) if (!done) return text;
            return {};
        };
        if (before == questsSeen.end()) {
            if (id == 0) continue;
            Json banner{{"eyebrow", "Quest started"}, {"title", Phosphor::Live::CleanName(q.text, false)}};
            if (auto open = firstOpen(); !open.empty()) banner["objectives"] = Json::array({Json{{"t", open}}});
            banners.push_back(std::move(banner));
            continue;
        }
        const auto wasDone = [&](const std::string& text) -> int {
            for (const auto& [t, d] : before->second.objectives) if (t == text) return d ? 1 : 0;
            return -1;  // new objective
        };
        bool allDone = !q.objectives.empty(), allDoneBefore = !before->second.objectives.empty();
        for (const auto& [t, d] : before->second.objectives) allDoneBefore = allDoneBefore && d;
        for (const auto& [text, done] : q.objectives) {
            allDone = allDone && done;
            const int was = wasDone(text);
            if (was == 0 && done) lines.push_back(Json{{"text", text}, {"sub", "Objective completed · " + Phosphor::Live::CleanName(q.text, false)}, {"kind", "quest"}});
            else if (was == -1 && !done) lines.push_back(Json{{"text", text}, {"sub", id == 0 ? std::string("Miscellaneous") : Phosphor::Live::CleanName(q.text, false)}, {"kind", "quest"}});
        }
        if (id != 0 && allDone && !allDoneBefore) {
            Json last = Json::array();
            if (!q.objectives.empty()) last.push_back(Json{{"t", q.objectives.back().first}, {"done", true}});
            banners.push_back(Json{{"eyebrow", "Quest completed"}, {"title", Phosphor::Live::CleanName(q.text, false)}, {"objectives", last}, {"tone", "good"}});
        }
    }
    questsSeen = std::move(now);
    // hudUpdate takes one quest banner and one feed line per message; extra ones go out as their own messages.
    for (std::size_t i = 0; i < banners.size(); ++i) {
        if (i == 0 && !message.contains("quest")) message["quest"] = banners[i];
        else Send(Json{{"quest", banners[i]}});
    }
    for (std::size_t i = 0; i < lines.size(); ++i) {
        if (i == 0 && !message.contains("notify")) message["notify"] = lines[i];
        else Send(Json{{"notify", lines[i]}});
    }
}
int lastStealthRaw = -9999;

// Vanilla HUD text mirrored into Phosphor: prompt/subtitle/tutorial follow vanilla's show and hide; each new
// on-screen message posts once to the feed.
std::vector<std::string> shownMessages;
bool messagesBaseline = false;
void HudTextFields(Json& current, Json& message, const Phosphor::Live::HudText& t) {
    if (!t.ok) return;
    if (t.rolloverShown) {
        Json keys = Json::array();
        for (const auto& [key, label] : t.keys) {
            Json k{{"text", Phosphor::Live::CleanName(label, false)}};
            if (!key.empty()) k["key"] = key;
            keys.push_back(std::move(k));
        }
        Json p{{"name", Phosphor::Live::CleanName(t.rolloverName, false)}, {"keys", keys}};
        if (t.crime) p["crime"] = true;
        current["prompt"] = std::move(p);
    } else current["prompt"] = nullptr;
    current["subtitle"] = t.subtitleShown ? Json{{"who", t.who}, {"line", t.line}, {"ms", 60000}} : Json(nullptr);
    current["message"] = t.tutorialShown ? Json{{"text", t.tutorial}, {"ms", 60000}} : Json(nullptr);
    std::vector<std::string> fresh;
    for (const auto& m : t.messages)
        if (std::find(shownMessages.begin(), shownMessages.end(), m) == shownMessages.end()) fresh.push_back(m);
    shownMessages = t.messages;
    if (!messagesBaseline) { messagesBaseline = true; return; }
    const auto has = [](std::string text, std::string_view part) {
        for (auto& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return text.find(part) != std::string::npos;
    };
    bool first = true;
    for (const auto& text : fresh) {
        // CHALLENGES' completion message becomes Phosphor's challenge banner; KARMA's and REPUTATION's own
        // gain/loss messages are left out because Phosphor posts those changes itself.
        if (auto done = Phosphor::Mods::ChallengeFromMessage(text)) { Send(Json{{"challengeComplete", *done}}); continue; }
        Phosphor::Live::NoteSurvivalMessage(text);  // still shown in the feed; it also sets the need's stage
        if (Phosphor::Mods::HasKarma() && has(text, "karma")) continue;
        if (Phosphor::Mods::HasReputation() && (has(text, "fame") || has(text, "infamy"))) continue;
        Json line{{"text", Phosphor::Live::CleanName(text, false)}, {"kind", "item"}};
        if (first && !message.contains("notify")) message["notify"] = std::move(line);
        else Send(Json{{"notify", line}});
        first = false;
    }
}

// Mod and deeper-game fields that ride along with the slow tick.
void ModFields(Json& current, const Phosphor::Pipboy::Snapshot& pip, int enemies) {
    if (auto karma = Phosphor::Mods::ReadKarma(); !karma.is_null()) current["karma"] = std::move(karma);
    // Only sent while the armor plates mod is loaded: the HUD builds its armor core from the first one.
    if (Phosphor::Mods::HasArmorPlates()) current["armor"] = Phosphor::Mods::ReadArmorPlates();
    if (auto challenges = Phosphor::Mods::ReadChallenges(pip); !challenges.is_null()) current["challenges"] = std::move(challenges);
    current["settlementAttack"] = Phosphor::Mods::ReadAttack(pip, enemies);
    current["gear"] = Phosphor::Live::ReadGear();
    // The local reputation chip: send the faction's standing first (setReputation keeps it), then the name.
    Json status;
    const auto faction = Phosphor::Mods::LocalFaction(&status);
    if (!localFactionBaseline || faction != localFactionSeen) {
        if (faction) Send(Json{{"reputation", status}});
        localFactionSeen = faction;
        localFactionBaseline = true;
    }
    current["faction"] = faction ? Json(*faction) : Json(nullptr);
}
// Events that go out as their own messages: reputation changes, legendary drops, companion reactions.
void ModEvents(const Json& current) {
    for (auto& e : Phosphor::Mods::ReputationEvents()) { Send(Json{{"reputation", e}}); lastRepEvent = Clock::now(); }
    if (current.contains("party")) {
        const bool baseline = !affinitySeen.empty();
        std::map<std::string, float> now;
        for (const auto& m : current["party"]) {
            if (!m.contains("affinity") || !m["affinity"].is_number()) continue;
            const auto name = m.value("name", std::string{});
            const float value = m["affinity"].get<float>();
            now[name] = value;
            const auto before = affinitySeen.find(name);
            if (!baseline || before == affinitySeen.end()) continue;
            const float d = value - before->second;
            if (std::abs(d) < 5) continue;
            const char* kind = d >= 30 ? "loved" : d > 0 ? "liked" : d <= -30 ? "hated" : "disliked";
            // Before the party update in this tick's message: the HUD nudges its own copy, then gets the exact value.
            Send(Json{{"companionReact", Json{{"name", name}, {"kind", kind}}}});
            spdlog::info("Companion {} affinity {:.0f} -> {:.0f} ({})", name, before->second, value, kind);
        }
        // Keep companions who just stepped out of the high-process list so they don't re-baseline.
        for (const auto& [name, value] : affinitySeen) now.try_emplace(name, value);
        affinitySeen = std::move(now);
    }
}
void StopLive() {
    Phosphor::Live::ResetNativeHud();
    Phosphor::EngineQuests::Reset();
    liveStarted = false;
    liveShown = false;
    delta.Clear();
    Phosphor::Mods::ResetSession();
    Phosphor::Mines::Reset();
    Phosphor::Live::ResetCompass();
    localFactionSeen.reset();
    localFactionBaseline = false;
    affinitySeen.clear();
    karmaSeen.reset();
    targetSeen.clear();
    targetMutationsSeen = 0;
}

// No exception may unwind across the game/framework callback boundary.
void Guard(const std::function<void()>& task) noexcept {
    try { task(); }
    catch (const std::exception& e) { spdlog::error("Phosphor task failed: {}", e.what()); }
    catch (...) { spdlog::error("Phosphor task failed with an unknown exception"); }
}
bool Queue(std::function<void()> task) {
    if (!api) return false;
    // At kGameDataReady PrismaUI may not have verified the window thread yet.
    // Its dispatcher can reject startup work. F4SE's task queue exists already
    // and drains on the game thread once frame processing starts (as in Gunsmith).
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
    // Messages go to PhosphorHUD/messages.jsonl only, so the main log keeps a whole session of diagnostics.
    if (messageLog) messageLog->debug("{}", serialized);
}

Phosphor::Ini ReadIni(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    return Phosphor::ReadMainIni(file);
}
void SendSettings() {
    const auto defaults = ReadIni("Data/MCM/Config/PhosphorHUD/settings.ini");
    const auto overrides = ReadIni("Data/MCM/Settings/PhosphorHUD.ini");
#if defined(PHOSPHOR_AE)
    Phosphor::Ini condition;
    if (Phosphor::AE::Enabled("bModData")) {
    condition = ReadIni("Data/MCM/Config/ConditionBoy/settings.ini");
#else
    auto condition = ReadIni("Data/MCM/Config/ConditionBoy/settings.ini");
#endif
    for (const auto& [key, value] : ReadIni("Data/MCM/Settings/ConditionBoy.ini")) {
        if (key != "iCharacterType" || value == 0 || value == 1) condition[key] = value;
    }
#if defined(PHOSPHOR_AE)
    }
#endif
    std::optional<int> character;
    if (const auto it = condition.find("iCharacterType"); it != condition.end()) character = it->second;
    auto settings = Phosphor::MakeSettings(defaults, overrides, character);
    {
        std::ifstream keybinds("Data/MCM/Settings/Keybinds.json", std::ios::binary);
        const auto key = keybinds ? Phosphor::FindKeybind(keybinds, "PhosphorHUD", "PerkChoicesKey") : std::nullopt;
        Phosphor::QuickLootFirst::Configure(Phosphor::QuickLootFirstEnabled(defaults, overrides),
                                            key ? key->code : 0, key ? key->modifiers : 0);
    }
    dropMarkers = settings.value("legendaryDropMarkers", true);
    if (settings.contains("contextRules") && settings["contextRules"].is_object())
        dialogueRule = settings["contextRules"].value("dialogue", std::string("recommended"));
    Send(Json{{"settings", settings}, {"perkKey", Phosphor::Mods::PerkMenuKey()}});
    maxEffectSeconds = 3600.0F * Phosphor::EffectHourLimit(defaults, overrides);
    effectsKey.clear();  // resend the tray with the new limit
    settingsDirty = false;
    spdlog::info("MCM settings refreshed");
}

void CreateView();
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
    // Examples deliberately change persistent context/effects/companions.
    // A fresh DOM prevents those synthetic values leaking into the next replay.
    if (dataReady) CreateView();
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
    RestoreNativeHud(); // Restore on the UI queue; never touch Flash from this task.
    messagesBaseline = false;
    HideReplay();
}

void ConsoleMessage(PrismaView source, PRISMA_UI_API::ConsoleMessageLevel level, const char* message) noexcept {
    // Console callbacks may run on a render thread. Logging is thread-safe.
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
    // Guard real game screens independently of the replay's synthetic context examples.
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
    // The page itself remains awaiting a snapshot until the replay provides it.
    api->Show(g_view);
}

// Game hours between two readings, wrapped across midnight.
double HourGap(double a, double b) {
    double d = std::fmod(a - b, 24.0);
    if (d > 12) d -= 24;
    if (d < -12) d += 24;
    return d;
}

// Quest markers, objectives, map locations and the custom waypoint come from the engine's own data: it works without
// the Pip-Boy App option (and on AE, where the game no longer starts that service), and indoors it points at the exit
// door like vanilla, where the feed only has world positions. The feed is the fallback (bEngineQuestData=0, or the
// engine read fails).
Phosphor::Pipboy::Snapshot QuestData() {
#if defined(PHOSPHOR_AE)
    if (!Phosphor::AE::Enabled("bCompass") || !Phosphor::AE::Enabled("bEngineQuests"))
        return Phosphor::Pipboy::Latest();
#endif
    if (options.engineQuests) {
        static Phosphor::Pipboy::Snapshot cached;
        static Clock::time_point cachedAt{};
        const auto now = Clock::now();
        if (!cached.connected || now - cachedAt > std::chrono::milliseconds(200)) {
            cached = Phosphor::EngineQuests::Read();
            cachedAt = now;
        }
        if (cached.connected) return cached;
    }
    return Phosphor::Pipboy::Latest();
}

void LiveTick() {
    Phosphor::QuickLootFirst::Maintain();
    const auto now = Clock::now();
    const bool blocked = BlockingMenu();
    if (!liveStarted) {
#if defined(PHOSPHOR_AE)
        Phosphor::AE::ResetObservations();
#endif
        auto snap = Phosphor::Live::ReadSlow(raw);
        if (!raw.valid) return;
        snap.update(Phosphor::Live::ReadFast(raw));
        snap.update(Phosphor::Live::ReadVitals(raw));
        snap["loot"] = LootNow();
        Json startAllies = Json::array();
        snap["party"] = Phosphor::Live::ReadParty(&startAllies);
        snap.update(Phosphor::Live::ReadSurvival());
        snap["weight"] = Phosphor::Live::ReadWeight();
        snap["target"] = Phosphor::Live::ReadTarget(lastHitId.load(), NowMs() - lastHitAt.load() < 6000);
        snap["stealth"] = Phosphor::Live::ReadStealth(nullptr);
        questBaseline = false;
        const auto pip = QuestData();
        snap.update(Phosphor::Live::ReadQuests(pip, options.locationMeters));
        auto enemies = Phosphor::Live::ReadEnemies();
        const int enemyCount = static_cast<int>(enemies.size());
        for (auto& enemy : enemies) snap["markers"].push_back(std::move(enemy));
        for (auto& ally : startAllies) snap["markers"].push_back(std::move(ally));
        ModFields(snap, pip, enemyCount);
        Phosphor::Mods::ReputationEvents();  // baseline
        for (const auto& m : snap["party"])
            if (m.contains("affinity") && m["affinity"].is_number()) affinitySeen[m.value("name", std::string{})] = m["affinity"].get<float>();
        if (snap.contains("karma")) karmaSeen = snap["karma"].value("value", 0.0);
        delta.Clear();
        delta.Changes(snap);
        snap["effects"] = Phosphor::Live::ReadEffects(effectsKey, maxEffectSeconds);
        lastEffects = now;
        snap["snapshot"] = true;
        snap["paused"] = raw.menuOpen;
        snap["time"] = Json{{"hour", std::round(raw.hour * 1000) / 1000}, {"timescale", raw.timescale}};
        snap["mines"] = Json::array();
        Send(Json{{"reset", true}});
        if (localFactionSeen) {
            Json status;
            if (Phosphor::Mods::LocalFaction(&status)) Send(Json{{"reputation", status}});
        }
#if defined(PHOSPHOR_AE)
        Phosphor::Live::LogAeSelfCheck(snap, pip);
#endif
        Send(snap);
        illnesses = Phosphor::Live::ReadIllnesses(raw.timescale);
        for (const auto& [id, left] : illnesses) {
            Json ill{{"id", id}, {"quiet", true}};
            if (left >= 0) ill["hoursLeft"] = std::round(left * 10) / 10;
            Send(Json{{"illness", ill}});
            spdlog::info("Illness on load: {} ({:.1f} game hours left)", id, left);
        }
        predHour = raw.hour;
        predScale = raw.timescale;
        pausedSent = raw.menuOpen;
        lastLevel = raw.level;
        lastExp = raw.experience;
        radiation.Reset(snap.value("rads", 0), now);
        spdlog::info("Radiation baseline: Value {:.3f}, Permanent {:.3f}, HUD {}",
                     raw.radsValue, raw.radsPermanent, snap.value("rads", 0));
        lastSlow = lastTick = now;
        liveStarted = true;
        liveStartedAt = now;
        liveShown = false;
        SetSuppressed(!raw.powerArmor, QuestData().connected);
        spdlog::info("Live HUD started: level {}, experience {:.1f} -> xp {:.3f}, perks {}, ammo in inventory {}, clip {}/{}",
                     raw.level, raw.experience, snap.value("xp", 0.0), raw.perks, raw.ammoInInventory,
                     snap.value("clip", 0), snap.value("clipMax", 0));
    }
    if (liveShown == blocked) {
        liveShown = !blocked;
        if (liveShown) api->Show(g_view); else api->Hide(g_view);
    }
    if (blocked && now - lastTick < std::chrono::milliseconds(250)) return;

    const double dt = std::chrono::duration<double>(now - lastTick).count();
    lastTick = now;
    if (!raw.menuOpen && predHour >= 0) predHour = std::fmod(predHour + dt * predScale / 3600.0, 24.0);

    Json current = Phosphor::Live::ReadVitals(raw);
    const bool slow = now - lastSlow >= std::chrono::milliseconds(250);
    if (slow) {
        current.update(Phosphor::Live::ReadSlow(raw));
        const auto pip = QuestData();
        current.update(Phosphor::Live::ReadQuests(pip, options.locationMeters));
        auto enemies = Phosphor::Live::ReadEnemies();
        const int enemyCount = static_cast<int>(enemies.size());
        for (auto& enemy : enemies) current["markers"].push_back(std::move(enemy));
        Json allies = Json::array();
        current["party"] = Phosphor::Live::ReadParty(&allies);
        for (auto& ally : allies) current["markers"].push_back(std::move(ally));
        static Clock::time_point lastCompare{};
        if (options.engineQuests && now - liveStartedAt > std::chrono::seconds(10) && now - lastCompare > std::chrono::seconds(15)) {
            lastCompare = now;
            if (const auto feed = Phosphor::Pipboy::Latest(); feed.connected) Phosphor::EngineQuests::Compare(pip, feed);
        }
        if (!pipNoticeSent && !pip.connected && now - liveStartedAt > std::chrono::seconds(20)
#if defined(PHOSPHOR_AE)
            && Phosphor::AE::Enabled("bPipboyFeed")
#endif
        ) {
            pipNoticeSent = true;
#if defined(PHOSPHOR_AE)
            const char* sub = "AE needs the Pip-Boy Companion App Enabler (AE) mod for quest and location markers";
#else
            const char* sub = "Turn on Settings > Gameplay > Pip-Boy App for quest and location markers";
#endif
            Send(Json{{"notify", Json{{"text", "Compass markers unavailable"}, {"sub", sub}, {"kind", "bad"}, {"ms", 15000}}}});
            spdlog::warn("Pip-Boy feed still unreachable 20 s after the HUD started; showed the compass notice");
        }
        ModFields(current, pip, enemyCount);
        for (auto& m : Phosphor::Mods::LegendaryLootEvents(dropMarkers)) Send(m);
        ModEvents(current);
        current.update(Phosphor::Live::ReadSurvival());
        current["weight"] = Phosphor::Live::ReadWeight();
        int stealthRaw = 0;
        current["stealth"] = Phosphor::Live::ReadStealth(&stealthRaw);
        if (current["stealth"].value("sneaking", false) && std::abs(stealthRaw - lastStealthRaw) >= 10) {
            spdlog::debug("Detection level against player: {}", stealthRaw);
            lastStealthRaw = stealthRaw;
        }
        lastSlow = now;
    }
    current.update(Phosphor::Live::ReadFast(raw));
    if (!raw.valid) return;
    current["loot"] = LootNow();  // every tick: the selection moves as the player scrolls
    current["target"] = Phosphor::Live::ReadTarget(lastHitId.load(), NowMs() - lastHitAt.load() < 6000);
    const auto nativeHud = Phosphor::Live::LatestNativeHud();
    current["crosshair"] = nativeHud.crosshair;
    current["flashlight"] = nativeHud.flashlight;
    std::vector<std::string> exploded;
    current["mines"] = Phosphor::Mines::Read(&exploded);
    Json feedLines = Json::object();
    HudTextFields(current, feedLines, nativeHud.text);
    Json message = delta.Changes(current);
    if (feedLines.contains("notify") && !message.contains("notify")) message["notify"] = feedLines["notify"];
    if (!exploded.empty()) message["mineExploded"] = exploded.size() == 1 ? Json(exploded.front()) : Json(exploded);
    if (message.contains("karma") && message["karma"].is_object()) {
        const double value = message["karma"].value("value", 0.0);
        if (karmaSeen && std::abs(value - *karmaSeen) >= 1) {
            message["karma"]["change"] = std::lround(value - *karmaSeen);
            lastKarmaChange = Clock::now();
        }
        karmaSeen = value;
    }
    // A new mutation on the same enemy: targetMutated plays the HUD's mutation moment.
    if (current.contains("target")) {
        const auto& t = current["target"];
        const std::string name = t.is_object() ? t.value("name", std::string{}) : std::string{};
        const std::size_t count = t.is_object() && t.contains("mutations") ? t["mutations"].size() : 0;
        if (!name.empty() && name == targetSeen && count > targetMutationsSeen)
            message["targetMutated"] = t["mutations"].back();
        if (!name.empty() || t.is_null()) { targetSeen = name; targetMutationsSeen = count; }
    }
    if (slow) {
        if (current.contains("rads")) {
            // Events bypass Delta: equal consecutive gains must still pop, and a delayed
            // gain must carry the exact total even if that total was already sent.
            const auto exposure = radiation.Update(current["rads"].get<int>(), now);
            if (!exposure.empty()) {
                message.update(exposure);
                spdlog::info("Radiation exposure: Value {:.3f}, Permanent {:.3f}, HUD {}, gain {}",
                             raw.radsValue, raw.radsPermanent, current["rads"].get<int>(), exposure["radsGain"].get<int>());
            }
        }
        if (raw.menuOpen != pausedSent) {
            message["paused"] = raw.menuOpen;
            pausedSent = raw.menuOpen;
        }
        // The HUD runs its own clock; resend when it would be more than ~2 game minutes off.
        if (std::abs(HourGap(raw.hour, predHour)) > 0.035 || std::abs(raw.timescale - predScale) > 0.01F) {
            message["time"] = Json{{"hour", std::round(raw.hour * 1000) / 1000}, {"timescale", raw.timescale}};
            predHour = raw.hour;
            predScale = raw.timescale;
        }
        if (raw.level != lastLevel || std::abs(raw.experience - lastExp) >= 1.0F) {
            message.update(Phosphor::ProgressEvents(lastLevel, lastExp, raw.level, raw.experience));
            spdlog::debug("XP: level {} -> {}, experience {:.1f} -> {:.1f}", lastLevel, raw.level, lastExp, raw.experience);
            lastLevel = raw.level;
            lastExp = raw.experience;
        }
        // The tray counts down by itself: resend when the set changes, and every 5 s to stay in step.
        std::string key;
        auto effects = Phosphor::Live::ReadEffects(key, maxEffectSeconds);
        // Something eaten or drunk resets a need the game's messages had set.
        for (const auto& e : effects) {
            const auto kind = e.value("kind", std::string{});
            const auto id = e.value("id", std::string{});
            if (effectsKey.find(id) != std::string::npos) continue;
            if (kind == "food") Phosphor::Live::ClearSurvivalMessage("food");
            if (kind == "drink") Phosphor::Live::ClearSurvivalMessage("water");
        }
        if (key != effectsKey || now - lastEffects >= std::chrono::seconds(5)) {
            message["effects"] = std::move(effects);
            effectsKey = key;
            lastEffects = now;
        }
        // Illnesses: one message per change (caught / cured); the HUD counts them down itself.
        const auto now_ill = Phosphor::Live::ReadIllnesses(raw.timescale);
        for (const auto& [id, left] : now_ill) {
            if (illnesses.contains(id)) continue;
            Json ill{{"id", id}};
            if (left >= 0) ill["hoursLeft"] = std::round(left * 10) / 10;
            Send(Json{{"illness", ill}});
            spdlog::info("Illness caught: {} ({:.1f} game hours left)", id, left);
        }
        for (const auto& [id, left] : illnesses)
            if (!now_ill.contains(id)) { Send(Json{{"cure", id}}); spdlog::info("Illness cured: {}", id); }
        illnesses = now_ill;
        const auto pip = QuestData();
        QuestEvents(pip, message);
        SetSuppressed(!raw.powerArmor, pip.connected);
    }
    if (!message.empty()) Send(message);
    // Only copied inputs/results cross into the Flash UI queue. Game objects stay on this task queue.
    const bool hide = options.hideVanilla && !raw.powerArmor;
    auto* uiNow = RE::UI::GetSingleton();
    const bool dialogueOpen = uiNow && uiNow->GetMenuOpen(RE::BSFixedString("DialogueMenu"));
    Phosphor::Live::RequestNativeHud({true, api && api->IsUsingGamepad(), hide && options.hideText,
                                    hide && options.hideCrosshair && liveShown, hide && options.hideFlashlight && liveShown,
                                    hide && options.hideWidgets && !((options.keepWidgetMask >> 4) & 1),
                                    Phosphor::KeepGameSubtitles(dialogueOpen, dialogueRule)});
    nativeHudActive = true;
    nativeRestorePending = false;
}

// Unattended save-load test, only while Data/F4SE/Plugins/PhosphorHUD/autotest.ini sets iInGameLoads > 0:
// load the most recent save from the main menu, then in game open and close the pause menu and load again,
// iInGameLoads times, logging each step; then quit to desktop. Lets the crash-on-load fix be checked without input.
struct AutoTest {
    bool checked = false;
    int wanted = 0, done = 0, phase = 0, step = 0;
    std::uint64_t gen = 0;
    Clock::time_point at{};
} autotest;
void AutoTestQueueLoad(const char* what) {
    auto* manager = RE::BGSSaveLoadManager::GetSingleton();
    if (!manager) { spdlog::warn("Autotest: no save/load manager"); return; }
    autotest.gen = generation.load();
    spdlog::info("Autotest: {} - loading the most recent save", what);
    manager->QueueSaveLoadTask(RE::BGSSaveLoadManager::QUEUED_TASK::kLoadMostRecentSave);
}
void AutoTestTick() {
    if (!autotest.checked) {
        autotest.checked = true;
        const auto ini = ReadIni("Data/F4SE/Plugins/PhosphorHUD/autotest.ini");
        if (const auto it = ini.find("iInGameLoads"); it != ini.end()) autotest.wanted = std::clamp(it->second, 0, 20);
        if (autotest.wanted > 0) spdlog::info("Autotest: on, {} in-game loads planned", autotest.wanted);
    }
    if (autotest.wanted <= 0 || autotest.phase >= 6) return;
    auto* ui = RE::UI::GetSingleton();
    if (!ui) return;
    const auto now = Clock::now();
    const bool mainMenu = ui->GetMenuOpen(RE::BSFixedString("MainMenu"));
    const bool loading = ui->GetMenuOpen(RE::BSFixedString("LoadingMenu")) || ui->GetMenuOpen(RE::BSFixedString("FaderMenu"));
    using Clk = Clock::time_point;
    switch (autotest.phase) {
    case 0: {  // at the main menu. A load only goes through once the "press any button" screen is dismissed,
               // so keep asking every 30 s until one starts.
        if (session) {  // a load went through: run once only, so a normal play session isn't taken over next time
            std::ofstream("Data/F4SE/Plugins/PhosphorHUD/autotest.ini", std::ios::trunc) << "[Main]\niInGameLoads=0\n";
            autotest.phase = 1; autotest.at = {};
            break;
        }
        if (ui->GetMenuOpen(RE::BSFixedString("LoadingMenu")) || !mainMenu) break;
        if (autotest.at == Clk{}) { autotest.at = now + std::chrono::seconds(20); break; }
        if (now < autotest.at) break;
        ++autotest.step;
        AutoTestQueueLoad(std::format("main menu, attempt {}", autotest.step).c_str());
        autotest.at = now + std::chrono::seconds(30);
        break;
    }
    case 1:  // wait until the new session is live in game, then play for a while
        if (generation.load() == autotest.gen || !session || !liveStarted || loading || mainMenu) { autotest.at = {}; break; }
        if (autotest.at == Clk{}) {
            spdlog::info("Autotest: in game after load {} of {}", autotest.done, autotest.wanted);
            autotest.at = now + std::chrono::seconds(45);
            break;
        }
        if (now >= autotest.at) {
            if (autotest.done >= autotest.wanted) {
                spdlog::info("Autotest: PASSED - {} in-game loads, no crash", autotest.done);
                autotest.phase = 5; autotest.at = now + std::chrono::seconds(5);
                break;
            }
            if (auto* queue = RE::UIMessageQueue::GetSingleton()) queue->AddMessage(RE::BSFixedString("PauseMenu"), RE::UI_MESSAGE_TYPE::kShow);
            spdlog::info("Autotest: pause menu opened");
            autotest.phase = 2; autotest.at = now + std::chrono::seconds(4);
        }
        break;
    case 2:  // close the pause menu (the unpause path)
        if (now < autotest.at) break;
        if (auto* queue = RE::UIMessageQueue::GetSingleton()) queue->AddMessage(RE::BSFixedString("PauseMenu"), RE::UI_MESSAGE_TYPE::kHide);
        spdlog::info("Autotest: pause menu closed");
        autotest.phase = 3; autotest.at = now + std::chrono::seconds(5);
        break;
    case 3:  // load again from inside the game
        if (now < autotest.at) break;
        ++autotest.done;
        AutoTestQueueLoad(std::format("in-game load {} of {}", autotest.done, autotest.wanted).c_str());
        autotest.phase = 1; autotest.at = {};
        break;
    case 5:  // done: quit to desktop
        if (now < autotest.at) break;
        spdlog::info("Autotest: quitting to desktop");
        if (auto* main = RE::Main::GetSingleton()) main->quitGame = true;
        autotest.phase = 6;
        break;
    default: break;
    }
}

void Tick() {
    if (!session || !options.live || displaying || !domReady) {
        if (nativeHudActive) RestoreNativeHud();
        if (nativeRestorePending) {
            if (Phosphor::Live::LatestNativeHud().visibilityApplied) nativeRestorePending = false;
            else Phosphor::Live::RequestNativeHud({});
        }
    }
    // Stop retaining engine references while live consumers are unavailable (replay, loading, disabled HUD).
    const bool capture = dataReady && session && options.live && !displaying && domReady && ViewValid();
    Phosphor::Mods::SetEventCapture(capture);
    Phosphor::Mines::SetEventCapture(capture);
    if (!dataReady) return;
    AutoTestTick();
    if (!ViewValid()) {
        displaying = false;
        replay.Stop();
        domReady = false;
        CreateView();
        return;
    }
    if (!domReady) return;
    const auto* ui = RE::UI::GetSingleton();
    if (ui && ui->GetMenuOpen(RE::BSFixedString("MainMenu")) && session) EndSession();
    if (settingsDirty) SendSettings();
    if (toggleRequested.exchange(false)) ToggleReplay();
    if (!session) return;
    if (probeRequested.exchange(false) && liveStarted && options.engineProbe)
        Phosphor::EngineProbe::Run(Phosphor::Pipboy::Latest(), "Ctrl+F11");
    if (!displaying) {
        if (options.live) LiveTick();
        return;
    }
    const bool blocked = BlockingMenu();
    if (blocked != realMenuPaused) {
        realMenuPaused = blocked;
        Send(Json{{"paused", blocked || replayPaused}});
        if (blocked) api->Hide(g_view); else api->Show(g_view);
    }
    if (blocked) return;
    if (auto message = replay.Next(Clock::now())) {
        if (message->contains("paused") && (*message)["paused"].is_boolean()) replayPaused = (*message)["paused"];
        Send(*message);
        if (!replay.Active()) spdlog::info("Replay complete; Ctrl+F10 hides the preview");
    }
}

void Poll(std::stop_token stop) {
    bool wasDown = false, probeWasDown = false;
    timeBeginPeriod(1);
    while (!stop.stop_requested()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        if (stop.stop_requested()) break;
        DWORD foregroundPID = 0;
        GetWindowThreadProcessId(GetForegroundWindow(), &foregroundPID);
        const bool down = foregroundPID == GetCurrentProcessId() &&
                          (GetAsyncKeyState(VK_CONTROL) & 0x8000) && (GetAsyncKeyState(VK_F10) & 0x8000);
        if (down && !wasDown) toggleRequested.store(true);
        wasDown = down;
        const bool probeDown = foregroundPID == GetCurrentProcessId() &&
                               (GetAsyncKeyState(VK_CONTROL) & 0x8000) && (GetAsyncKeyState(VK_F11) & 0x8000);
        if (probeDown && !probeWasDown) probeRequested.store(true);
        probeWasDown = probeDown;
        Phosphor::QuickLootFirst::Update(foregroundPID == GetCurrentProcessId());
        // At most one timer task can be queued, even if the game stops draining.
        if (!tickQueued.exchange(true)) {
            const auto epoch = generation.load();
            if (!Queue([epoch] {
                Guard([epoch] { if (epoch == generation.load()) Tick(); });
                tickQueued.store(false);
            })) tickQueued.store(false);
        }
    }
    timeEndPeriod(1);
}

class MenuWatch final : public RE::BSTEventSink<RE::MenuOpenCloseEvent> {
    RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent& event,
                                         RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override {
        Guard([&] {
            const std::string name(event.menuName.c_str());
            const bool opening = event.opening;
            // Loading screens and fades stop the task queue that normally hides the view, so hide it right here
            // (menu events arrive on the game's main thread). The tick shows it again once the menu has closed.
            if (opening && name == "LoadingMenu") {
                Phosphor::Live::ResetNativeHud();
                // Stop collecting game events for the length of the load; the tick turns capture back on.
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
                        if (auto* hits = HitEventSource()) hits->RegisterSink(&hitWatch);
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
            generation.fetch_add(1); // Invalidate queued work immediately, before the game-thread reset.
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

namespace {
// F4SE co-save: survival need stages that came from the game's stage messages (see Live::SurvivalMessageStages).
constexpr std::uint32_t kCoSaveId = 0x50485544;      // 'PHUD'
constexpr std::uint32_t kSurvivalRecord = 0x53555256; // 'SURV'
constexpr std::uint32_t kSurvivalVersion = 1;
void F4SEAPI OnCoSaveRevert(const F4SE::SerializationInterface*) {
    Phosphor::Live::SetSurvivalMessageStages({-1, -1, -1});
}
void F4SEAPI OnCoSaveSave(const F4SE::SerializationInterface* serialization) {
    const auto stages = Phosphor::Live::SurvivalMessageStages();
    const std::int32_t data[3] = {stages[0], stages[1], stages[2]};
    if (!serialization->WriteRecord(kSurvivalRecord, kSurvivalVersion, data, sizeof(data)))
        spdlog::warn("Co-save: could not write survival stages");
}
void F4SEAPI OnCoSaveLoad(const F4SE::SerializationInterface* serialization) {
    std::uint32_t type = 0, version = 0, length = 0;
    while (serialization->GetNextRecordInfo(type, version, length)) {
        if (type != kSurvivalRecord || version != kSurvivalVersion || length != 3 * sizeof(std::int32_t)) continue;
        std::int32_t data[3] = {-1, -1, -1};
        if (serialization->ReadRecordData(data, sizeof(data)) != sizeof(data)) continue;
        Phosphor::Live::SetSurvivalMessageStages({data[0], data[1], data[2]});
        const auto stages = Phosphor::Live::SurvivalMessageStages();
        spdlog::info("Co-save: survival message stages restored (food {}, water {}, rest {})", stages[0], stages[1], stages[2]);
    }
}
}  // namespace

extern "C" __declspec(dllexport) constinit auto F4SEPlugin_Version = []() noexcept {
    F4SE::PluginVersionData version{};
#if defined(PHOSPHOR_AE)
    version.PluginVersion({0, 9, 24, 0});
#else
    version.PluginVersion({0, 9, 24, 0});
#endif
    version.PluginName("PhosphorHUD");
    version.AuthorName("Phosphor HUD");
    version.UsesAddressLibrary(true);
    version.UsesSigScanning(false);
    version.HasNoStructUse(false);
#if defined(PHOSPHOR_AE)
    // Force the exact whitelist; the broad AE layout flag bypasses it in F4SE.
    version.IsLayoutDependent(false);
    version.CompatibleVersions({F4SE::RUNTIME_1_11_240});
    version.MinimumRequiredXSEVersion({0, 7, 9, 0});
#else
    version.IsLayoutDependent(true);
    version.CompatibleVersions({F4SE::RUNTIME_1_10_163});
#endif
    return version;
}();

extern "C" __declspec(dllexport) bool F4SEAPI F4SEPlugin_Query(
    const F4SE::QueryInterface* f4se, F4SE::PluginInfo* info) {
    if (!f4se || !info) return false;
    info->infoVersion = F4SE::PluginInfo::kVersion;
    info->name = "PhosphorHUD";
    info->version = 4;
#if defined(PHOSPHOR_AE)
    return !f4se->IsEditor() && Phosphor::AE::RuntimeAllowed(f4se->RuntimeVersion());
#else
    return !f4se->IsEditor() && f4se->RuntimeVersion() == F4SE::RUNTIME_1_10_163;
#endif
}

extern "C" __declspec(dllexport) bool F4SEAPI F4SEPlugin_Load(const F4SE::LoadInterface* f4se) {
#if defined(PHOSPHOR_AE)
    if (!f4se || f4se->IsEditor()) return false;
#else
    if (!f4se || f4se->IsEditor() || f4se->RuntimeVersion() != F4SE::RUNTIME_1_10_163) return false;
#endif
    try {
        std::filesystem::create_directories("Data/F4SE/Plugins/PhosphorHUD");
        auto logger = spdlog::rotating_logger_mt("PhosphorHUD", "Data/F4SE/Plugins/PhosphorHUD.log", 2 * 1024 * 1024, 2);
        logger->set_level(spdlog::level::debug);
        logger->flush_on(spdlog::level::debug);
        spdlog::set_default_logger(logger);
#if defined(PHOSPHOR_AE)
        if (!Phosphor::AE::RuntimeAllowed(f4se->RuntimeVersion())) {
            spdlog::error("unsupported runtime {}: AE experimental requires 1.11.240.0; staying inert", f4se->RuntimeVersion().string());
            return false; // Before F4SE initialization, relocations or engine access.
        }
        Phosphor::AE::runtime = f4se->RuntimeVersion().string();
        Phosphor::AE::f4seVersion = f4se->F4SEVersion().string();
        Phosphor::AE::Initialize();
#endif
        F4SE::Init(f4se, false);
        const auto* messaging = F4SE::GetMessagingInterface();
        if (!messaging || !messaging->RegisterListener(HandleMessage)) return false;
        if (const auto* serialization = F4SE::GetSerializationInterface()) {
            serialization->SetUniqueID(kCoSaveId);
            serialization->SetRevertCallback(OnCoSaveRevert);
            serialization->SetSaveCallback(OnCoSaveSave);
            serialization->SetLoadCallback(OnCoSaveLoad);
        } else spdlog::warn("Co-save unavailable: survival stages from game messages will not survive a save load");
#if defined(PHOSPHOR_AE)
        spdlog::info("Phosphor HUD 0.9.24-ae1 EXPERIMENTAL built for AE 1.11.240; in-game behavior UNVERIFIED");
#else
        spdlog::info("Phosphor HUD 0.9.24 loaded for OG 1.10.163; live HUD");
#endif
        return true;
    } catch (const std::exception& e) {
        OutputDebugStringA(e.what());
        return false;
    }
}
