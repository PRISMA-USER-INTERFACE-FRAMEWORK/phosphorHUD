#pragma once
#include <nlohmann/json.hpp>
#include <chrono>
#include <cstdint>
#include <istream>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Phosphor {
using Json = nlohmann::json;
using Ini = std::map<std::string, int>;
struct CustomWaypoint { double x = 0, y = 0; };
std::optional<CustomWaypoint> ReadCustomWaypoint(std::optional<double> x, std::optional<double> y, bool visible);
Json CustomWaypointMarker(const std::optional<CustomWaypoint>& waypoint, double playerX, double playerY);
struct MarkerSpot { double x = 0, y = 0; bool door = false; };
std::vector<std::size_t> MergeDoorMarkers(const std::vector<MarkerSpot>& spots);
std::string DoorGroupLabel(const std::string& single, std::size_t count);
Ini ReadMainIni(std::istream& input);
int EffectHourLimit(const Ini& defaults, const Ini& overrides);
Json MakeSettings(const Ini& defaults, const Ini& overrides);
struct LimbFractions { float head = 1, torso = 1, larm = 1, rarm = 1, lleg = 1, rleg = 1; };
Json MakeLimbStatus(const LimbFractions& limbs, bool infected, const std::vector<std::string>& effectNames, std::uint32_t armorSlots = 0);
bool QuickLootFirstEnabled(const Ini& defaults, const Ini& overrides);
bool KeepGameSubtitles(bool dialogueOpen, std::string_view dialogueRule);
struct Keybind { int code = 0; int modifiers = 0; };
std::optional<Keybind> FindKeybind(std::istream& input, std::string_view modName, std::string_view id);

struct NeedStage { const char* need; std::string_view name; int stage; };
inline constexpr NeedStage kNeedStages[] = {
    {"food", "starving", 5}, {"food", "ravenous", 4}, {"food", "famished", 3}, {"food", "hungry", 2}, {"food", "peckish", 1},
    {"water", "severely dehydrated", 5}, {"water", "mildly dehydrated", 3}, {"water", "dehydrated", 4},
    {"water", "thirsty", 2}, {"water", "parched", 1},
    {"rest", "incapacitated", 5}, {"rest", "exhausted", 4}, {"rest", "weary", 3}, {"rest", "overtired", 2}, {"rest", "tired", 1}};
std::optional<NeedStage> ParseSurvivalMessage(std::string_view text);

struct ReplayFile {
    std::vector<Json> messages;
    std::vector<std::string> errors;
};
ReplayFile ReadReplay(std::istream& input, const Json& schema);

class Replay {
public:
    using Clock = std::chrono::steady_clock;
    void Start(std::vector<Json> messages, Clock::time_point now);
    void Stop();
    bool Active() const;
    std::optional<Json> Next(Clock::time_point now);
private:
    std::vector<Json> messages_;
    std::size_t index_ = 0;
    Clock::time_point next_{};
    bool primed_ = false;
};
float XpForLevel(int level);
float XpFraction(int level, float experience);
Json ProgressEvents(int levelBefore, float xpBefore, int levelNow, float xpNow);

class RadiationEvents {
public:
    using Clock = std::chrono::steady_clock;
    void Reset(int rads, Clock::time_point now);
    Json Update(int rads, Clock::time_point now);
private:
    int last_ = 0, pending_ = 0;
    Clock::time_point sent_{};
};

class Delta {
public:
    Json Changes(const Json& now);
    void Clear() { last_ = Json::object(); }
private:
    Json last_ = Json::object();
};
}
