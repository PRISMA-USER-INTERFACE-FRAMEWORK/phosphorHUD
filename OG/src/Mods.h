#pragma once
// Data from other mods and the deeper game systems: KARMA, REPUTATION, CHALLENGES, Immersive Legendary
// Mutations, Legendary Effect Overhaul slots, settlement attacks, survival adrenaline, companion affinity and
// LevelUpMenuEx's hotkey. Forms are looked up by plugin + local form id (from each mod's own plugin), so a mod
// that isn't installed simply reads as absent. Everything here runs on the game thread unless noted.
#include "Core.h"
#include "Pipboy.h"
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace RE {
class Actor;
class ExtraDataList;
}

namespace Phosphor::Mods {

// Looks up every form once game data is loaded and logs which mods were found.
void Resolve();
// A save was loaded (or the session ended): drop baselines so nothing fires for the load itself.
void ResetSession();
// Disabling live mode releases queued references and stops accepting events (any thread).
void SetEventCapture(bool enabled);

// Companion affinity (vanilla CA_Affinity), or nullopt when the actor has none.
std::optional<float> Affinity(RE::Actor& actor);
// Logs the companion's FollowerState actor value when it changes (to work out how "waiting" reads).
void LogFollowerState(RE::Actor& actor, const std::string& name);
// The player's companions: vanilla CurrentCompanionFaction (Fallout4.esm 023C01), Liga's team faction
// (Liga.esp 0B80A1, minus companions sent home) or the commanded-actor flag.
bool IsCompanion(RE::Actor& actor);

// Legendary effect names on an item stack: object mods in the legendary attach points
// (vanilla ap_Legendary, LEO's ap_Legendary_2..5). In slot order, at most 5.
std::vector<std::string> LegendaryEffects(const RE::ExtraDataList* extra);
// Immersive Legendary Mutations on an actor: [{name, kind}] (kind offense/defense/heal), oldest first.
Json Mutations(RE::Actor& actor);

// KARMA (Nexus 80425): {value, alignment, title}, or null when KARMA isn't loaded.
Json ReadKarma();
// Warzone - Armor Plates System (Nexus, LuBuCake): true while its esp is loaded. Then ReadArmorPlates() gives
// {current, max, perPlate, stock}: armor now, the armor pool, armor per plate and the plates carried.
bool HasArmorPlates();
Json ReadArmorPlates();
// REPUTATION (Nexus 79394): one message body per change since the last call (discovered, fame/infamy
// gained/lost). The first call after a load only records the baseline.
std::vector<Json> ReputationEvents();
// REPUTATION: the faction that owns where the player is (the mod's location lists, walking up parent
// locations), or nullopt. With *status set to that faction's {faction, title, fame, infamy}.
std::optional<std::string> LocalFaction(Json* status);
// CHALLENGES (Nexus 76347): the tracked challenges, from the mod's "Tracked Challenges" quest objectives in
// the Pip-Boy feed. Null when the mod isn't loaded or the feed is down.
Json ReadChallenges(const Pipboy::Snapshot& pip);
// Form id of CHALLENGES' display quest (0 if absent); the quest list skips it.
std::uint32_t ChallengeQuestId();
// "Challenge completed: X." (the mod's completion message, mirrored off the vanilla HUD) -> the
// challengeComplete body {id, name}; nullopt for any other text.
std::optional<Json> ChallengeFromMessage(const std::string& text);

// Settlement attack, from a "Defend <settlement>" objective in the Pip-Boy feed (SKK Settlement Attack
// System and the vanilla/Minutemen attack quests both phrase it that way). `enemyCount` is the number of
// hostile actors in combat around the player. Returns the settlementAttack message value: an object (with
// b/dist while it's running, which the HUD turns into its "defend" compass marker) or null.
Json ReadAttack(const Pipboy::Snapshot& pip, int enemyCount);

// Survival adrenaline: {rank, kills[, perRank]}. Kills are counted from the player's kills since the rank
// last changed.
Json ReadAdrenaline();
// Legendary drops, as whole messages: {"legendaryLoot": dropped/picked}, and while `markers` is on, compass
// marker moves ({"marker": {id "loot:<id>", b, dist}}) and removals ({"markerRemove": ...}) for drops still
// lying in loaded corpses. The HUD owns those markers; they survive its "markers" list updates.
std::vector<Json> LegendaryLootEvents(bool markers);
// True while KARMA / REPUTATION are loaded (their own HUD messages are then left to Phosphor's feed).
bool HasKarma();
bool HasReputation();
// Registers the death event sink (kills for adrenaline, legendary deaths for drops). Idempotent.
void RegisterSinks();

// True while the player is in an NAF scene (NAF_InScene) or an AAF/NAF Bridge scene (Bridge_ActorBusy).
bool PlayerInScene();
// LevelUpMenuEx's "Open LevelUpMenu" hotkey from MCM's Keybinds.json, or "" when it isn't bound.
std::string PerkMenuKey();
}
