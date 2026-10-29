#include "Mods.h"
#include "ModData.h"
#include "EventInbox.h"
#include "Live.h"
#include <F4SE/F4SE.h>
#include <RE/Fallout.h>
#include <Windows.h>
#include <spdlog/spdlog.h>
#if defined(PHOSPHOR_AE)
#include "../ae/AeRuntime.h"
#endif
#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <map>
#include <mutex>
#include <numbers>
#include <set>
#include <utility>

namespace Phosphor::Mods {
namespace {
using Clock = std::chrono::steady_clock;

std::string Name(const RE::TESForm* form) {
    return form ? Live::ToUtf8(RE::TESFullName::GetFullName(*form)) : std::string{};
}
std::string Lower(std::string_view text) {
    std::string out(text);
    for (auto& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}
bool Has(std::string_view text, std::string_view part) { return Lower(text).find(Lower(part)) != std::string::npos; }
bool StartsWith(std::string_view text, std::string_view prefix) {
    return text.size() >= prefix.size() && Lower(text.substr(0, prefix.size())) == Lower(prefix);
}
std::string Trim(std::string s) {
    const auto a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return {};
    return s.substr(a, s.find_last_not_of(" \t\r\n") - a + 1);
}
using ModData::Slug;
double Round1(double value, double step) { return std::round(value / step) * step; }

template <class T>
T* Lookup(std::uint32_t local, std::string_view plugin) {
    auto* data = RE::TESDataHandler::GetSingleton();
    return data ? data->LookupForm<T>(local, plugin) : nullptr;
}
const RE::TESFile* Plugin(std::string_view name) {
    auto* data = RE::TESDataHandler::GetSingleton();
    if (!data) return nullptr;
#if defined(PHOSPHOR_AE)
    if (const auto* file = data->LookupLoadedModByName(name)) return file;
    return data->LookupLoadedLightModByName(name);
#else
    const auto file = data->LookupLoadedFile(name);
    return file.first;
#endif
}
RE::ActorValueInfo* AvByEditorId(std::string_view edid) {
    auto* data = RE::TESDataHandler::GetSingleton();
    if (!data) return nullptr;
    for (auto* av : data->GetFormArray<RE::ActorValueInfo>()) {
        if (!av) continue;
        const char* id = av->GetFormEditorID();
        if (id && Lower(id) == Lower(edid)) return av;
    }
    return nullptr;
}
std::vector<RE::TESForm*> ListItems(RE::BGSListForm* list) {
    std::vector<RE::TESForm*> out;
    if (!list) return out;
    for (auto* form : list->arrayOfForms) out.push_back(form);
    return out;
}
float PlayerValue(RE::ActorValueInfo* av) {
    auto* player = RE::PlayerCharacter::GetSingleton();
    return player && av ? player->GetActorValue(*av) : 0.0F;
}
bool Inactive(const RE::ActiveEffect& effect) {
    using Flag = RE::ActiveEffect::Flags;
    return effect.flags.any(Flag::kInactive, Flag::kDispelled, Flag::kWornOff);
}
double Bearing(double dx, double dy) {
    double b = std::atan2(dx, dy) * 180.0 / std::numbers::pi;
    return b < 0 ? b + 360 : b;
}

// --- forms, looked up in Resolve() ---
struct Forms {
    RE::ActorValueInfo* affinity = nullptr;       // vanilla CA_Affinity
    RE::ActorValueInfo* followerState = nullptr;  // vanilla FollowerState (logged only)
    RE::TESFaction* currentCompanion = nullptr;   // vanilla CurrentCompanionFaction
    RE::TESFaction* ligaTeam = nullptr;           // Liga of my Companions: Team
    RE::TESFaction* ligaHome = nullptr;           // Liga: sent home
    // KARMA.esm
    RE::ActorValueInfo* karma = nullptr;
    std::array<RE::ActorValueInfo*, 5> karmaTitles{};  // very evil, evil, neutral, good, very good
    // Warzone - Armor Plates System.esp (LuBuCake): the armor pool globals and the plate item
    RE::TESGlobal* plateNow = nullptr;        // WZ_PlatesPlayer_Health (0x0F99): armor now
    RE::TESGlobal* plateMax = nullptr;        // WZ_PlatesPlayer_MaxHealth (0x0F9A): the armor pool
    RE::TESGlobal* platePer = nullptr;        // WZ_PlatesPlayer_HealthToAdd (0x0011): armor per plate
    RE::TESObjectMISC* plateItem = nullptr;   // WZ_PlatesSystem_Plates (0x0005): the plates carried
    // REPUTATION.esm
    std::vector<RE::ActorValueInfo*> repFame, repInfamy, repTitle;
    std::vector<RE::TESGlobal*> repShown;
    std::vector<std::string> repNames;
    std::map<const RE::BGSLocation*, std::size_t> repLocations;  // location -> faction index
    struct RepQuest { RE::TESQuest* quest; std::size_t faction; const char* event; };
    std::vector<RepQuest> repQuests;
    // CHALLENGES.esm
    RE::TESQuest* challengeQuest = nullptr;
    // ImmersiveLegendaryMutations.esp
    const RE::TESFile* mutations = nullptr;
    // animation scenes
    std::vector<RE::BGSKeyword*> sceneKeywords;
    // survival adrenaline
    std::vector<RE::BGSPerk*> adrenaline;
    int adrenalinePerRank = 0;
} forms;
bool resolved = false;

// --- death events (any thread) ---
// Only ids cross threads: a copied TESDeathEvent holds NiPointers that would keep actors alive across a save load.
struct DeathNote { std::uint32_t dying = 0; bool byPlayer = false; };
EventInbox<DeathNote> deathEvents{256};
int pendingKills = 0;
std::vector<std::uint32_t> pendingLegendaryDeaths;
class DeathWatch final : public RE::BSTEventSink<RE::TESDeathEvent> {
    RE::BSEventNotifyControl ProcessEvent(const RE::TESDeathEvent& event, RE::BSTEventSource<RE::TESDeathEvent>*) override {
#if defined(PHOSPHOR_AE)
        if (!AE::Enabled("bModData")) return RE::BSEventNotifyControl::kContinue;
#endif
        try {
            if (!event.dead || !event.actorDying) return RE::BSEventNotifyControl::kContinue;
            auto* player = RE::PlayerCharacter::GetSingleton();
            deathEvents.Push(DeathNote{event.actorDying->GetFormID(), player && event.actorKiller.get() == player});
        } catch (...) {}
        return RE::BSEventNotifyControl::kContinue;
    }
} deathWatch;
bool sinksRegistered = false;

void DrainDeaths() {
    auto batch = deathEvents.Take();
    if (batch.dropped) spdlog::warn("Death event queue full: skipped {} events", batch.dropped);
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) return;
    for (const auto& note : batch.events) {
        if (note.byPlayer) ++pendingKills;
        auto* dying = RE::TESForm::GetFormByID<RE::Actor>(note.dying);
        if (!dying || dying == player || dying->IsDeleted()) continue;
        const char* display = dying->GetDisplayFullName();
        if (display && Has(display, "legendary") && pendingLegendaryDeaths.size() < 16)
            pendingLegendaryDeaths.push_back(dying->GetFormID());
    }
}

// --- per-session state ---
struct RepSeen { float fame = 0, infamy = 0; bool shown = false; };
std::map<const RE::TESQuest*, bool> repQuestRunning;
std::vector<RepSeen> repSeen;
bool repBaseline = false;
std::map<std::uint32_t, float> followerStates;
int adrenalineRank = -1, adrenalineKills = 0;
struct Drop {
    std::uint32_t corpse = 0, object = 0;
    std::string id, item;
    std::vector<std::string> effects;
    double b = -1000;
    long dist = -1;  // last position sent to the HUD
};
struct Corpse { std::uint32_t id = 0; Clock::time_point since; bool found = false; };
std::vector<Corpse> corpses;
std::vector<Drop> drops;
struct AttackSeen { std::string name; std::string state; int total = 0; Clock::time_point endedAt; bool ended = false; };
std::optional<AttackSeen> attack;
}

void Resolve() {
#if defined(PHOSPHOR_AE)
    AE_GATE("bModData", );
#endif
    forms = Forms{};
#if defined(PHOSPHOR_AE)
    if (AE::Enabled("bCompanions")) {
#endif
    forms.affinity = AvByEditorId("CA_Affinity");
    forms.followerState = AvByEditorId("FollowerState");
    forms.currentCompanion = RE::TESForm::GetFormByID<RE::TESFaction>(0x00023C01);
    forms.ligaTeam = Lookup<RE::TESFaction>(0x0B80A1, "Liga.esp");
    forms.ligaHome = Lookup<RE::TESFaction>(0x010317, "Liga.esp");
    spdlog::info("Companion factions: CurrentCompanionFaction {}, Liga team {}, Liga home {}",
                 forms.currentCompanion != nullptr, forms.ligaTeam != nullptr, forms.ligaHome != nullptr);

#if defined(PHOSPHOR_AE)
    }
#endif
    forms.karma = Lookup<RE::ActorValueInfo>(0xF99, "KARMA.esm");
    // Armor plates: all four must resolve or the HUD's armor core is never sent (the mod isn't loaded).
    constexpr const char* kPlatesEsp = "Warzone - Armor Plates System.esp";
    forms.plateNow = Lookup<RE::TESGlobal>(0xF99, kPlatesEsp);
    forms.plateMax = Lookup<RE::TESGlobal>(0xF9A, kPlatesEsp);
    forms.platePer = Lookup<RE::TESGlobal>(0x011, kPlatesEsp);
    forms.plateItem = Lookup<RE::TESObjectMISC>(0x005, kPlatesEsp);
    spdlog::info("Armor plates: pool {}, per plate {}, plate item {}",
                 forms.plateNow && forms.plateMax && forms.platePer, forms.platePer != nullptr, forms.plateItem != nullptr);
    constexpr std::uint32_t kTitleAvs[] = {0x015, 0x016, 0x014, 0x017, 0x018};
    for (std::size_t i = 0; i < 5; ++i) forms.karmaTitles[i] = Lookup<RE::ActorValueInfo>(kTitleAvs[i], "KARMA.esm");

    const auto fame = ListItems(Lookup<RE::BGSListForm>(0x056, "REPUTATION.esm"));
    const auto infamy = ListItems(Lookup<RE::BGSListForm>(0x057, "REPUTATION.esm"));
    const auto titles = ListItems(Lookup<RE::BGSListForm>(0x001, "REPUTATION.esm"));
    const auto shown = ListItems(Lookup<RE::BGSListForm>(0x002, "REPUTATION.esm"));
    const std::size_t factions = std::min({fame.size(), infamy.size(), titles.size(), shown.size()});
    for (std::size_t i = 0; i < factions; ++i) {
        auto* f = fame[i] ? fame[i]->As<RE::ActorValueInfo>() : nullptr;
        auto* n = infamy[i] ? infamy[i]->As<RE::ActorValueInfo>() : nullptr;
        auto* t = titles[i] ? titles[i]->As<RE::ActorValueInfo>() : nullptr;
        auto* s = shown[i] ? shown[i]->As<RE::TESGlobal>() : nullptr;
        if (!f || !n || !t) continue;
        forms.repFame.push_back(f);
        forms.repInfamy.push_back(n);
        forms.repTitle.push_back(t);
        forms.repShown.push_back(s);
        forms.repNames.push_back(Name(t));  // the tab AV's name is the faction's display name
    }

    // Which faction owns which locations: REP_LocTypeList_All (REPUTATION.esm 147) holds one location list per
    // faction, in faction order.
    const auto lists = ListItems(Lookup<RE::BGSListForm>(0x147, "REPUTATION.esm"));
    std::vector<std::size_t> kept;  // faction index in `forms` for each original index
    {
        std::size_t k = 0;
        for (std::size_t i = 0; i < factions; ++i) {
            auto* f = fame[i] ? fame[i]->As<RE::ActorValueInfo>() : nullptr;
            auto* n = infamy[i] ? infamy[i]->As<RE::ActorValueInfo>() : nullptr;
            auto* t = titles[i] ? titles[i]->As<RE::ActorValueInfo>() : nullptr;
            kept.push_back(f && n && t ? k++ : SIZE_MAX);
        }
    }
    for (std::size_t i = 0; i < lists.size() && i < kept.size(); ++i) {
        auto* list = lists[i] ? lists[i]->As<RE::BGSListForm>() : nullptr;
        if (!list || kept[i] == SIZE_MAX) continue;
        list->ForEachForm([&](RE::TESForm* form) {
            if (auto* location = form->As<RE::BGSLocation>()) forms.repLocations.emplace(location, kept[i]);
            return RE::BSContainer::ForEachResult::kContinue;
        });
    }
    // Hit squad and gift-giver quests, named after the faction's tab AV (REP_PBT_<Name>AV).
    struct QuestRow { std::uint32_t id; const char* faction; const char* event; };
    constexpr QuestRow kRepQuests[] = {
        {0x359, "BrotherhoodOfSteel", "hitSquad"}, {0x35D, "ChildrenOfAtom", "hitSquad"}, {0x360, "Covenant", "hitSquad"},
        {0x45B, "DiamondCity", "gift"}, {0x469, "Acadia", "gift"}, {0x472, "BunkerHill", "gift"},
        {0x478, "ChildrenOfAtom", "gift"}, {0x482, "Covenant", "gift"}, {0x48A, "Disciples", "gift"},
        {0x48B, "Operators", "gift"}, {0x48C, "Pack", "gift"}, {0x48D, "Goodneighbor", "gift"},
        {0x495, "Harbormen", "gift"}, {0x49D, "TheSlog", "gift"}, {0x4A7, "Vault81", "gift"},
        {0x6B2, "DiamondCity", "hitSquad"}, {0x6B3, "Disciples", "hitSquad"}, {0x6B4, "Goodneighbor", "hitSquad"},
        {0x6B5, "Institute", "hitSquad"}, {0x6B6, "Operators", "hitSquad"}, {0x6B7, "Pack", "hitSquad"},
        {0x6B8, "Railroad", "hitSquad"}, {0x6B9, "BunkerHill", "hitSquad"}, {0x87E, "BrotherhoodOfSteel", "hitSquad"},
        {0x883, "BunkerHill", "hitSquad"}, {0x886, "ChildrenOfAtom", "hitSquad"}, {0x889, "Covenant", "hitSquad"},
        {0x88C, "Institute", "hitSquad"}, {0x88F, "Railroad", "hitSquad"}};
    for (const auto& row : kRepQuests) {
        auto* quest = Lookup<RE::TESQuest>(row.id, "REPUTATION.esm");
        if (!quest) continue;
        const auto want = Lower(std::string("REP_PBT_") + row.faction + "AV");
        for (std::size_t i = 0; i < forms.repTitle.size(); ++i) {
            const char* edid = forms.repTitle[i]->GetFormEditorID();
            if (!edid || !StartsWith(edid, want)) continue;
            forms.repQuests.push_back({quest, i, row.event});
            break;
        }
    }

    forms.challengeQuest = Lookup<RE::TESQuest>(0x067, "CHALLENGES.esm");
    forms.mutations = Plugin("ImmersiveLegendaryMutations.esp");
    if (auto* k = Lookup<RE::BGSKeyword>(0x1ECF, "NAF.esp")) forms.sceneKeywords.push_back(k);   // NAF_InScene
    if (auto* k = Lookup<RE::BGSKeyword>(0x915A, "AAF.esm")) forms.sceneKeywords.push_back(k);   // Bridge_ActorBusy (NAF Bridge)

#if defined(PHOSPHOR_AE)
    if (AE::Enabled("bSurvival"))
#endif
    if (auto* data = RE::TESDataHandler::GetSingleton()) {
        for (auto* perk : data->GetFormArray<RE::BGSPerk>()) {
            if (!perk) continue;
            auto* file = perk->GetFile(0);
            if (!file || _stricmp(file->filename, "Fallout4.esm") != 0) continue;
            if (Name(perk) == "Adrenaline") forms.adrenaline.push_back(perk);
        }
        for (auto* global : data->GetFormArray<RE::TESGlobal>()) {
            if (!global) continue;
            const char* id = global->GetFormEditorID();
            if (!id || !Has(id, "adrenaline")) continue;
            spdlog::info("Adrenaline global {} = {}", id, global->GetValue());
            const int v = static_cast<int>(std::lround(global->GetValue()));
            if (Has(id, "kill") && v >= 1 && v <= 50) forms.adrenalinePerRank = v;
        }
    }
    resolved = true;
#if defined(PHOSPHOR_AE)
    AE::modDetection = std::format("Mods: affinity AV {}, FollowerState AV {}, KARMA {}, REPUTATION {} factions, CHALLENGES {}, Immersive Legendary Mutations {}, Adrenaline perks {} (kills per rank {}), scene keywords {}",
        forms.affinity != nullptr, forms.followerState != nullptr, forms.karma != nullptr, forms.repFame.size(),
        forms.challengeQuest != nullptr, forms.mutations != nullptr, forms.adrenaline.size(), forms.adrenalinePerRank, forms.sceneKeywords.size());
#endif
    spdlog::info("REPUTATION: {} owned locations, {} hit squad/gift quests", forms.repLocations.size(), forms.repQuests.size());
    spdlog::info("Mods: affinity AV {}, FollowerState AV {}, KARMA {}, REPUTATION {} factions, CHALLENGES {}, "
                 "Immersive Legendary Mutations {}, Adrenaline perks {} (kills per rank {}), scene keywords {}",
                 forms.affinity != nullptr, forms.followerState != nullptr, forms.karma != nullptr, forms.repFame.size(),
                 forms.challengeQuest != nullptr, forms.mutations != nullptr, forms.adrenaline.size(), forms.adrenalinePerRank,
                 forms.sceneKeywords.size());
}

void ResetSession() {
    SetEventCapture(false);
    repSeen.clear();
    repBaseline = false;
    repQuestRunning.clear();
    followerStates.clear();
    adrenalineRank = -1;
    adrenalineKills = 0;
    corpses.clear();
    drops.clear();
    attack.reset();
    pendingKills = 0;
    pendingLegendaryDeaths.clear();
}

void SetEventCapture(bool enabled) { deathEvents.SetEnabled(enabled); }

void RegisterSinks() {
#if defined(PHOSPHOR_AE)
    AE_GATE("bModData", );
#endif
    if (sinksRegistered) return;
    if (auto* deaths = RE::TESDeathEvent::GetEventSource()) {
        deaths->RegisterSink(&deathWatch);
        sinksRegistered = true;
    }
}

bool HasKarma() { return forms.karma != nullptr; }

bool IsCompanion(RE::Actor& actor) {
#if defined(PHOSPHOR_AE)
    if (!AE::Enabled("bCompanions")) return false;
#endif
#if defined(PHOSPHOR_AE)
    AE_GATE("bModData", false);
#endif
    if (forms.ligaHome && actor.IsInFaction(forms.ligaHome)) return false;
    if (forms.currentCompanion && actor.IsInFaction(forms.currentCompanion)) return true;
    if (forms.ligaTeam && actor.IsInFaction(forms.ligaTeam)) return true;
    return actor.boolFlags.all(RE::Actor::BOOL_FLAGS::kIsCommandedActor);
}
bool HasReputation() { return !forms.repFame.empty(); }

std::optional<float> Affinity(RE::Actor& actor) {
#if defined(PHOSPHOR_AE)
    if (!AE::Enabled("bCompanions")) return std::nullopt;
#endif
#if defined(PHOSPHOR_AE)
    AE_GATE("bModData", std::nullopt);
#endif
    if (!forms.affinity) return std::nullopt;
    const float value = actor.GetActorValue(*forms.affinity);
    // Vanilla companions carry the value; other followers (Liga recruits, settlers) read 0.
    if (value == 0.0F) return std::nullopt;
    return value;
}

void LogFollowerState(RE::Actor& actor, const std::string& name) {
#if defined(PHOSPHOR_AE)
    if (!AE::Enabled("bCompanions")) return ;
#endif
#if defined(PHOSPHOR_AE)
    AE_GATE("bModData", );
#endif
    if (!forms.followerState) return;
    const float value = actor.GetActorValue(*forms.followerState);
    auto [it, fresh] = followerStates.try_emplace(actor.GetFormID(), value);
    if (fresh || it->second != value) {
        it->second = value;
        spdlog::info("Companion {} FollowerState = {}", name, value);
    }
}

std::vector<std::string> LegendaryEffects(const RE::ExtraDataList* extra) {
#if defined(PHOSPHOR_AE)
    AE_GATE("bModData", {});
#endif
    std::vector<std::pair<int, std::string>> found;
    auto* instance = extra ? extra->GetByType<RE::BGSObjectInstanceExtra>() : nullptr;
    if (!instance || !instance->values) return {};
    for (const auto& data : instance->GetIndexData()) {
        if (data.disabled) continue;
        auto* mod = RE::TESForm::GetFormByID<RE::BGSMod::Attachment::Mod>(data.objectID);
        if (!mod) continue;
        auto* point = RE::detail::BGSKeywordGetTypedKeywordByIndex(RE::KeywordType::kAttachPoint, mod->attachPoint.keywordIndex);
        const char* edid = point ? point->GetFormEditorID() : nullptr;
        if (!edid || !StartsWith(edid, "ap_Legendary")) continue;
        const std::string_view id(edid);
        int slot = 1;
        if (id.size() > 13 && id[12] == '_' && std::isdigit(static_cast<unsigned char>(id[13]))) slot = id[13] - '0';
        auto name = Live::CleanName(Name(mod), true);
        if (name.empty()) continue;
        found.emplace_back(slot, std::move(name));
    }
    std::stable_sort(found.begin(), found.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    std::vector<std::string> out;
    for (auto& [slot, name] : found) {
        if (std::find(out.begin(), out.end(), name) == out.end()) out.push_back(std::move(name));
        if (out.size() == 5) break;
    }
    return out;
}

Json Mutations(RE::Actor& actor) {
#if defined(PHOSPHOR_AE)
    AE_GATE("bModData", Json::array());
#endif
    Json out = Json::array();
    if (!forms.mutations) return out;
    auto* list = actor.GetActiveEffectList();
    if (!list) return out;
    struct Known { std::string_view spell; const char* name; const char* kind; };
    constexpr Known kKnown[] = {
        {"dr buff", "Damage resist", "defense"}, {"er buff", "Energy resist", "defense"},
        {"dt buff", "Damage threshold", "defense"}, {"dr shield buff", "Shield", "defense"},
        {"er shield buff", "Energy shield", "defense"}, {"disguised synth", "Disguised synth", "defense"},
        {"speed", "Speed", "offense"}, {"self destruct", "Self-destruct", "offense"}, {"firey", "Fiery", "offense"},
        {"healing", "Healing", "heal"}};
    std::set<std::string> seen;
    for (auto& entry : list->data) {
        auto* effect = entry.get();
        if (!effect || !effect->spell || Inactive(*effect)) continue;
        if (effect->spell->GetFile(0) != forms.mutations) continue;
        const auto spell = Trim(Name(effect->spell));
        const auto lower = Lower(spell);
        if (lower.empty() || lower == "legendary" || Has(lower, "attack")) continue;
        Json m;
        for (const auto& k : kKnown) {
            if (lower == k.spell) { m = Json{{"name", k.name}, {"kind", k.kind}}; break; }
        }
        if (m.is_null()) {
            auto name = spell;
            name[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(name[0])));
            m = Json{{"name", name}};
        }
        if (seen.insert(m["name"].get<std::string>()).second) out.push_back(std::move(m));
    }
    return out;
}

bool HasArmorPlates() {
#if defined(PHOSPHOR_AE)
    AE_GATE("bModData", false);
#endif
    return forms.plateNow && forms.plateMax && forms.platePer;
}

Json ReadArmorPlates() {
#if defined(PHOSPHOR_AE)
    AE_GATE("bModData", nullptr);
#endif
    if (!HasArmorPlates()) return nullptr;
    int stock = 0;
    if (auto* player = RE::PlayerCharacter::GetSingleton(); player && player->inventoryList && forms.plateItem) {
        auto& inventory = *player->inventoryList;
        RE::BSAutoReadLock lock{inventory.rwLock};
        for (auto& item : inventory.data)
            if (item.object == forms.plateItem) stock += static_cast<int>(item.GetCount());
    }
    return Json{{"current", forms.plateNow->GetValue()}, {"max", forms.plateMax->GetValue()},
                {"perPlate", forms.platePer->GetValue()}, {"stock", stock}};
}

Json ReadKarma() {
#if defined(PHOSPHOR_AE)
    AE_GATE("bModData", nullptr);
#endif
    if (!forms.karma) return nullptr;
    const float value = PlayerValue(forms.karma);
    const int alignment = value >= 750 ? 2 : value >= 250 ? 1 : value <= -750 ? -2 : value <= -250 ? -1 : 0;
    Json out{{"value", std::lround(value)}, {"alignment", alignment}};
    if (auto* titleAv = forms.karmaTitles[static_cast<std::size_t>(alignment + 2)]) {
        const int index = static_cast<int>(std::lround(PlayerValue(titleAv)));
        if (auto title = ModData::KarmaTitle(index)) out["title"] = *title;
    }
    return out;
}

namespace {
Json RepStatus(std::size_t i) {
    const float fame = PlayerValue(forms.repFame[i]), infamy = PlayerValue(forms.repInfamy[i]);
    Json out{{"faction", forms.repNames[i]}, {"fame", std::max(0.0F, fame)}, {"infamy", std::max(0.0F, infamy)}};
    const int index = static_cast<int>(std::lround(PlayerValue(forms.repTitle[i])));
    if (auto title = ModData::ReputationTitle(index)) out["title"] = *title;
    return out;
}
// Quest running: the QUEST_DATA "enabled" bit (bit 0), as TESQuest::IsRunning reads it.
bool Running(const RE::TESQuest* quest) { return quest && (quest->data.flags & 0x1) != 0; }
}

std::optional<std::string> LocalFaction(Json* status) {
#if defined(PHOSPHOR_AE)
    AE_GATE("bModData", std::nullopt);
#endif
    if (forms.repLocations.empty()) return std::nullopt;
    auto* player = RE::PlayerCharacter::GetSingleton();
    auto* location = player ? player->GetCurrentLocation() : nullptr;
    for (int depth = 0; location && depth < 8; ++depth, location = location->parentLoc) {
        const auto it = forms.repLocations.find(location);
        if (it == forms.repLocations.end()) continue;
        if (status) *status = RepStatus(it->second);
        return forms.repNames[it->second];
    }
    return std::nullopt;
}

std::vector<Json> ReputationEvents() {
#if defined(PHOSPHOR_AE)
    AE_GATE("bModData", {});
#endif
    std::vector<Json> out;
    const std::size_t n = forms.repFame.size();
    if (!n) return out;
    std::vector<RepSeen> now(n);
    for (std::size_t i = 0; i < n; ++i) {
        now[i].fame = PlayerValue(forms.repFame[i]);
        now[i].infamy = PlayerValue(forms.repInfamy[i]);
        now[i].shown = forms.repShown[i] && forms.repShown[i]->GetValue() >= 1.0F;
    }
    std::map<const RE::TESQuest*, bool> running;
    for (const auto& q : forms.repQuests) running[q.quest] = Running(q.quest);
    if (!repBaseline || repSeen.size() != n) {
        repSeen = std::move(now);
        repQuestRunning = std::move(running);
        repBaseline = true;
        return out;
    }
    for (const auto& q : forms.repQuests) {
        if (!running[q.quest] || repQuestRunning[q.quest]) continue;
        Json e = RepStatus(q.faction);
        e["event"] = q.event;
        out.push_back(std::move(e));
        spdlog::info("REPUTATION: {} {}", forms.repNames[q.faction], q.event);
    }
    repQuestRunning = std::move(running);
    for (std::size_t i = 0; i < n; ++i) {
        const auto& before = repSeen[i];
        const auto& cur = now[i];
        const int index = static_cast<int>(std::lround(PlayerValue(forms.repTitle[i])));
        Json base{{"faction", forms.repNames[i]}, {"fame", std::max(0.0F, cur.fame)}, {"infamy", std::max(0.0F, cur.infamy)}};
        if (auto title = ModData::ReputationTitle(index)) base["title"] = *title;
        if (cur.shown && !before.shown) { Json e = base; e["event"] = "discovered"; out.push_back(std::move(e)); }
        if (std::abs(cur.fame - before.fame) >= 0.5F) {
            Json e = base; e["event"] = cur.fame > before.fame ? "fameGained" : "fameLost"; out.push_back(std::move(e));
        }
        if (std::abs(cur.infamy - before.infamy) >= 0.5F) {
            Json e = base; e["event"] = cur.infamy > before.infamy ? "infamyGained" : "infamyLost"; out.push_back(std::move(e));
        }
    }
    repSeen = std::move(now);
    return out;
}

#if defined(PHOSPHOR_AE)
std::uint32_t ChallengeQuestId() {
    AE_GATE("bModData", 0);
    return forms.challengeQuest ? forms.challengeQuest->GetFormID() : 0;
}
#else
std::uint32_t ChallengeQuestId() { return forms.challengeQuest ? forms.challengeQuest->GetFormID() : 0; }
#endif

Json ReadChallenges(const Pipboy::Snapshot& pip) {
#if defined(PHOSPHOR_AE)
    AE_GATE("bModData", nullptr);
#endif
    if (!forms.challengeQuest || !pip.connected) return nullptr;
    const auto id = forms.challengeQuest->GetFormID();
    Json out = Json::array();
    for (const auto& q : pip.quests) {
        if (q.formId != id) continue;
        for (const auto& o : q.objectives) {
            if (o.completed) continue;
            auto challenge = ModData::ParseChallenge(o.text, [](std::string_view edid) -> std::optional<double> {
                const std::string key(edid);
                auto* global = RE::TESForm::GetFormByEditorID<RE::TESGlobal>(RE::BSFixedString(key.c_str()));
                return global ? std::optional<double>(global->GetValue()) : std::nullopt;
            });
            if (challenge) out.push_back(std::move(*challenge));
        }
    }
    return out;
}

std::optional<Json> ChallengeFromMessage(const std::string& text) {
    constexpr std::string_view kPrefix = "Challenge completed: ";
    if (!forms.challengeQuest || !StartsWith(text, kPrefix)) return std::nullopt;
    auto name = Trim(text.substr(kPrefix.size()));
    while (!name.empty() && (name.back() == '.' || name.back() == '!')) name.pop_back();
    if (name.empty()) return std::nullopt;
    return Json{{"id", "ch-" + Slug(name)}, {"name", name}};
}

Json ReadAttack(const Pipboy::Snapshot& pip, int enemyCount) {
#if defined(PHOSPHOR_AE)
    AE_GATE("bModData", nullptr);
#endif
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player || !pip.connected) return nullptr;
    auto* cell = player->GetParentCell();
    const bool exterior = cell && cell->IsExterior();
    const double px = exterior ? player->data.location.x : pip.playerX;
    const double py = exterior ? player->data.location.y : pip.playerY;

