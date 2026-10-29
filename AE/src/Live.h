#pragma once
// Game data is read on the game task queue. Native Flash is accessed only by the UI task bridge.
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
    // Values kept outside the HUD message, for events and verification logging.
    bool valid = false;
    int level = 0;
    float experience = 0;   // the Experience actor value, as read
    int perks = 0;          // PlayerCharacter::perkCount, clamped to >= 0
    bool powerArmor = false;
    bool menuOpen = false;  // a menu that pauses the HUD is open (MENU_PAUSE list)
    float hour = 0;         // GameHour
    float timescale = 20;
    int ammoInInventory = -1;
    float radsValue = 0, radsPermanent = 0; // diagnostic comparison with the Pip-Boy RAD number
};

// Fields worth reading every tick: heading, magazine, aim state.
Json ReadFast(Raw& raw);
// Everything else Stage 1 sends: vitals, level, location, combat, weapon, grenade, context.
Json ReadSlow(Raw& raw);
// Consumable effects on the player (food, drinks, chems, meds), one entry per item, longest timer wins.
// `key` lists the item ids so the caller can tell when the set changes.
// Effects whose total length is over maxSeconds are left out (0 = keep all); at most 10, soonest to end first.
Json ReadEffects(std::string& key, float maxSeconds);
// Display name without sorting tags: drops leading "[Pistol] (Aid) {x}" groups and, for weapons,
// everything after the first " | " (mod-name suffixes).
std::string CleanName(std::string name, bool cutPipes);
// Compass heading in degrees (0 = north, clockwise): the camera's view direction, the player's facing as fallback.
double Heading();
// Discard quest-marker height data across saves and HUD restarts.
void ResetCompass();
// "$TAKE" -> "Take": a Scaleform translation key that reached us untranslated, as readable text.
std::string Untranslate(std::string text);
// Compass quest markers and the quest tracker from the Pip-Boy feed, relative to where the player stands.
// Returns {"markers": [...], "tracker": {...}|null}.
Json ReadQuests(const Pipboy::Snapshot& pip, double locationRangeMeters = 100);
// The vanilla quick-loot box's data (the container under the crosshair), or null when it is closed.
// keyFor maps a ControlMap user event ("Activate", "ReadyWeapon", "Jump") to its key text.
// `log` gets one line per newly shown container (for checking the selection against vanilla).
Json ReadLoot(const std::function<std::string(const char*)>& keyFor, std::string* log);
// Compass markers for actors in combat and hostile to the player, within 150 m (like the vanilla compass).
Json ReadEnemies();
void LogCompassSettings();
// Survival illnesses on the player (spell effects named Infection, Parasites, ...): id -> game hours left,
// or -1 when the effect has no timer.
std::map<std::string, float> ReadIllnesses(float timescale);
// The player's companions: high-process actors carrying the commanded-actor flag (vanilla and Liga teams).
// Fills allyMarkers (when given) with one compass marker per companion at its real bearing.
Json ReadParty(Json* allyMarkers = nullptr);
// Survival: {"survival": {on, stages}, "needs": {...}} from the difficulty and the game's hunger/thirst/sleep
// effects (Peckish ... Starving, Parched ... Severely Dehydrated, Tired ... Incapacitated).
Json ReadSurvival();
// Survival stage messages from the game ("You are Peckish from lack of food.") set that need's stage until the
// effects say otherwise. "You are properly hydrated." resets thirst even without an item effect.
// "You are properly fed." likewise resets hunger; partial-meal warnings still keep their stage.
// ClearSurvivalMessage drops the fallback when the player eats/drinks/sleeps. True if it matched.
bool NoteSurvivalMessage(const std::string& text);
void ClearSurvivalMessage(const char* need);
// The message-derived stages (food, water, rest; -1 = none) for the F4SE co-save: the game only sends a stage
// message when a need changes, so each save keeps the stages it last saw and restores them on load.
std::array<int, 3> SurvivalMessageStages();
void SetSurvivalMessageStages(const std::array<int, 3>& stages);
// {clothed, armor:{head, torso, larm, rarm, lleg, rleg}} from the biped slots of equipped armor
// (33/36-40 clothing and under-armor, 41-45 armor pieces, 30/46 head).
Json ReadGear();
// {"cur", "max"}: carried weight and carry limit.
Json ReadWeight();
// Enemy bar: the game's aim-assist actor when it is hostile, in combat or was just hit; otherwise the actor
// the player last hit (lastHitId) while `recentHit`. Null when nothing qualifies.
Json ReadTarget(std::uint32_t lastHitId, bool recentHit);
// {"sneaking", "level"}: level is the game's highest detection level against the player (raw value in *raw).
Json ReadStealth(int* raw);
// Text the vanilla HUD movie is showing right now (HUDMenu.swf): subtitles, tutorial box, message list and the
// activation prompt. Read through Scaleform so no engine struct layouts are needed.
struct HudText {
    bool ok = false;
    bool subtitleShown = false;
    std::string who, line;
    bool tutorialShown = false;
    std::string tutorial;
    std::vector<std::string> messages;   // every message currently on screen
    bool rolloverShown = false;
    std::string rolloverName;
    bool crime = false;
    std::vector<std::pair<std::string, std::string>> keys;  // (key, label)
};
struct NativeHudSnapshot {
    HudText text;
    Json crosshair = nullptr;
    bool flashlight = false, hidden = false, visibilityApplied = false;
};
struct NativeHudRequest {
    bool capture = false, gamepad = false;
    bool hideText = false, hideCrosshair = false, hideFlashlight = false;
    bool hideCompass = false;  // shrink the vanilla compass clip (it keeps running; see ShrinkVanillaCompass)
    // With hideText: leave the game's own subtitle at full size (dialogue, where Phosphor's subtitles stand down and
    // the dialogue menu, e.g. Extended Dialogue Interface, positions the game's subtitle itself).
    bool keepSubtitles = false;
};
// The bridge stores plain copies, never GFx objects or movie pointers. No synchronous wait on the UI queue.
NativeHudSnapshot LatestNativeHud();
void RequestNativeHud(NativeHudRequest request);
void ResetNativeHud();
void SetNativeHudLoading(bool loading);
// Logs the XP-related game settings once so the curve can be checked against the game.
void LogXpSettings();
#if defined(PHOSPHOR_AE)
void LogAeSelfCheck(const Json& snapshot, const Pipboy::Snapshot& pip);
#endif
std::string ToUtf8(std::string_view text);
}
