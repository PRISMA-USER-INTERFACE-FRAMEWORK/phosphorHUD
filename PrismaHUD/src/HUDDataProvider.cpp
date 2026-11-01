#include <mutex>

#include "HUDDataProvider.h"
#include "HUDLayoutManager.h"
#include "QuickLootController.h"
#include "ActivateChoiceRegistry.h"
#include "ControlLabels.h"
#include <shlobj.h>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <functional>
#include <limits>

namespace
{
    struct MapMarkerLayout
    {
        std::byte       _fullName[0x10];
        std::uint8_t    flags;
        std::uint8_t    originalFlags;
        RE::MARKER_TYPE type;
    };
    static_assert(sizeof(MapMarkerLayout) == sizeof(RE::MapMarkerData));
    constexpr std::uint8_t kMapMarkerVisible = 0x01;

    constexpr const char* kSuppressedWidgets[] = {
        "HUDAmmoCounter",
        "HUDExplosiveAmmoCounter",
        "HUDCompass",
        "HUDPlayerHealthMeter",
        "HUDEnemyHealthMeter",
        "HUDActiveEffectsDisplay",
        "HUDActionPointMeter",
        "HUDQuickContainer",
        "HUDRollover",
    };
}

void InstallVanillaHUDSuppression(PRISMA_UI_API::IVPrismaUI6* api)
{
    if (!api) {
        logger::error("PrismaHUD: InstallVanillaHUDSuppression called with a null V6 API pointer");
        return;
    }
    for (const char* widget : kSuppressedWidgets) {
        const bool ok = api->SuppressHUDWidget(widget, true);
        logger::info("PrismaHUD: SuppressHUDWidget('{}') -> {}", widget, ok);
    }
}

namespace
{
    struct QuestTargetLite
    {
        std::uint8_t  flags;
        std::uint8_t  _pad01[7];
        void*         conditions;
        std::int32_t  alias;
        std::uint32_t _pad14;
    };
    static_assert(offsetof(QuestTargetLite, alias) == 0x10, "TESQuestTarget::alias must be at 0x10");
}

HUDDataProvider* HUDDataProvider::GetSingleton()
{
    static HUDDataProvider s_instance;
    return &s_instance;
}

static float GetAV(RE::Actor* actor, RE::ActorValueInfo** field) noexcept
{
    if (!actor || !field || !*field) return 0.0f;
    try { return actor->GetActorValue(**field); }
    catch (...) {
        static int s_e = 0;
        if ((s_e++ % 100) == 0) logger::warn("PrismaHUD: GetActorValue threw -- returning 0");
        return 0.0f;
    }
}

static float GetPermAV(RE::Actor* actor, RE::ActorValueInfo** field) noexcept
{
    if (!actor || !field || !*field) return 0.0f;
    try { return actor->GetPermanentActorValue(**field); }
    catch (...) {
        static int s_e = 0;
        if ((s_e++ % 100) == 0) logger::warn("PrismaHUD: GetPermanentActorValue threw -- returning 0");
        return 0.0f;
    }
}

namespace {
struct CompassCache {
    int tick = 0;
    std::string location;
    float questDir = -1.f;
    nlohmann::json markers = nlohmann::json::array();
    std::string markersSent;
};
CompassCache g_compassCache;

void ResetDataBuilderCaches()
{
    g_compassCache = CompassCache{};
}
}

static std::string BuildHealthJSON()
{
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) return R"({"hp":0,"hpMax":1,"rads":0,"radsMax":1000})";

    auto* av    = RE::ActorValue::GetSingleton();
    float hp    = GetAV(player,     &av->health);
    float hpMax = GetPermAV(player, &av->health);
    float rads  = GetAV(player,     &av->rads);
    if (hpMax <= 0.f) hpMax = 1.f;

    return nlohmann::json{
        {"hp",      hp},
        {"hpMax",   hpMax},
        {"rads",    rads},
        {"radsMax", 1000.f},
    }.dump();
}

static std::string BuildAPJSON()
{
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) return R"({"ap":0,"apMax":1})";

    auto* av    = RE::ActorValue::GetSingleton();
    float ap    = GetAV(player,     &av->actionPoints);
    float apMax = GetPermAV(player, &av->actionPoints);
    if (apMax <= 0.f) apMax = 1.f;

    return nlohmann::json{{"ap", ap}, {"apMax", apMax}}.dump();
}

