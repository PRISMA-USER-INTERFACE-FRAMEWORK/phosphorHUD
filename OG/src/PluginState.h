#pragma once
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

namespace Phosphor::Plugin {
using Phosphor::Json;
using API = PRISMA_UI_API::IVPrismaUI12;
using Clock = Phosphor::Replay::Clock;

struct PluginOptions {
    bool live = true; bool hideVanilla = true; int locationMeters = 100;
    bool hideWidgets = true, hideText = true, hideCrosshair = true, hideFlashlight = true; int keepWidgetMask = 0;
    bool engineQuests = true;
    bool engineProbe = false;
    bool messageLog = false;
};

extern API* api;
extern PrismaView g_view;
extern bool domReady, dataReady, session, displaying;
extern bool realMenuPaused, replayPaused, settingsDirty;
extern bool liveStarted, liveShown;
extern bool nativeHudActive, nativeRestorePending;
extern std::atomic<bool> toggleRequested;
extern std::atomic<unsigned> generation;
extern Phosphor::Replay replay;
extern PluginOptions options;
extern std::atomic<std::uint32_t> lastHitId;
extern std::atomic<std::int64_t> lastHitAt;
extern bool questBaseline, messagesBaseline, localFactionBaseline;
extern std::optional<std::string> localFactionSeen;
extern std::map<std::string, float> affinitySeen;
extern std::string effectsKey;
extern float maxEffectSeconds;
extern bool dropMarkers;
extern std::string dialogueRule;
extern Clock::time_point lastRepEvent;

bool ViewValid();
void Send(const Json& message);
void Guard(const std::function<void()>& task) noexcept;
bool Queue(std::function<void()> task);
void SetSuppressed(bool on, bool quests = false);
void CreateView();
bool BlockingMenu();
void ToggleReplay();
void RestoreNativeHud();
void EndSession();
void HandleMessage(F4SE::MessagingInterface::Message* message);
void RegisterHitWatch();
std::int64_t NowMs();
void StopLive();
void Poll(std::stop_token stop);
void QuestEvents(const Phosphor::Pipboy::Snapshot& pip, Json& message);
void HudTextFields(Json& current, Json& message, const Phosphor::Live::HudText& t);
void ModFields(Json& current, const Phosphor::Pipboy::Snapshot& pip, int enemies);
void ModEvents(const Json& current);
Phosphor::Ini ReadIni(const std::filesystem::path& path);
void SendSettings();
void AutoTestTick();
}
