#pragma once
#include <array>
#include "Core.h"
#include "Pipboy.h"
#include <functional>
#include <map>
#include <utility>
#include <vector>
#include <string>
#include <string_view>

namespace Phosphor::Live {

struct Raw {
    bool valid = false;
    int level = 0;
    float experience = 0;
    int perks = 0;
    bool powerArmor = false;
    bool menuOpen = false;
    float hour = 0;
    float timescale = 20;
    int ammoInInventory = -1;
    float radsValue = 0, radsPermanent = 0;
};

Json ReadFast(Raw& raw);
Json ReadVitals(Raw& raw);
Json ReadSlow(Raw& raw);
Json ReadContext(Raw& raw);
bool IsAds();
bool IsSneaking();
Json ReadEffects(std::string& key, float maxSeconds);
std::string CleanName(std::string name, bool cutPipes);
double Heading();
void ResetCompass();
std::string Untranslate(std::string text);
Json ReadQuests(const Pipboy::Snapshot& pip, double locationRangeMeters = 100);
Json ReadLoot(const std::function<std::string(const char*)>& keyFor, std::string* log);
Json ReadEnemies();
void LogCompassSettings();
std::map<std::string, float> ReadIllnesses(float timescale);
Json ReadParty(Json* allyMarkers = nullptr);
Json ReadSurvival();
bool NoteSurvivalMessage(const std::string& text);
void ClearSurvivalMessage(const char* need);
std::array<int, 3> SurvivalMessageStages();
void SetSurvivalMessageStages(const std::array<int, 3>& stages);
Json ReadGear();
Json ReadWeight();
Json ReadTarget(std::uint32_t lastHitId, bool recentHit);
Json ReadStealth(int* raw);
struct HudText {
    bool ok = false;
    bool subtitleShown = false;
    std::string who, line;
    bool tutorialShown = false;
    std::string tutorial;
    std::vector<std::string> messages;
    bool rolloverShown = false;
    std::string rolloverName;
    bool crime = false;
    std::vector<std::pair<std::string, std::string>> keys;
};
struct NativeHudSnapshot {
    HudText text;
    Json crosshair = nullptr;
    bool flashlight = false, hidden = false, visibilityApplied = false;
};
struct NativeHudRequest {
    bool capture = false, gamepad = false;
    bool hideText = false, hideCrosshair = false, hideFlashlight = false;
    bool hideCompass = false;
    bool keepSubtitles = false;
};
NativeHudSnapshot LatestNativeHud();
void RequestNativeHud(NativeHudRequest request);
void ResetNativeHud();
void SetNativeHudLoading(bool loading);
void LogXpSettings();
#if defined(PHOSPHOR_AE)
void LogAeSelfCheck(const Json& snapshot, const Pipboy::Snapshot& pip);
#endif
std::string ToUtf8(std::string_view text);
}
