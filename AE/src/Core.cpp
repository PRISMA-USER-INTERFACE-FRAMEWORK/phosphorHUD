#include "Core.h"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <numbers>
#include <cctype>
#include <string_view>
#include <valijson/adapters/nlohmann_json_adapter.hpp>
#include <valijson/schema.hpp>
#include <valijson/schema_parser.hpp>
#include <valijson/validator.hpp>
namespace Phosphor {
std::optional<CustomWaypoint> ReadCustomWaypoint(std::optional<double> x, std::optional<double> y, bool visible) {
    if (!visible || !x || !y || !std::isfinite(*x) || !std::isfinite(*y)) return std::nullopt;
    return CustomWaypoint{*x, *y};
}
Json CustomWaypointMarker(const std::optional<CustomWaypoint>& waypoint, double playerX, double playerY) {
    if (!waypoint || !std::isfinite(playerX) || !std::isfinite(playerY)) return nullptr;
    const double dx = waypoint->x - playerX, dy = waypoint->y - playerY;
    const double distance = std::hypot(dx, dy) / 70.0;
    if (!std::isfinite(distance) || distance > 40000.0) return nullptr;
    const double bearing = std::fmod(std::atan2(dx, dy) * 180.0 / std::numbers::pi + 360.0, 360.0);
    return Json{{"id", "player-waypoint"}, {"t", "pin"}, {"b", std::round(bearing * 2) / 2},
                {"dist", std::lround(distance)}, {"label", "Custom waypoint"}, {"active", true}};
}
std::vector<std::size_t> MergeDoorMarkers(const std::vector<MarkerSpot>& spots) {
    std::vector<std::size_t> leader(spots.size());
    for (std::size_t i = 0; i < spots.size(); ++i) {
        leader[i] = i;
        if (!spots[i].door) continue;
        for (std::size_t j = 0; j < i; ++j) {
            if (leader[j] != j || !spots[j].door) continue;
            if (std::hypot(spots[i].x - spots[j].x, spots[i].y - spots[j].y) <= 50.0) { leader[i] = j; break; }
        }
    }
    return leader;
}
std::string DoorGroupLabel(const std::string& single, std::size_t count) {
    return count <= 1 ? single : "Door \u00b7 " + std::to_string(count) + " objectives";
}
namespace {
std::string_view Trim(std::string_view text) {
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == text.npos) return {};
    return text.substr(first, text.find_last_not_of(" \t\r\n") - first + 1);
}
int Setting(const Ini& defaults, const Ini& overrides, const char* key, int fallback, int max) {
    for (const auto* source : {&defaults, &overrides}) {
        const auto it = source->find(key);
        if (it != source->end() && it->second >= 0 && it->second <= max) fallback = it->second;
    }
    return fallback;
}
}
Ini ReadMainIni(std::istream& input) {
    Ini result;
    bool main = false;
    std::string line;
    while (std::getline(input, line)) {
        std::string_view text(line);
        if (text.starts_with("\xef\xbb\xbf")) text.remove_prefix(3);
        text = Trim(text.substr(0, text.find_first_of(";#")));
        if (text.empty()) continue;
        if (text.front() == '[') { main = text == "[Main]"; continue; }
        if (!main) continue;
        const auto eq = text.find('=');
        if (eq == text.npos) continue;
        const auto key = Trim(text.substr(0, eq)), value = Trim(text.substr(eq + 1));
        if (key.empty() || value.empty()) continue;
        int number = 0;
        const auto parsed = std::from_chars(value.data(), value.data() + value.size(), number);
        if (parsed.ec == std::errc{} && parsed.ptr == value.data() + value.size()) {
            result[std::string(key)] = number;
        }
    }
    return result;
}
bool KeepGameSubtitles(bool dialogueOpen, std::string_view dialogueRule) { return dialogueOpen && dialogueRule != "show"; }
bool QuickLootFirstEnabled(const Ini& defaults, const Ini& overrides) {
    return Setting(defaults, overrides, "bQuickLootFirst", 1, 1) != 0;
}
std::optional<Keybind> FindKeybind(std::istream& input, std::string_view modName, std::string_view id) {
    Json root;
    try { root = Json::parse(input); } catch (...) { return std::nullopt; }
    if (!root.is_object() || !root.contains("keybinds") || !root["keybinds"].is_array()) return std::nullopt;
    for (const auto& bind : root["keybinds"]) {
        if (!bind.is_object()) continue;
        const auto mod = bind.find("modName"), name = bind.find("id"), code = bind.find("keycode");
        if (mod == bind.end() || name == bind.end() || code == bind.end()) continue;
        if (!mod->is_string() || !name->is_string() || !code->is_number_integer()) continue;
        if (mod->get<std::string>() != modName || name->get<std::string>() != id) continue;
        const int key = code->get<int>();
        if (key <= 0 || key > 0xFE) return std::nullopt;
        int modifiers = 0;
        if (const auto m = bind.find("modifiers"); m != bind.end() && m->is_number_integer()) modifiers = m->get<int>() & 7;
        return Keybind{key, modifiers};
    }
    return std::nullopt;
}
int EffectHourLimit(const Ini& defaults, const Ini& overrides) { return Setting(defaults, overrides, "iMaxEffectHours", 2, 48); }
std::optional<NeedStage> ParseSurvivalMessage(std::string_view text) {
    std::string lower(text);
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    const auto has = [&](std::string_view part) { return lower.find(part) != std::string::npos; };
    // Observed after drinking directly from world water: no new alchemy effect is required.
    if (Trim(lower) == "you are properly hydrated." || Trim(lower) == "you are properly hydrated")
        return NeedStage{"water", "properly hydrated", 0};
    if (Trim(lower) == "you are properly fed." || Trim(lower) == "you are properly fed")
        return NeedStage{"food", "properly fed", 0};
    if (!has("you are") && !has("you're") && !has("you feel")) return std::nullopt;
    for (const auto& stage : kNeedStages) if (has(stage.name)) return stage;
    return std::nullopt;
}
Json MakeSettings(const Ini& defaults, const Ini& overrides, std::optional<int> character) {
    const auto get = [&](const char* key, int fallback, int max) { return Setting(defaults, overrides, key, fallback, max); };
    const char* figures[] = {"silhouette", "silhouette", "vaultboy", "vaultgirl"};
    const auto figure = get("iLimbFigure", 0, 3);
    Json settings{{"limbFigure", figure == 0 && character && (*character == 0 || *character == 1)
                                    ? (*character == 0 ? "vaultboy" : "vaultgirl") : figures[figure]}};
    struct Boolean { const char* ini; const char* hud; int fallback; };
    constexpr Boolean booleans[] = {
        {"bHideHealthyFigure", "hideHealthyFigure", 1}, {"bShowArmorPieces", "armorPieces", 1},
        {"bShowAdrenaline", "showAdrenaline", 1}, {"bNeedStageNames", "needStageNames", 1},
        {"bHideSatisfiedNeeds", "hideSatisfiedNeeds", 0},
        {"bHideFullVitals", "hideFullVitals", 0},
        {"bLocationOnArrival", "locationOnArrival", 0},
        {"bShowUndiscovered", "showUndiscovered", 1},
        {"bHideThirdPersonAimCrosshair", "hideThirdPersonAimCrosshair", 0},
        {"bShowCrosshair", "showCrosshair", 1}, {"bShowHitMarkers", "showHitMarkers", 1},
        {"bSurvivalEnemyMarkers", "survivalEnemyMarkers", 0}, {"bMineWarnings", "mineWarnings", 1},
        {"bMineCompass", "mineCompass", 1}, {"bKarmaReadout", "karmaReadout", 1},
        {"bLocalReputation", "localReputation", 1}, {"bChallengeTracker", "challengeTracker", 1},
        {"bLegendaryDropMarkers", "legendaryDropMarkers", 1}, {"bShowWeaponLegendary", "showWeaponLegendary", 1}, {"bLevelUpBanner", "levelUpBanner", 1},
        {"bShowArmorWheel", "showArmorWheel", 1},
        {"bPerkReminder", "perkReminder", 1}, {"bShowLevelBar", "showLevelBar", 1},
        {"iClockFormat", "clock24", 0}
    };
    for (const auto& setting : booleans) settings[setting.hud] = get(setting.ini, setting.fallback, 1) != 0;
    constexpr std::pair<const char*, const char*> contexts[] = {
        {"iCtxAds", "ads"}, {"iCtxScope", "scope"}, {"iCtxVats", "vats"}, {"iCtxDialogue", "dialogue"},
        {"iCtxWheel", "wheel"}, {"iCtxWorkshop", "workshop"}, {"iCtxPowerArmor", "powerarmor"}, {"iCtxScene", "scene"}
    };
    const char* rules[] = {"recommended", "show", "hide"};
    for (const auto& [ini, hud] : contexts) settings["contextRules"][hud] = rules[get(ini, 0, 2)];
    constexpr std::pair<const char*, const char*> calm[] = {
        {"iCalmVitals", "vitals"}, {"iCalmCompass", "compass"}, {"iCalmWeapon", "kit"},
        {"iCalmCompanions", "party"}, {"iCalmQuests", "tracker"}, {"iCalmEffects", "effects"},
        {"iCalmCrosshair", "cross"}
    };
    const char* calmRules[] = {"show", "dim", "hide"};
    for (const auto& [ini, hud] : calm) settings["calmRules"][hud] = calmRules[get(ini, 0, 2)];
    settings["calmDelay"] = get("iCalmDelay", 5, 15);
    const char* weights[] = {"near", "always", "never"};
    const char* mutations[] = {"bar", "feed", "off"};
    const char* readability[] = {"off", "subtle", "strong"};
    settings["textReadability"] = readability[get("iTextReadability", 0, 2)];
    settings["showWeight"] = weights[get("iShowWeight", 0, 2)];
    settings["mutationNotices"] = mutations[get("iMutationNotices", 0, 2)];
    settings["idleOpacity"] = std::max(20, get("iIdleOpacity", 85, 100)) / 100.0;
    // Accessibility: HUD text size 80-150%, compass icon size 80-200%, ring size 80-150% (MCM Look).
    settings["textScale"] = std::max(80, get("iTextScale", 100, 150)) / 100.0;
    settings["iconScale"] = std::max(80, get("iIconScale", 100, 200)) / 100.0;
    settings["ringScale"] = std::max(80, get("iRingScale", 100, 150)) / 100.0;  // vital/effect rings, companion portraits
    return settings;
}
ReplayFile ReadReplay(std::istream& input, const Json& schemaJson) {
    ReplayFile result;
    valijson::Schema schema;
    try {
        valijson::SchemaParser parser;
        parser.populateSchema(valijson::adapters::NlohmannJsonAdapter(schemaJson), schema);
    } catch (const std::exception& e) {
        result.errors.push_back(std::string("invalid replay schema: ") + e.what());
        return result;
    }
    valijson::Validator validator(valijson::Validator::kStrongTypes);
    std::string line;
    std::size_t lineNumber = 0;
    while (std::getline(input, line)) {
        ++lineNumber;
        std::string_view text(line);
        if (lineNumber == 1 && text.starts_with("\xef\xbb\xbf")) text.remove_prefix(3);
        text = Trim(text);
        if (text.empty() || text.starts_with("//")) continue;
        try {
            auto message = Json::parse(text);
            if (!message.is_object()) throw std::runtime_error("expected a JSON object");
            valijson::ValidationResults errors;
            if (!validator.validate(schema, valijson::adapters::NlohmannJsonAdapter(message), &errors)) {
                const auto& error = *errors.begin();
                throw std::runtime_error(error.jsonPointer + ": " + error.description);
            }
            result.messages.push_back(std::move(message));
        } catch (const std::exception& e) {
            result.errors.push_back("line " + std::to_string(lineNumber) + ": " + e.what());
        }
    }
    if (input.bad()) result.errors.push_back("I/O error while reading replay");
    if (result.messages.empty() && result.errors.empty()) result.errors.push_back("replay has no messages");
    if (!result.errors.empty()) result.messages.clear();
    return result;
}
void Replay::Start(std::vector<Json> messages, Clock::time_point now) {
    messages_ = std::move(messages);
    index_ = 0;
    next_ = now;
    primed_ = false;
}
void Replay::Stop() { messages_.clear(); index_ = 0; primed_ = false; }
bool Replay::Active() const { return index_ < messages_.size(); }
std::optional<Json> Replay::Next(Clock::time_point now) {
    if (!Active() || now < next_) return {};
    auto& message = messages_[index_++];
    // The HUD stays invisible until its first snapshot, so lead-in lines (settings, reset)
    // and that snapshot go out back to back; pacing starts after it.
    if (!primed_ && (message.contains("snapshot") || message.contains("hp"))) primed_ = true;
    next_ = primed_ ? now + std::chrono::seconds(1) : now;
    return message;
}
float XpForLevel(int level) {
    if (level <= 1) return 0.0F;
    // Level L -> L+1 costs 75L + 125 XP (200 for level 2), so the total is (L - 1)(37.5L + 125).
    return (level - 1.0F) * (37.5F * level + 125.0F);
}
float XpFraction(int level, float experience) {
    if (level < 1 || experience <= 0) return 0.0F;
    const float floor = XpForLevel(level), need = XpForLevel(level + 1) - floor;
    const float into = experience >= floor ? experience - floor : experience;
    return std::clamp(into / need, 0.0F, 1.0F);
}
Json ProgressEvents(int levelBefore, float xpBefore, int levelNow, float xpNow) {
    Json out = Json::object();
    if (levelBefore <= 0 || levelNow <= 0) return out;
    // Gain measured on the running total, so a level-up in between still counts.
    const auto total = [](int level, float xp) { return xp >= XpForLevel(level) ? xp : XpForLevel(level) + xp; };
    const float gain = total(levelNow, xpNow) - total(levelBefore, xpBefore);
    if (gain >= 1.0F) out["xpGain"] = static_cast<int>(gain + 0.5F);
    if (levelNow > levelBefore) out["levelUp"] = Json{{"from", levelBefore}, {"to", levelNow}};
    return out;
}
void RadiationEvents::Reset(int rads, Clock::time_point now) {
    last_ = rads;
    pending_ = 0;
    sent_ = now;
}
Json RadiationEvents::Update(int rads, Clock::time_point now) {
    pending_ += std::max(0, rads - last_);
    last_ = rads;
    if (!pending_ || now - sent_ < std::chrono::seconds(1)) return Json::object();
    Json out{{"rads", rads}, {"radsGain", pending_}};
    pending_ = 0;
    sent_ = now;
    return out;
}

Json Delta::Changes(const Json& now) {
    Json out = Json::object();
    for (const auto& [key, value] : now.items()) {
        const auto it = last_.find(key);
        if (it == last_.end() || *it != value) {
            out[key] = value;
            last_[key] = value;
        }
    }
    return out;
}
}