static std::string BuildCompassJSON()
{
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) return R"({"heading":0,"location":"","questDir":-1.0})";

    constexpr float kRad2Deg = 57.2957795f;
    float heading = player->data.angle.z * kRad2Deg;
    while (heading <   0.f) heading += 360.f;
    while (heading >= 360.f) heading -= 360.f;

    auto& cache = g_compassCache;
    const bool heavy = (cache.tick++ % 5) == 0;

    std::string location = cache.location;
    float questDir = cache.questDir;
    if (!heavy) {
        return nlohmann::json{
            {"heading",  heading},
            {"location", location},
            {"questDir", questDir},
        }.dump();
    }

    location.clear();
    try {
        auto* cell = player->GetParentCell();
        if (cell) {
            auto sv = RE::TESFullName::GetFullName(*cell);
            if (!sv.empty()) location = std::string(sv);
        }
    } catch (...) {}

    constexpr std::uint16_t kQuestActiveFlag = 0x0800;

    questDir = -1.f;
    int dbgTotal = 0, dbgDisplayed = 0, dbgTracked = 0, dbgTargets = 0, dbgResolved = 0;
    int dbgAlias0 = -999;
    try {
        const RE::NiPoint3 ppos = player->GetPosition();
        float bestDistSq = (std::numeric_limits<float>::max)();

        for (auto& inst : player->objectives) {
            ++dbgTotal;
            if (inst.enstanceState.get() != RE::QUEST_OBJECTIVE_STATE::kDisplayed) continue;
            ++dbgDisplayed;
            auto* obj = inst.objective;
            if (!obj || !obj->ownerQuest || !obj->targets) continue;
            if ((obj->ownerQuest->data.flags & kQuestActiveFlag) == 0) continue;
            ++dbgTracked;

            std::uint32_t n = obj->numTargets;
            if (n > 64u) n = 64u;
            for (std::uint32_t i = 0; i < n; ++i) {
                auto* tgt = obj->targets[i];
                if (!tgt) continue;
                ++dbgTargets;
                std::int32_t alias = reinterpret_cast<const QuestTargetLite*>(tgt)->alias;
                if (dbgAlias0 == -999) dbgAlias0 = alias;
                if (alias < 0) continue;

                RE::ObjectRefHandle handle;
                obj->ownerQuest->GetAliasedRef(&handle, static_cast<std::uint32_t>(alias));
                auto ref = handle.get();
                if (!ref) continue;
                ++dbgResolved;

                const RE::NiPoint3 rpos = ref->GetPosition();
                const float dx = rpos.x - ppos.x;
                const float dy = rpos.y - ppos.y;
                const float dsq = dx * dx + dy * dy;
                if (dsq < bestDistSq && dsq > 1.0f) {
                    bestDistSq = dsq;
                    float bearing = std::atan2(dx, dy) * kRad2Deg;
                    while (bearing <   0.f) bearing += 360.f;
                    while (bearing >= 360.f) bearing -= 360.f;
                    questDir = bearing;
                }
            }
        }
    } catch (...) {
        questDir = -1.f;
    }

    {
        static int s_cq = 0;
        if ((s_cq++ % 6) == 0)
            logger::debug("PrismaHUD compass: objectives={} displayed={} tracked={} targets={} resolved={} "
                          "alias0={} heading={:.0f} questDir={:.0f}",
                          dbgTotal, dbgDisplayed, dbgTracked, dbgTargets, dbgResolved, dbgAlias0, heading, questDir);
    }

    nlohmann::json markers = nlohmann::json::array();
    try {
        const RE::NiPoint3 ppos = player->GetPosition();
        for (const auto& handle : player->currentMapMarkers) {
            auto refPtr = handle.get();
            auto* ref = refPtr.get();
            if (!ref || ref->IsDeleted()) continue;
            if (!ref->extraList) continue;

            auto* xm = ref->extraList->GetByType<RE::ExtraMapMarker>();
            if (!xm || !xm->mapMarkerData) continue;
            auto* md = xm->mapMarkerData;

            const auto* mm = reinterpret_cast<const MapMarkerLayout*>(md);
            if (!(mm->flags & kMapMarkerVisible)) continue;

            const RE::NiPoint3 rpos = ref->GetPosition();
            const float dx = rpos.x - ppos.x;
            const float dy = rpos.y - ppos.y;
            const float dsq = dx * dx + dy * dy;
            if (dsq <= 1.0f) continue;

            float bearing = std::atan2(dx, dy) * kRad2Deg;
            while (bearing <   0.f) bearing += 360.f;
            while (bearing >= 360.f) bearing -= 360.f;

            markers.push_back({
                {"dir",  bearing},
                {"dist", std::sqrt(dsq)},
                {"type", static_cast<int>(mm->type)},
            });
        }
    } catch (...) {}

    cache.location = location;
    cache.questDir = questDir;
    cache.markers  = markers;

    nlohmann::json out{
        {"heading",  heading},
        {"location", location},
        {"questDir", questDir},
    };
    if (auto dump = markers.dump(); dump != cache.markersSent) {
        cache.markersSent = std::move(dump);
        out["markers"] = markers;
    }
    return out.dump();
}