    std::string name;
    const Pipboy::Location* where = nullptr;
    bool done = false;
    for (const auto& q : pip.quests) {
        for (const auto& o : q.objectives) {
            if (!StartsWith(o.text, "Defend ")) continue;
            const auto target = Trim(o.text.substr(7));
            for (const auto& l : pip.locations) {
                if (!l.workshop || Lower(l.name) != Lower(target)) continue;
                where = &l;
                break;
            }
            if (!where) continue;
            name = where->name;
            done = o.completed;
            break;
        }
        if (where) break;
    }

    const auto now = Clock::now();
    if (!where) {
        if (!attack) return nullptr;
        if (!attack->ended) {
            attack->ended = true;
            attack->endedAt = now;
            if (attack->state != "won") attack->state = "ended";
        }
        if (now - attack->endedAt > std::chrono::seconds(8)) { attack.reset(); return nullptr; }
        return Json{{"id", Slug(attack->name)}, {"name", attack->name}, {"state", attack->state}};
    }
    if (!attack || attack->name != name) attack = AttackSeen{name, "pending", 0, {}, false};

    const double dx = where->x - px, dy = where->y - py;
    const double dist = std::sqrt(dx * dx + dy * dy) / 70.0;
    if (done) {
        attack->state = "won";
    } else if (dist < 200 && exterior && enemyCount > 0) {
        attack->state = "active";
        attack->total = std::max(attack->total, enemyCount);
    } else if (attack->state != "active") {
        attack->state = "pending";
    }
    Json out{{"id", Slug(name)}, {"name", name}, {"state", attack->state}};
    if (attack->state == "active") {
        out["total"] = attack->total;
        out["remaining"] = std::min(enemyCount, attack->total);
    }
    if (attack->state != "won") {
        out["b"] = Round1(Bearing(dx, dy), 1);
        out["dist"] = std::lround(dist);
    }
    return out;
}

