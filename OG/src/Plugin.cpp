#include "PluginState.h"

namespace Phosphor::Plugin {
API* api = nullptr;
bool liveStarted = false, liveShown = false;
bool nativeHudActive = false, nativeRestorePending = false;
PluginOptions options;
std::string effectsKey;
float maxEffectSeconds = 2 * 3600.0F;
bool dropMarkers = true;
std::string dialogueRule = "recommended";
namespace {
std::atomic<bool> tickQueued{false};
bool pausedSent = false;
Phosphor::Delta delta;
Phosphor::RadiationEvents radiation;
Phosphor::Live::Raw raw;
Clock::time_point lastSlow{}, lastTick{};
Clock::time_point liveStartedAt{};
bool pipNoticeSent = false;
std::atomic<bool> probeRequested{false};
double predHour = -1;
float predScale = 20;
int lastLevel = 0;
float lastExp = 0;
std::map<std::string, std::string> keyCache;
Clock::time_point keyCacheAt{};
std::string lastLootId;
std::map<std::string, float> illnesses;
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
Clock::time_point lastEffects{};
std::optional<double> karmaSeen;
std::string targetSeen;
std::size_t targetMutationsSeen = 0;
Clock::time_point lastKarmaChange{};
int lastStealthRaw = -9999;
}
Clock::time_point lastRepEvent{};

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

namespace {
double HourGap(double a, double b) {
    double d = std::fmod(a - b, 24.0);
    if (d > 12) d -= 24;
    if (d < -12) d += 24;
    return d;
}

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
        Phosphor::Mods::ReputationEvents();
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
    current["loot"] = LootNow();
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
        std::string key;
        auto effects = Phosphor::Live::ReadEffects(key, maxEffectSeconds);
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

void Tick() {
    if (!session || !options.live || displaying || !domReady) {
        if (nativeHudActive) RestoreNativeHud();
        if (nativeRestorePending) {
            if (Phosphor::Live::LatestNativeHud().visibilityApplied) nativeRestorePending = false;
            else Phosphor::Live::RequestNativeHud({});
        }
    }
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
}

namespace {
constexpr std::uint32_t kCoSaveId = 0x50485544;
constexpr std::uint32_t kSurvivalRecord = 0x53555256;
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
}

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
            return false;
        }
        Phosphor::AE::runtime = f4se->RuntimeVersion().string();
        Phosphor::AE::f4seVersion = f4se->F4SEVersion().string();
        Phosphor::AE::Initialize();
#endif
        F4SE::Init(f4se, false);
        const auto* messaging = F4SE::GetMessagingInterface();
        if (!messaging || !messaging->RegisterListener(Phosphor::Plugin::HandleMessage)) return false;
        if (const auto* serialization = F4SE::GetSerializationInterface()) {
            serialization->SetUniqueID(kCoSaveId);
            serialization->SetRevertCallback(OnCoSaveRevert);
            serialization->SetSaveCallback(OnCoSaveSave);
            serialization->SetLoadCallback(OnCoSaveLoad);
        } else spdlog::warn("Co-save unavailable: survival stages from game messages will not survive a save load");
#if defined(PHOSPHOR_AE)
        spdlog::info("Phosphor HUD 0.9.25-ae1 EXPERIMENTAL built for AE 1.11.240; in-game behavior UNVERIFIED");
#else
        spdlog::info("Phosphor HUD 0.9.25 loaded for OG 1.10.163; live HUD");
#endif
        return true;
    } catch (const std::exception& e) {
        OutputDebugStringA(e.what());
        return false;
    }
}