static std::string BuildRadsJSON()
{
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) return R"({"rads":0,"radsMax":1000,"radHealthMax":0})";

    auto* av  = RE::ActorValue::GetSingleton();
    float rads         = GetAV(player, &av->rads);
    float radHealthMax = GetAV(player, &av->radHealthMax);

    return nlohmann::json{
        {"rads",         rads},
        {"radsMax",      1000.f},
        {"radHealthMax", radHealthMax},
    }.dump();
}

static std::string BuildAmmoJSON()
{
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) return R"({"loaded":0,"reserve":0,"name":"","hasWeapon":false})";

    try {
        RE::TESObjectWEAP* weapon   = nullptr;
        RE::TESAmmo*       ammoForm = nullptr;
        int32_t            loaded   = -1;

        if (auto* proc = player->currentProcess; proc && proc->middleHigh) {
            RE::BSAutoLock lk{ proc->middleHigh->equippedItemsLock };
            for (auto& eq : proc->middleHigh->equippedItems) {
                auto* obj = eq.item.object;
                if (!obj) continue;
                auto* form = obj->As<RE::TESForm>();
                if (!form || form->formType != RE::ENUM_FORM_ID::kWEAP) continue;
                weapon = form->As<RE::TESObjectWEAP>();
                auto* base = eq.data.get();
                if (base && reinterpret_cast<const std::uintptr_t*>(base)[0] ==
                                RE::EquippedWeaponData::VTABLE[0].address()) {
                    auto* wd = static_cast<RE::EquippedWeaponData*>(base);
                    ammoForm = wd->ammo;
                    loaded   = static_cast<int32_t>(wd->ammoCount);
                } else if (base) {
                    static int s_vt = 0;
                    if ((s_vt++ % 40) == 0)
                        logger::warn("PrismaHUD ammo: EquippedItemData vtable mismatch "
                                     "(got {:#x}, want {:#x}) -- offset/runtime drift?",
                                     reinterpret_cast<const std::uintptr_t*>(base)[0],
                                     RE::EquippedWeaponData::VTABLE[0].address());
                }
                break;
            }
        }

        if (!weapon || !weapon->IsGunWeapon())
            return R"({"loaded":0,"reserve":0,"name":"","ammoName":"","hasWeapon":false})";

        std::string weaponName;
        { auto sv = RE::TESFullName::GetFullName(*weapon); weaponName = sv.empty() ? "" : std::string(sv); }

        std::string ammoName;
        int32_t     reserve = 0;
        if (ammoForm) {
            auto sv  = RE::TESFullName::GetFullName(*ammoForm);
            ammoName = sv.empty() ? "" : std::string(sv);
            if (player->inventoryList) {
                auto ammoID = ammoForm->GetFormID();
                player->inventoryList->ForEachStack(
                    [](RE::BGSInventoryItem&) { return true; },
                    [&](RE::BGSInventoryItem& item, RE::BGSInventoryItem::Stack& stack) {
                        if (item.object && item.object->GetFormID() == ammoID)
                            reserve += static_cast<int32_t>(stack.count);
                        return true;
                    });
            }
        }
        if (loaded < 0) loaded = 0;

        return nlohmann::json{
            {"loaded",    loaded},
            {"reserve",   reserve},
            {"name",      weaponName},
            {"ammoName",  ammoName},
            {"hasWeapon", true},
        }.dump();
    } catch (...) {
        return R"({"loaded":0,"reserve":0,"name":"","ammoName":"","hasWeapon":false})";
    }
}