Json ReadAdrenaline() {
#if defined(PHOSPHOR_AE)
    if (!AE::Enabled("bSurvival")) return nullptr;
#endif
#if defined(PHOSPHOR_AE)
    AE_GATE("bModData", nullptr);
#endif
    DrainDeaths();
    auto* player = RE::PlayerCharacter::GetSingleton();
    const int kills = std::exchange(pendingKills, 0);
    if (!player || forms.adrenaline.empty()) return nullptr;
    int rank = 0;
    for (auto* perk : forms.adrenaline) rank += player->GetPerkRank(perk);
    rank = std::clamp(rank, 0, 10);
    if (rank != adrenalineRank) {
        if (adrenalineRank >= 0) spdlog::info("Adrenaline rank {} -> {} (kills counted {})", adrenalineRank, rank, adrenalineKills + kills);
        adrenalineRank = rank;
        adrenalineKills = 0;
    } else {
        adrenalineKills += kills;
    }
    const int perRank = forms.adrenalinePerRank > 0 ? forms.adrenalinePerRank : 5;
    Json out{{"rank", rank}, {"kills", rank >= 10 ? 0 : std::min(adrenalineKills, perRank - 1)}};
    if (forms.adrenalinePerRank > 0) out["perRank"] = forms.adrenalinePerRank;
    return out;
}

