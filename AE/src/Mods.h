#pragma once
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

void Resolve();
void ResetSession();
void SetEventCapture(bool enabled);

std::optional<float> Affinity(RE::Actor& actor);
void LogFollowerState(RE::Actor& actor, const std::string& name);
bool IsCompanion(RE::Actor& actor);

std::vector<std::string> LegendaryEffects(const RE::ExtraDataList* extra);
Json Mutations(RE::Actor& actor);

Json ReadKarma();
bool HasArmorPlates();
Json ReadArmorPlates();
std::vector<Json> ReputationEvents();
std::optional<std::string> LocalFaction(Json* status);
Json ReadChallenges(const Pipboy::Snapshot& pip);
std::uint32_t ChallengeQuestId();
std::optional<Json> ChallengeFromMessage(const std::string& text);

Json ReadAttack(const Pipboy::Snapshot& pip, int enemyCount);

Json ReadAdrenaline();
std::vector<Json> LegendaryLootEvents(bool markers);
bool HasKarma();
bool HasReputation();
void RegisterSinks();

bool PlayerInScene();
std::string PerkMenuKey();
}