static std::string BuildGrenadeJSON()
{
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) return R"({"count":0,"name":"","hasGrenade":false})";

    try {
        auto* proc = player->currentProcess;
        if (!proc || !proc->middleHigh) return R"({"count":0,"name":"","hasGrenade":false})";

        RE::BSAutoLock lk{ proc->middleHigh->equippedItemsLock };

        static int s_gd = 0;
        const bool grenDbg = (s_gd++ % 25) == 0;

        for (auto& eq : proc->middleHigh->equippedItems) {
            if (!eq.item.object) continue;
            auto* form = eq.item.object->As<RE::TESForm>();
            if (!form || form->formType != RE::ENUM_FORM_ID::kWEAP) continue;
            auto* weap = form->As<RE::TESObjectWEAP>();
            if (!weap) continue;
            const std::uint8_t wtype = weap->weaponData.type.underlying();
            if (grenDbg) {
                auto dsv = RE::TESFullName::GetFullName(*form);
                logger::debug("PrismaHUD grenade-scan: equipped WEAP formID={:08X} type={} name='{}'",
                             form->GetFormID(), static_cast<int>(wtype),
                             dsv.empty() ? "" : std::string(dsv));
            }
            if (wtype != static_cast<std::uint8_t>(RE::WEAPON_TYPE::kGrenade) &&
                wtype != static_cast<std::uint8_t>(RE::WEAPON_TYPE::kMine)) continue;

            auto sv = RE::TESFullName::GetFullName(*form);
            std::string name = sv.empty() ? "" : std::string(sv);
            int32_t count = 0;
            auto formID = form->GetFormID();

            if (player->inventoryList) {
                player->inventoryList->ForEachStack(
                    [](RE::BGSInventoryItem&) { return true; },
                    [&](RE::BGSInventoryItem& item, RE::BGSInventoryItem::Stack& stack) {
                        if (item.object && item.object->GetFormID() == formID)
                            count += static_cast<int32_t>(stack.count);
                        return true;
                    });
            }
            return nlohmann::json{{"count", count}, {"name", name}, {"hasGrenade", true}}.dump();
        }
    } catch (...) {}

    return R"({"count":0,"name":"","hasGrenade":false})";
}

static std::string BuildSurvivalJSON()
{
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) return R"({"hunger":0,"thirst":0,"sleep":0,"active":false})";

    static RE::ActorValueInfo* s_avHunger      = nullptr;
    static RE::ActorValueInfo* s_avThirst      = nullptr;
    static bool                s_survivalDone  = false;
    if (!s_survivalDone) {
        s_survivalDone = true;
        s_avHunger = RE::TESForm::GetFormByEditorID<RE::ActorValueInfo>("Hunger");
        s_avThirst = RE::TESForm::GetFormByEditorID<RE::ActorValueInfo>("Dehydration");
        if (!s_avHunger) logger::warn("PrismaHUD: Hunger AV not found");
        if (!s_avThirst) logger::warn("PrismaHUD: Dehydration AV not found");
    }

    auto* av    = RE::ActorValue::GetSingleton();
    float sleep  = GetAV(player, &av->fatigue);
    float hunger = GetAV(player, &s_avHunger);
    float thirst = GetAV(player, &s_avThirst);

    bool active = false;
    try {
        auto lvl = player->GetDifficultyLevel();
        active = (lvl == RE::DifficultyLevel::kSurvival) || (lvl == RE::DifficultyLevel::kTrueSurvival);
    } catch (...) {}

    return nlohmann::json{
        {"hunger", hunger},
        {"thirst", thirst},
        {"sleep",  sleep},
        {"active", active},
    }.dump();
}