std::vector<Json> LegendaryLootEvents(bool markers) {
#if defined(PHOSPHOR_AE)
    AE_GATE("bModData", {});
#endif
    DrainDeaths();
    std::vector<Json> out;
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) return out;
    const auto now = Clock::now();
    {
        for (auto id : pendingLegendaryDeaths) {
            if (std::none_of(corpses.begin(), corpses.end(), [&](const Corpse& c) { return c.id == id; }))
                corpses.push_back(Corpse{id, now, false});
        }
        pendingLegendaryDeaths.clear();
    }
    const double px = player->data.location.x, py = player->data.location.y;
    // Legendary items are added to the body by script as it dies; give that a few seconds to show up.
    std::vector<Drop> still;
    for (auto& corpse : corpses) {
        auto* ref = RE::TESForm::GetFormByID<RE::TESObjectREFR>(corpse.id);
        if (!ref || !ref->inventoryList || !ref->GetParentCell()) continue;
        auto& inventory = *ref->inventoryList;
        RE::BSAutoReadLock lock{inventory.rwLock};
        for (auto& item : inventory.data) {
            if (!item.object) continue;
            for (auto* stack = item.stackData.get(); stack; stack = stack->nextStack.get()) {
                auto effects = LegendaryEffects(stack->extra.get());
                if (effects.empty()) continue;
                char id[24];
                std::snprintf(id, sizeof(id), "%08x_%08x", corpse.id, item.object->GetFormID());
                Drop drop{corpse.id, item.object->GetFormID(), id, Live::CleanName(Name(item.object), true), std::move(effects)};
                if (drop.item.empty()) drop.item = "Item";
                const auto known = std::find_if(drops.begin(), drops.end(), [&](const Drop& d) { return d.id == drop.id; });
                corpse.found = true;
                const double dx = ref->data.location.x - px, dy = ref->data.location.y - py;
                const double b = Round1(Bearing(dx, dy), 1);
                const long dist = std::lround(std::sqrt(dx * dx + dy * dy) / 70.0);
                if (known == drops.end()) {
                    Json e{{"id", drop.id}, {"item", drop.item}, {"effects", drop.effects}, {"dropped", true}};
                    if (markers) { e["b"] = b; e["dist"] = dist; }
                    out.push_back(Json{{"legendaryLoot", std::move(e)}});
                    spdlog::info("Legendary drop: {} on {:08X}", drop.item, corpse.id);
                } else {
                    drop.b = known->b;
                    drop.dist = known->dist;
                    if (markers && (std::abs(b - drop.b) >= 1 || dist != drop.dist))
                        out.push_back(Json{{"marker", Json{{"id", "loot:" + drop.id}, {"b", b}, {"dist", dist}}}});
                }
                drop.b = b;
                drop.dist = dist;
                still.push_back(std::move(drop));
                break;
            }
        }
    }
    // Drops no longer on their corpse were picked up (by the player, as a rule).
    for (const auto& d : drops) {
        if (std::any_of(still.begin(), still.end(), [&](const Drop& s) { return s.id == d.id; })) continue;
        auto* ref = RE::TESForm::GetFormByID<RE::TESObjectREFR>(d.corpse);
        if (!ref || !ref->GetParentCell()) {  // unloaded: drop its marker quietly
            out.push_back(Json{{"markerRemove", "loot:" + d.id}});
            continue;
        }
        out.push_back(Json{{"legendaryLoot", Json{{"id", d.id}, {"item", d.item}, {"effects", d.effects}, {"picked", true}}}});
    }
    drops = std::move(still);
    std::erase_if(corpses, [&](const Corpse& c) {
        auto* ref = RE::TESForm::GetFormByID<RE::TESObjectREFR>(c.id);
        if (!ref || !ref->GetParentCell()) return true;
        if (c.found) return std::none_of(drops.begin(), drops.end(), [&](const Drop& d) { return d.corpse == c.id; });
        return now - c.since > std::chrono::seconds(8);
    });
    return out;
}

