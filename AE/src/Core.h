#pragma once
#include <nlohmann/json.hpp>
#include <chrono>
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
// Quest markers that sit on the same door (within 50 world units) are drawn as one compass marker. For each marker,
// the index of the marker that leads its group: itself for a leader and for every marker that is not on a door.
struct MarkerSpot { double x = 0, y = 0; bool door = false; };
std::vector<std::size_t> MergeDoorMarkers(const std::vector<MarkerSpot>& spots);
// Label of a merged door marker: the objective itself for one, "Door · N objectives" for several.
std::string DoorGroupLabel(const std::string& single, std::size_t count);
Ini ReadMainIni(std::istream& input);
// MCM iMaxEffectHours: effects longer than this many hours stay off the tray (0 = show all). Default 2.
int EffectHourLimit(const Ini& defaults, const Ini& overrides);
Json MakeSettings(const Ini& defaults, const Ini& overrides,
                  std::optional<int> conditionCharacter = std::nullopt);
// MCM bQuickLootFirst: perk activation choices on containers and dead bodies wait for the perk-choices key. Default on.
bool QuickLootFirstEnabled(const Ini& defaults, const Ini& overrides);
// The game's own subtitle stays visible during dialogue unless the dialogue rule shows Phosphor's subtitles
// (`contextRules.dialogue` "show"): then Phosphor shows them and the game's stays hidden, so lines never double.
bool KeepGameSubtitles(bool dialogueOpen, std::string_view dialogueRule);
// An MCM hotkey from Data/MCM/Settings/Keybinds.json: Windows virtual-key code and MCM modifiers (1 Shift, 2 Ctrl,
// 4 Alt). Nothing when the key is unbound or the file has no usable entry.
struct Keybind { int code = 0; int modifiers = 0; };
std::optional<Keybind> FindKeybind(std::istream& input, std::string_view modName, std::string_view id);

struct NeedStage { const char* need; std::string_view name; int stage; };
// Most specific names first: shared by message and active-effect recognition.
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
    bool primed_ = false; // true once a snapshot (or hp) line has gone out
};
// FO4's level curve: level L -> L+1 costs 75L + 125 XP; reaching L takes (L - 1)(37.5L + 125) in total.
float XpForLevel(int level);
// Progress toward the next level. Takes a running total; a value below the level's
// threshold is read as XP earned within the level instead.
float XpFraction(int level, float experience);
// xpGain / levelUp for a change between two reads; empty when nothing was gained.
Json ProgressEvents(int levelBefore, float xpBefore, int levelNow, float xpNow);

// Absolute radiation plus a popup event, batched at most once a second.
// Reset on each snapshot so loading a save never looks like radiation exposure.
class RadiationEvents {
public:
    using Clock = std::chrono::steady_clock;
    void Reset(int rads, Clock::time_point now);
    Json Update(int rads, Clock::time_point now);
private:
    int last_ = 0, pending_ = 0;
    Clock::time_point sent_{};
};

// Remembers what was sent and returns only the fields that changed.
class Delta {
public:
    Json Changes(const Json& now);
    void Clear() { last_ = Json::object(); }
private:
    Json last_ = Json::object();
};
}