static std::string BuildPromptJSON()
{
    try {
        auto* qlc = QuickLootController::GetSingleton();

        if (qlc->Active() && !qlc->IsWorkshopBenchTarget() && !qlc->UserHidden())
            return R"({"text":"","subtext":"","hasTarget":false})";

        auto handle = qlc->CurrentCrosshairPick();
        auto* ref   = handle.get().get();
        if (!ref || ref->IsDeleted()) return R"({"text":"","subtext":"","hasTarget":false})";

        auto* base  = ref->GetObjectReference();
        auto* bound = base ? base->As<RE::TESBoundObject>() : nullptr;
        if (!bound) return R"({"text":"","subtext":"","hasTarget":false})";

        RE::BSStringT<char> activateText;
        bound->GetActivateText(ref, activateText);
        std::string text = activateText.empty() ? "" : std::string(activateText.c_str());

        std::string targetName;
        auto sv = RE::TESFullName::GetFullName(*base);
        if (!sv.empty()) targetName = std::string(sv);

        if (!targetName.empty() && text.size() > targetName.size() &&
            text.compare(0, targetName.size(), targetName) == 0) {
            std::string verb = text.substr(targetName.size());
            const auto first = verb.find_first_not_of(" 	");
            if (first != std::string::npos) {
                verb.erase(0, first);
                if (!verb.empty()) text = verb;
            }
        }

        if (qlc->IsWorkshopBenchTarget()) text = "Activate Workshop";

        nlohmann::json extras = nlohmann::json::array();
        if (qlc->IsWorkshopBenchTarget()) {
            const std::string perkLabel = QuickLootController::EnginePerkButtonLabel();
            auto transferKey = ControlLabels::ForHUDAction("bench_transfer");
            const auto transferCode = ControlLabels::GamepadCodeForHUDAction("bench_transfer");
            const auto perkCode = perkLabel.empty() ? 0u : ControlLabels::GamepadCodeForHUDAction("perk");
            const bool transferCollidesWithPerk = ControlLabels::UsingGamepad() &&
                                                   transferCode != 0u && perkCode != 0u &&
                                                   perkCode == transferCode;

            if (!transferCollidesWithPerk) {
                extras.push_back({{"key", transferKey.empty() ? "R" : transferKey}, {"label", "Transfer"}});
            }

            if (!perkLabel.empty()) {
                std::string perkKey = ControlLabels::ForHUDAction("perk");
                if (perkKey.empty()) perkKey = "Space";
                extras.push_back({{"key", perkKey}, {"label", perkLabel}});
            } else {
                static std::string s_lastDiag;
                std::string diag = "no-state";
                try {
                    if (auto* fav = RE::FavoritesManager::GetSingleton()) {
                        auto& cached = fav->quickContainerMode.eventDataStruct;
                        if (cached.optionalValue.has_value()) {
                            const auto& qc = *cached.optionalValue;
                            diag = std::format("populated mode={} perkText='{}' perkEnabled={}",
                                               static_cast<int>(qc.mode.underlying()),
                                               qc.perkButtonText.c_str(), qc.perkButtonEnabled);
                        } else {
                            diag = "NOT POPULATED (optionalValue empty)";
                        }
                    }
                } catch (...) {}
                if (diag != s_lastDiag) {
                    s_lastDiag = diag;
                    logger::info("PrismaHUD DIAG: bench has NO perk button -- quick-container state: {}",
                                 diag);
                }
            }
        }

        for (auto& c : ActivateChoiceRegistry::GetSingleton()->BuildChoicesJSON())
            extras.push_back({{"key", c.value("key", "")}, {"label", c.value("label", "")},
                              {"id", c.value("id", "")}, {"icon", c.value("icon", "")}});

        auto activateKey = ControlLabels::ForUserEvent("Activate");
        if (activateKey.empty()) activateKey = "E";

        return nlohmann::json{
            {"text",        text},
            {"subtext",     targetName},
            {"hasTarget",   !text.empty()},
            {"extras",      extras},
            {"activateKey", activateKey},
            {"controllerStyle", ControlLabels::GetStyle() == ControlLabels::Style::kPlayStation ? "playstation" : "xbox"},
        }.dump();
    } catch (...) {
        return R"({"text":"","subtext":"","hasTarget":false})";
    }
}

static void OnPositionReport(const char* json)
{
    try {
        auto j  = nlohmann::json::parse(json);
        auto id = j.at("id").get<std::string>();
        HUDLayoutManager::GetSingleton()->UpdatePositionFromReport(id.c_str(), json);
    } catch (...) {}
}