bool PlayerInScene() {
#if defined(PHOSPHOR_AE)
    AE_GATE("bModData", false);
#endif
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) return false;
    static bool last = false;
    bool in = false;
    for (auto* keyword : forms.sceneKeywords) if (player->HasKeywordHelper(keyword, nullptr)) { in = true; break; }
    if (in != last) { spdlog::info("Animation scene: {}", in ? "started" : "ended"); last = in; }
    return in;
}

std::string PerkMenuKey() {
#if defined(PHOSPHOR_AE)
    AE_GATE("bModData", {});
#endif
    std::ifstream file("Data/MCM/Settings/Keybinds.json", std::ios::binary);
    if (!file) return {};
    Json root;
    try { root = Json::parse(file); } catch (...) { return {}; }
    if (!root.contains("keybinds") || !root["keybinds"].is_array()) return {};
    for (const auto& bind : root["keybinds"]) {
        if (bind.value("modName", "") != "PRKF" || bind.value("id", "") != "levelupmenuex_open") continue;
        const int code = bind.value("keycode", 0);
        const int mods = bind.value("modifiers", 0);
        if (code <= 0) return {};
        std::string key;
        if ((code >= '0' && code <= '9') || (code >= 'A' && code <= 'Z')) key = std::string(1, static_cast<char>(code));
        else if (code >= VK_F1 && code <= VK_F24) key = "F" + std::to_string(code - VK_F1 + 1);
        else {
            char text[64] = {};
            const LONG scan = static_cast<LONG>(MapVirtualKeyA(static_cast<UINT>(code), MAPVK_VK_TO_VSC)) << 16;
            if (GetKeyNameTextA(scan, text, sizeof(text)) > 0) key = text;
        }
        if (key.empty()) return {};
        std::string prefix;
        if (mods & 2) prefix += "Ctrl+";
        if (mods & 1) prefix += "Shift+";
        if (mods & 4) prefix += "Alt+";
        return prefix + key;
    }
    return {};
}
}