void HUDDataProvider::BindPositionReport(PrismaView view, const char* )
{
    if (!m_api) return;
    m_api->BindUIEvent(view, "reportPosition", OnPositionReport);
}

void HUDDataProvider::PollCombatThrottled()
{
    auto* lm = HUDLayoutManager::GetSingleton();
    if (!lm->GetCombatOnly()) return;
    if (!lm->ShouldPollCombat()) return;
    bool inCombat = false;
    try {
        auto* pc = RE::PlayerCharacter::GetSingleton();
        if (pc) inCombat = pc->IsInCombat();
    } catch (...) {}
    lm->UpdateCombatState(inCombat);
}

void HUDDataProvider::ResetTransientForLoad()
{
    std::lock_guard<std::recursive_mutex> lock(DataLock());
    m_lastPayload.clear();
    m_lastPrompt.clear();
    ResetDataBuilderCaches();
}

std::recursive_mutex& HUDDataProvider::DataLock()
{
    static std::recursive_mutex m;
    return m;
}

#define BIND(ViewField, Builder, JSFn) \
    if (!m_api) return; \
    m_api->BindUIEvent(view, "requestUpdate", [](const char*) { \
        auto* s = HUDDataProvider::GetSingleton(); \
        if (!s->m_api || !s->m_views.ViewField) return; \
        std::string payload; \
        { \
            std::lock_guard<std::recursive_mutex> lock(HUDDataProvider::DataLock()); \
            HUDDataProvider::PollCombatThrottled(); \
            if (!HUDLayoutManager::GetSingleton()->IsMasterVisible()) return; \
            auto j = Builder(); \
            auto& last = s->m_lastPayload[s->m_views.ViewField]; \
            if (j == last) return; \
            last = j; \
            payload = std::move(j); \
        } \
        s->m_api->InteropCall(s->m_views.ViewField, JSFn, payload.c_str()); \
    });

void HUDDataProvider::BindHealthUpdate(PrismaView view)     { BIND(health,    BuildHealthJSON,    "updateStats")    }
void HUDDataProvider::BindAPUpdate(PrismaView view)         { BIND(ap,        BuildAPJSON,        "updateStats")    }
void HUDDataProvider::BindCompassUpdate(PrismaView view)    { BIND(compass,   BuildCompassJSON,   "updateHeading")  }
void HUDDataProvider::BindRadsUpdate(PrismaView view)       { BIND(rads,      BuildRadsJSON,      "updateStats")    }
void HUDDataProvider::BindAmmoUpdate(PrismaView view)       { BIND(ammo,      BuildAmmoJSON,      "updateStats")    }
void HUDDataProvider::BindGrenadeUpdate(PrismaView view)    { BIND(grenade,   BuildGrenadeJSON,   "updateStats")    }
void HUDDataProvider::BindSurvivalUpdate(PrismaView view)   { BIND(survival,  BuildSurvivalJSON,  "updateStats")    }

#undef BIND

void HUDDataProvider::PushPromptNow()
{
    if (!m_api || !m_views.prompt) return;
    std::string payload;
    {
        std::lock_guard<std::recursive_mutex> lock(DataLock());
        if (!HUDLayoutManager::GetSingleton()->IsMasterVisible()) return;
        auto j = BuildPromptJSON();
        if (j == m_lastPrompt) return;
        m_lastPrompt = j;
        payload = std::move(j);
    }
    m_api->InteropCall(m_views.prompt, "updatePrompt", payload.c_str());
}

void HUDDataProvider::BindPromptUpdate(PrismaView view)
{
    if (!m_api) return;
    m_api->BindUIEvent(view, "requestUpdate", [](const char*) {
        auto* s = HUDDataProvider::GetSingleton();
        if (!s->m_api || !s->m_views.prompt) return;
        std::string payload;
        {
            std::lock_guard<std::recursive_mutex> lock(HUDDataProvider::DataLock());
            HUDDataProvider::PollCombatThrottled();
            if (!HUDLayoutManager::GetSingleton()->IsMasterVisible()) return;
            auto j = BuildPromptJSON();
            if (j == s->m_lastPrompt) return;
            s->m_lastPrompt = j;
            payload = std::move(j);
        }
        s->m_api->InteropCall(s->m_views.prompt, "updatePrompt", payload.c_str());
    });
}
