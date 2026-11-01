#include "Live.h"
#include "Mods.h"
#include "UiMailbox.h"
#include <F4SE/F4SE.h>
#include <RE/Fallout.h>
#include <Windows.h>
#include <spdlog/spdlog.h>
#if defined(PHOSPHOR_AE)
#include "../ae/AeRuntime.h"
#endif
#include <algorithm>
#include <atomic>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <chrono>
#include <map>
#include <numbers>
#include <optional>
#include <vector>

namespace Phosphor::Live {
namespace {
// Menus the HUD knows (MENU_CTX in hud.html). Anything else is left out of context.menus.
constexpr const char* kKnownMenus[] = {
    "PipboyMenu", "ContainerMenu", "BarterMenu", "ExamineMenu", "TerminalMenu", "LockpickingMenu",
    "LevelUpMenu", "SleepWaitMenu", "PauseMenu", "Console", "LooksMenu", "BookMenu", "LoadingMenu",
    "MainMenu", "PowerArmorModMenu", "SPECIALMenu", "CreditsMenu", "CookingMenu", "VATSMenu",
    "DialogueMenu", "ScopeMenu", "WorkshopMenu", "FavoritesMenu", "WheelMenu", "ScreenArcherMenu",
    "PhotoMenu", "NAFMenu", "NAFStudioMenu", "FalloutEmotions2Menu", "OCM_outfitChangeMenu",
    "AddItemMenu", "TextInputMenu"};
// The ones that map to the HUD's "menu" situation, which also freezes its timers and clock.
constexpr std::string_view kPausingMenus[] = {
    "PipboyMenu", "ContainerMenu", "BarterMenu", "ExamineMenu", "TerminalMenu", "LockpickingMenu",
    "LevelUpMenu", "SleepWaitMenu", "PauseMenu", "Console", "LooksMenu", "BookMenu", "LoadingMenu",
    "MainMenu", "PowerArmorModMenu", "SPECIALMenu", "CreditsMenu", "CookingMenu", "NAFMenu",
    "NAFStudioMenu", "FalloutEmotions2Menu", "OCM_outfitChangeMenu", "AddItemMenu", "TextInputMenu"};

std::string Name(const RE::TESForm* form) {
    return form ? ToUtf8(RE::TESFullName::GetFullName(*form)) : std::string{};
}
float Value(RE::Actor& actor, RE::ActorValueInfo* info) { return info ? actor.GetActorValue(*info) : 0.0F; }
float Permanent(RE::Actor& actor, RE::ActorValueInfo* info) { return info ? actor.GetPermanentActorValue(*info) : 0.0F; }
int Round(float value) { return static_cast<int>(std::lround(value)); }
double Round1(double value, double step) { return std::round(value / step) * step; }

// TESObjectWEAP's IsThrownWeapon()/IsGunWeapon() test the type as bit flags, but WEAPON_TYPE is a
// plain value (kGun 9, kGrenade 10, kMine 11 share bits), so a gun can read as a grenade. Compare values.
RE::WEAPON_TYPE TypeOf(const RE::TESObjectWEAP& weapon) { return weapon.weaponData.type.get(); }
bool IsThrown(const RE::TESObjectWEAP& weapon) {
    const auto type = TypeOf(weapon);
    return type == RE::WEAPON_TYPE::kGrenade || type == RE::WEAPON_TYPE::kMine;
}
bool IsGun(const RE::TESObjectWEAP& weapon) { return TypeOf(weapon) == RE::WEAPON_TYPE::kGun; }

struct Equipped {
    RE::TESObjectWEAP* weapon = nullptr;
    RE::TESObjectWEAP::InstanceData* instance = nullptr;
    RE::TESAmmo* ammo = nullptr;
    int clip = 0;
    int clipMax = 0;
    RE::TESObjectWEAP* thrown = nullptr;  // grenade or mine in the throwable slot
};

Equipped ReadEquipped(RE::Actor& actor) {
    Equipped out;
    auto* process = actor.currentProcess;
    auto* middleHigh = process ? process->middleHigh : nullptr;
    if (!middleHigh) return out;
    RE::BSAutoLock lock{middleHigh->equippedItemsLock};
    for (auto& equipped : middleHigh->equippedItems) {
        auto* weapon = equipped.item.object ? equipped.item.object->As<RE::TESObjectWEAP>() : nullptr;
        if (!weapon) continue;
        if (IsThrown(*weapon)) {
            if (!out.thrown) out.thrown = weapon;
            continue;
        }
        if (equipped.equipIndex.index != 0 || out.weapon) continue;
        out.weapon = weapon;
        out.instance = static_cast<RE::TESObjectWEAP::InstanceData*>(equipped.item.instanceData.get());
        const RE::TESObjectWEAP::InstanceData& data = out.instance ? *out.instance : weapon->weaponData;
        auto* state = equipped.data ? RE::fallout_cast<RE::EquippedWeaponData*>(equipped.data.get()) : nullptr;
        out.ammo = state && state->ammo ? state->ammo : data.ammo;
        if (out.ammo && IsGun(*weapon)) {
            out.clipMax = data.ammoCapacity;
            out.clip = state ? static_cast<int>(state->ammoCount) : 0;
        }
    }
    return out;
}

struct InventoryRead {
    int ammo = 0;
    int thrown = 0;
    std::string weaponName;
    std::vector<std::string> legendary;  // legendary effects on the equipped weapon (LEO slots included)
};

InventoryRead ReadInventory(RE::PlayerCharacter& player, const Equipped& eq) {
    InventoryRead out;
    if (!player.inventoryList) return out;
    auto& inventory = *player.inventoryList;
    RE::BSAutoReadLock lock{inventory.rwLock};
    for (auto& item : inventory.data) {
        if (!item.object) continue;
        if (eq.ammo && item.object == eq.ammo) out.ammo += static_cast<int>(item.GetCount());
        if (eq.thrown && item.object == eq.thrown) out.thrown += static_cast<int>(item.GetCount());
        if (eq.weapon && item.object == eq.weapon && out.weaponName.empty()) {
            // The equipped stack's display name carries renames and legendary prefixes.
            std::uint32_t id = 0;
            for (auto* stack = item.stackData.get(); stack; stack = stack->nextStack.get(), ++id) {
                if (!stack->IsEquipped()) continue;
                if (const char* name = item.GetDisplayFullName(id); name && *name) out.weaponName = ToUtf8(name);
                out.legendary = Mods::LegendaryEffects(stack->extra.get());
                break;
            }
        }
    }
    return out;
}
}

std::string CleanName(std::string name, bool cutPipes) {
    if (cutPipes) if (const auto bar = name.find(" | "); bar != std::string::npos) name.resize(bar);
    for (;;) {
        const auto start = name.find_first_not_of(" \t");
        if (start == std::string::npos) return {};
        const char open = name[start];
        const char close = open == '[' ? ']' : open == '(' ? ')' : open == '{' ? '}' : 0;
        if (!close) { name.erase(0, start); break; }
        const auto end = name.find(close, start + 1);
        if (end == std::string::npos) { name.erase(0, start); break; }
        const auto rest = name.find_first_not_of(" \t", end + 1);
        if (rest == std::string::npos) { name.erase(0, start); break; }  // the whole name is a tag: keep it
        name.erase(0, end + 1);
    }
    // Trailing component lists from sorting mods ("Tin Can {{{Spring}}}").
    if (const auto brace = name.find("{{{"); brace != std::string::npos && brace > 0 && name.find("}}}", brace) != std::string::npos)
        name.resize(brace);
    while (!name.empty() && (name.back() == ' ' || name.back() == '\t')) name.pop_back();
    return name;
}

std::string Untranslate(std::string text) {
    // Scaleform translation keys ("$TAKE", "$Take_All") reach us before the movie translates them.
    if (text.size() < 2 || text[0] != '$') return text;
    text.erase(0, 1);
    bool start = true;
    for (auto& c : text) {
        if (c == '_') c = ' ';
        if (c == ' ') { start = true; continue; }
        c = static_cast<char>(start ? std::toupper(static_cast<unsigned char>(c)) : std::tolower(static_cast<unsigned char>(c)));
        start = false;
    }
    return text;
}

std::string ToUtf8(std::string_view text) {
    // Plugin strings are usually cp1252 (what xEdit and the CK write); valid UTF-8 passes through.
    if (text.empty()) return {};
    const int length = static_cast<int>(text.size());
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), length, nullptr, 0) > 0) return std::string(text);
    const int wide = MultiByteToWideChar(1252, 0, text.data(), length, nullptr, 0);
    std::wstring buffer(static_cast<std::size_t>(wide), L'\0');
    MultiByteToWideChar(1252, 0, text.data(), length, buffer.data(), wide);
    const int bytes = WideCharToMultiByte(CP_UTF8, 0, buffer.data(), wide, nullptr, 0, nullptr, nullptr);
    std::string result(static_cast<std::size_t>(bytes), '\0');
    WideCharToMultiByte(CP_UTF8, 0, buffer.data(), wide, result.data(), bytes, nullptr, nullptr);
    return result;
}

double Heading() {
#if defined(PHOSPHOR_AE)
    AE_GATE("bCompass", 0.0);
#endif
    // The compass follows the camera, like vanilla's. In first person that is the player's own facing. In third
    // person, mods such as True Third Person turn the character independently of the camera, so there the heading
    // comes from the camera: the w row of its world-to-camera matrix is its view direction (Mines::Project relies
    // on the same row for depth). 0 faces north (+Y) and grows clockwise.
    auto* player = RE::PlayerCharacter::GetSingleton();
    double heading = player ? player->data.angle.z * 180.0 / std::numbers::pi : 0.0;
    auto* pc = RE::PlayerCamera::GetSingleton();
    const bool third = pc && pc->currentState && pc->currentState->id.get() == RE::CameraStates::k3rdPerson;
    auto* camera = RE::Main::WorldRootCamera();
    if (camera) {
        const auto& m = camera->worldToCam;
        const double fx = m[3][0], fy = m[3][1];
        if (fx * fx + fy * fy > 1e-6) {
            double cam = std::fmod(std::atan2(fx, fy) * 180.0 / std::numbers::pi, 360.0);
            if (cam < 0) cam += 360.0;
            double own = std::fmod(heading, 360.0);
            if (own < 0) own += 360.0;
            static int loggedFirst = 0, loggedThird = 0;
            int& n = third ? loggedThird : loggedFirst;
            if (n < 3) {
                spdlog::info("Compass ({}): camera {:.0f}, character {:.0f}", third ? "third person" : "first person", cam, own);
                ++n;
            }
            if (third) heading = cam;
        }
    }
    heading = std::fmod(heading, 360.0);
    return heading < 0 ? heading + 360.0 : heading;
}

Json ReadFast(Raw& raw) {
    Json out = Json::object();
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) { raw.valid = false; return out; }
    raw.valid = true;
    out["heading"] = Round1(Heading(), 0.5);
    const auto eq = ReadEquipped(*player);
    out["clip"] = eq.clip;
    return out;
}

Json ReadVitals(Raw& raw) {
    Json out = Json::object();
    auto* player = RE::PlayerCharacter::GetSingleton();
    auto* av = RE::ActorValue::GetSingleton();
    if (!player || !av) return out;

    const int hp = Round(Value(*player, av->health));
    out["hp"] = hp;
    out["hpMax"] = std::max(hp, Round(Permanent(*player, av->health)));
    const int ap = Round(Value(*player, av->actionPoints));
    out["ap"] = ap;
    out["apMax"] = std::max(ap, Round(Permanent(*player, av->actionPoints)));
    raw.radsValue = Value(*player, av->rads);
    raw.radsPermanent = Permanent(*player, av->rads);
    out["rads"] = std::clamp(Round(raw.radsValue), 0, 1000);
    return out;
}

Json ReadSlow(Raw& raw) {
    Json out = Json::object();
    auto* player = RE::PlayerCharacter::GetSingleton();
    auto* av = RE::ActorValue::GetSingleton();
    if (!player || !av) { raw.valid = false; return out; }
    raw.valid = true;

    const auto limb = [&](RE::ActorValueInfo* info) {
        const float max = std::max(1.0F, Permanent(*player, info));
        return Round1(std::clamp(Value(*player, info) / max, 0.0F, 1.0F), 0.01);
    };
    out["limbs"] = Json{{"head", limb(av->perceptionCondition)}, {"torso", limb(av->enduranceCondition)},
                        {"larm", limb(av->leftAttackCondition)}, {"rarm", limb(av->rightAttackCondition)},
                        {"lleg", limb(av->leftMobiltyCondition)}, {"rleg", limb(av->rightMobilityCondition)}};

    raw.level = player->GetLevel();
    raw.experience = Value(*player, av->experience);
    raw.perks = std::max<int>(0, player->perkCount);
    out["level"] = raw.level;
    out["xp"] = Round1(Phosphor::XpFraction(raw.level, raw.experience), 0.001);
    out["perkPoints"] = raw.perks;

    std::string location, region;
    auto* place = player->GetCurrentLocation();
    location = Name(place);
    auto* cell = player->GetParentCell();
    if (location.empty() && cell) location = Name(cell);
    if (cell && cell->IsExterior() && cell->worldSpace) region = Name(cell->worldSpace);
    if (region.empty() && place) {
        auto* root = place;
        while (root->parentLoc && root->parentLoc != root) root = root->parentLoc;
        if (root != place) region = Name(root);
    }
    static std::string lastRegion;
    if (region.empty()) region = lastRegion; else lastRegion = region;
    out["location"] = location;
    out["region"] = region;
    out["combat"] = player->IsInCombat();

    const auto eq = ReadEquipped(*player);
    const auto inv = ReadInventory(*player, eq);
    out["weapon"] = CleanName(!inv.weaponName.empty() ? inv.weaponName : Name(eq.weapon), true);
    out["weaponLegendary"] = inv.legendary;
    out["clip"] = eq.clip;
    out["clipMax"] = eq.clipMax;
    out["reserve"] = eq.clipMax > 0 ? std::max(0, inv.ammo - eq.clip) : 0;
    raw.ammoInInventory = eq.ammo ? inv.ammo : -1;
    if (eq.thrown && inv.thrown > 0) out["explosive"] = Json{{"name", CleanName(Name(eq.thrown), true)}, {"count", inv.thrown}};
    else out["explosive"] = nullptr;

    if (auto* calendar = RE::Calendar::GetSingleton()) {
        if (calendar->gameHour) raw.hour = calendar->gameHour->value;
        if (calendar->timeScale) raw.timescale = calendar->timeScale->value;
    }

    Json menus = Json::array();
    raw.menuOpen = false;
    if (auto* ui = RE::UI::GetSingleton()) {
        for (const auto* name : kKnownMenus) {
            if (!ui->GetMenuOpen(RE::BSFixedString(name))) continue;
            menus.push_back(name);
            for (auto pausing : kPausingMenus) if (pausing == name) raw.menuOpen = true;
        }
    }
    raw.powerArmor = RE::PowerArmor::PlayerInPowerArmor();
    const auto gun = static_cast<RE::GUN_STATE>(player->gunState);
    bool photo = false;
    for (const auto& m : menus) if (m == "PhotoMenu" || m == "ScreenArcherMenu") photo = true;
#if defined(PHOSPHOR_AE)
    const bool thirdPerson = AE::options.ReadIf("bCrosshair", [] {
        auto* camera = RE::PlayerCamera::GetSingleton();
        return camera && camera->currentState && camera->currentState->id.get() == RE::CameraStates::k3rdPerson;
    }, false);
#else
    auto* camera = RE::PlayerCamera::GetSingleton();
    const bool thirdPerson = camera && camera->currentState && camera->currentState->id.get() == RE::CameraStates::k3rdPerson;
#endif
    out["context"] = Json{{"menus", menus}, {"powerArmor", raw.powerArmor},
                          {"ads", gun == RE::GUN_STATE::kSighted || gun == RE::GUN_STATE::kFireSighted},
                          {"thirdPerson", thirdPerson},
                          {"scene", Mods::PlayerInScene()}, {"photo", photo}, {"hudHidden", LatestNativeHud().hidden}};
    return out;
}

namespace {
bool HasKeyword(const RE::BGSKeywordForm& form, std::initializer_list<std::string_view> names) {
    bool found = false;
    form.ForEachKeyword([&](RE::BGSKeyword* keyword) {
        const std::string_view id = keyword ? keyword->GetFormEditorID() : "";
        for (auto name : names) if (id == name) { found = true; return RE::BSContainer::ForEachResult::kStop; }
        return RE::BSContainer::ForEachResult::kContinue;
    });
    return found;
}
bool Contains(std::string_view text, std::string_view part) {
    if (part.size() > text.size()) return false;
    for (std::size_t i = 0; i + part.size() <= text.size(); ++i) {
        bool same = true;
        for (std::size_t j = 0; j < part.size() && same; ++j)
            same = std::tolower(static_cast<unsigned char>(text[i + j])) == std::tolower(static_cast<unsigned char>(part[j]));
        if (same) return true;
    }
    return false;
}
// Effect tray look for a consumable: kind (colour) and icon, from its flags, keywords and name.
std::pair<const char*, const char*> Classify(const RE::AlchemyItem& item, std::string_view name) {
    constexpr std::uint32_t kFood = 0x2, kMedicine = 0x10000;
    const auto flags = item.data.flags;
    const bool addictive = item.data.addictionItem != nullptr;
    const bool drink = HasKeyword(item, {"ObjectTypeDrink", "ObjectTypeNukaCola", "ObjectTypeAlcohol", "ObjectTypeWater"}) ||
                       Contains(name, "nuka") || Contains(name, "water") || Contains(name, "sarsaparilla") ||
                       Contains(name, "beer") || Contains(name, "whiskey") || Contains(name, "vodka") ||
                       Contains(name, "bourbon") || Contains(name, "wine") || Contains(name, "rum");
    if (Contains(name, "radaway")) return {"med", "ivbag"};
    if (Contains(name, "stimpak")) return {"med", "syringe"};
    if (Contains(name, "psycho")) return {"chem", "bolt"};
    if (Contains(name, "med-x")) return {"chem", "shield"};
    if (Contains(name, "jet")) return {"chem", "hourglass"};
    if (Contains(name, "buff")) return {"chem", "dumbbell"};
    if (drink) return {"drink", "bottle"};
    if ((flags & kFood) || HasKeyword(item, {"ObjectTypeFood"})) return {"food", "food"};
    if (addictive || HasKeyword(item, {"ObjectTypeChem"})) return {"chem", "capsule"};
    if (flags & kMedicine) return {"med", "capsule"};
    return {"med", "capsule"};
}
}

Json ReadEffects(std::string& key, float maxSeconds) {
#if defined(PHOSPHOR_AE)
    AE_GATE("bEffects", Json::array());
#endif
    Json out = Json::array();
    key.clear();
    auto* player = RE::PlayerCharacter::GetSingleton();
    auto* list = player ? player->GetActiveEffectList() : nullptr;
    if (!list) return out;
    struct Group { const RE::AlchemyItem* item; float duration; float remaining; };
    std::vector<Group> groups;
    using Flag = RE::ActiveEffect::Flags;
    for (auto& entry : list->data) {
        auto* effect = entry.get();
        if (!effect || !effect->spell) continue;
        if (effect->flags.any(Flag::kInactive, Flag::kDispelled, Flag::kWornOff)) continue;
        auto* item = effect->spell->As<RE::AlchemyItem>();
        if (!item || effect->duration <= 0) continue;
        if (maxSeconds > 0 && effect->duration > maxSeconds) continue;  // "permanent" status items
        const float remaining = effect->duration - effect->elapsedSeconds;
        if (remaining <= 0) continue;
        auto it = std::find_if(groups.begin(), groups.end(), [&](const Group& g) { return g.item == item; });
        if (it == groups.end()) groups.push_back({item, effect->duration, remaining});
        else { it->duration = std::max(it->duration, effect->duration); it->remaining = std::max(it->remaining, remaining); }
    }
    std::sort(groups.begin(), groups.end(), [](const Group& a, const Group& b) { return a.remaining < b.remaining; });
    if (groups.size() > 10) groups.resize(10);
    for (const auto& g : groups) {
        char id[16];
        std::snprintf(id, sizeof(id), "%08x", g.item->GetFormID());
        const auto full = Name(g.item);
        const auto [kind, icon] = Classify(*g.item, full);
        const auto name = CleanName(full, false);
        out.push_back(Json{{"id", id}, {"name", name}, {"kind", kind}, {"icon", icon},
                           {"duration", Round1(g.duration, 1)}, {"remaining", Round1(g.remaining, 0.1)}});
        key += id;
        key += ',';
    }
    return out;
}

namespace {
// Creation Kit map marker types -> the compass's icon set (nullptr: decide by name).
const char* MarkerType(int type) {
    switch (type) {
    case 0: case 6: case 14: return "cave";                                 // cave, metro, sewer/utility tunnels
    case 15: return "vault";
    case 12: case 13: case 66: case 67: case 68: return "settlement";       // sanctuary, settlement, Nuka-World outposts
    case 1: case 2: case 9: case 10: case 11: case 17: case 22: case 25: case 29: case 43: case 49:
    case 51: case 54: case 55: case 56: case 58: return "city";             // towns, civic and city buildings
    case 4: case 7: case 16: case 32: case 33: case 36: case 40: case 52: return "factory";  // industry, military, junk
    case 3: case 5: case 8: case 18: case 19: case 20: case 21: case 23: case 24: case 26: case 27: case 28:
    case 30: case 31: case 34: case 35: case 37: case 38: case 39: case 41: case 42: case 44: case 45: case 46:
    case 47: case 48: case 50: case 53: case 57: case 59: case 60: case 61: case 62: case 63: case 64: case 65:
        return "landmark";
    default: return nullptr;
    }
}
}

namespace {
// Vanilla's compass shows a marker's up / down arrow beyond fQuestMarkerZDiffThreshold (250 units, about 3.6 m).
double HeightThreshold() {
#if defined(PHOSPHOR_AE)
    AE_GATE("bMarkerHeights", 250.0);
#endif
    if (auto* gmst = RE::GameSettingCollection::GetSingleton()) {
        auto* setting = gmst->GetSetting("fQuestMarkerZDiffThreshold:Interface");
        if (!setting) setting = gmst->GetSetting("fQuestMarkerZDiffThreshold");
        if (setting && setting->GetType() == RE::Setting::SETTING_TYPE::kFloat && setting->GetFloat() > 0) return setting->GetFloat();
    }
    return 250.0;
}
// Adds "dz" (meters, signed) when the height difference passes the vanilla threshold.
void AddHeight(Json& marker, double dzUnits, double threshold) {
    if (std::abs(dzUnits) <= threshold) return;
    const long meters = std::lround(dzUnits / 70.0);
    marker["dz"] = meters != 0 ? meters : (dzUnits > 0 ? 1 : -1);
}
// Map marker heights by reference form ID; map markers are persistent references and never move.
std::map<std::uint32_t, std::optional<double>> mapMarkerZ;
std::optional<double> MapMarkerZ(std::uint32_t formId) {
#if defined(PHOSPHOR_AE)
    AE_GATE("bMarkerHeights", std::nullopt);
#endif
    if (!formId) return std::nullopt;
    auto it = mapMarkerZ.find(formId);
    if (it != mapMarkerZ.end()) return it->second;
    std::optional<double> z;
    if (auto* ref = RE::TESForm::GetFormByID<RE::TESObjectREFR>(formId)) z = ref->data.location.z;
    mapMarkerZ.emplace(formId, z);
    return z;
}
}

namespace {
// Quest marker heights. The Pip-Boy feed's "Height" is 0 for most markers, so the height comes from the game
// instead: the marker's X/Y are the target reference's own position (or its door's), so a loaded reference at that
// spot gives its Z. Refreshed every ten seconds for markers inside the loaded area.
struct MarkerZ { double x = 0, y = 0; std::optional<double> z; std::chrono::steady_clock::time_point tried{}; };
std::map<std::uint32_t, MarkerZ> markerZ;
std::chrono::steady_clock::time_point lastZScan{};
std::uint32_t markerSpace = 0;

void ResolveMarkerHeights(const std::vector<std::pair<std::uint32_t, std::pair<double, double>>>& want) {
#if defined(PHOSPHOR_AE)
    AE_GATE("bMarkerHeights", );
#endif
    auto* player = RE::PlayerCharacter::GetSingleton();
    auto* cell = player ? player->GetParentCell() : nullptr;
    if (!cell || want.empty()) return;
    std::vector<RE::TESObjectCELL*> cells{cell};
    if (cell->IsExterior()) {
        if (auto* data = RE::TESDataHandler::GetSingleton()) {
            for (auto* other : data->GetFormArray<RE::TESObjectCELL>()) {
                if (other && other != cell && other->IsExterior() && other->worldSpace == cell->worldSpace &&
                    #if defined(PHOSPHOR_AE)
                    other->cellState == RE::TESObjectCELL::CELL_STATE::kAttached
#else
                    other->cellState.get() == RE::TESObjectCELL::CELL_STATE::kAttached
#endif
                    )
                    cells.push_back(other);
            }
        }
    }
    std::vector<double> best(want.size(), 1e9);
    std::vector<std::optional<double>> found(want.size());
    for (auto* c : cells) {
        RE::BSAutoLock guard{c->spinLock};
        for (auto& ref : c->references) {
            if (!ref) continue;
            const auto& p = ref->data.location;
            for (std::size_t i = 0; i < want.size(); ++i) {
                const double d = std::abs(p.x - want[i].second.first) + std::abs(p.y - want[i].second.second);
                if (d <= 4.0 && d < best[i]) { best[i] = d; found[i] = p.z; }
            }
        }
    }
    const auto now = std::chrono::steady_clock::now();
    int hits = 0;
    for (std::size_t i = 0; i < want.size(); ++i) {
        auto& e = markerZ[want[i].first];
        e.x = want[i].second.first; e.y = want[i].second.second; e.z = found[i]; e.tried = now;
        if (found[i]) ++hits;
    }
    static int logged = 0;
    if (logged < 5) { spdlog::info("Quest marker heights: {} of {} found in {} loaded cells", hits, want.size(), cells.size()); ++logged; }
}
}

void ResetCompass() {
    markerZ.clear();
    mapMarkerZ.clear();
    lastZScan = {};
    markerSpace = 0;
}

Json ReadQuests(const Pipboy::Snapshot& pip, double locationRangeMeters) {
#if defined(PHOSPHOR_AE)
    AE_GATE("bCompass", (Json{{"markers", Json::array()}, {"tracker", nullptr}}));
#endif
    Json out{{"markers", Json::array()}, {"tracker", nullptr}};
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player || !pip.connected) return out;
    auto* cell = player->GetParentCell();
    const bool interior = cell && cell->IsInterior();
    // Interior coordinates are local to that cell; exterior coordinates belong to the worldspace.
    // Keep only IDs, never engine references, across ticks or loading screens.
    const auto space = cell ? (interior || !cell->worldSpace ? cell->GetFormID() : cell->worldSpace->GetFormID()) : 0;
    if (space != markerSpace) {
        ResetCompass();
        markerSpace = space;
    }
#if defined(PHOSPHOR_AE)
    const double px = player->data.location.x, py = player->data.location.y;
    const double pz = AE::Enabled("bMarkerHeights") ? player->data.location.z : 0.0;
#else
    const double px = player->data.location.x, py = player->data.location.y, pz = player->data.location.z;
#endif
    const double zThreshold = HeightThreshold();

    // CHALLENGES' "Tracked Challenges" quest has its own panel, so it never takes the quest tracker.
    if (auto pin = CustomWaypointMarker(interior ? pip.customLocal : pip.customWorld, px, py); !pin.is_null())
        out["markers"].push_back(std::move(pin));
    const auto challenges = Mods::ChallengeQuestId();
    std::vector<std::uint32_t> active;
    for (const auto& q : pip.quests) if (q.active) active.push_back(q.formId);
    const auto isActive = [&](const Pipboy::QuestMarker& m) {
        for (auto id : m.quests) if (std::find(active.begin(), active.end(), id) != active.end()) return true;
        return false;
    };

    // Markers are in world coordinates. Inside an interior the player's own coordinates belong to that
    // cell, so only markers close enough to make sense there are kept (the rest are outside the building).
    double nearestTracked = -1;
    const Pipboy::Quest* tracked = nullptr;
    for (const auto& q : pip.quests) if (q.active && q.formId != 0 && q.formId != challenges) { tracked = &q; break; }
    std::vector<std::pair<std::uint32_t, std::pair<double, double>>> heightWanted;
    // Several objectives routed through the same door are one marker ("Door · 3 objectives"), active if any is.
    std::vector<MarkerSpot> spots;
    for (const auto& m : pip.markers) spots.push_back({m.x, m.y, m.onDoor});
    const auto leaders = MergeDoorMarkers(spots);
    std::vector<std::size_t> groupSize(pip.markers.size(), 0);
    std::vector<char> groupActive(pip.markers.size(), 0);
    for (std::size_t i = 0; i < pip.markers.size(); ++i) {
        ++groupSize[leaders[i]];
        if (isActive(pip.markers[i])) groupActive[leaders[i]] = 1;
    }
    for (std::size_t i = 0; i < pip.markers.size(); ++i) {
        const auto& m = pip.markers[i];
        const double dx = m.x - px, dy = m.y - py;
        const double dist = std::sqrt(dx * dx + dy * dy) / 70.0;
        if (interior && dist > 400) continue;
        if (dist > 40000) continue;
        if (tracked && std::find(m.quests.begin(), m.quests.end(), tracked->formId) != m.quests.end())
            if (nearestTracked < 0 || dist < nearestTracked) nearestTracked = dist;
        if (leaders[i] != i) continue;  // drawn by its group's first marker
        double b = std::atan2(dx, dy) * 180.0 / std::numbers::pi;
        if (b < 0) b += 360;
        char id[24];
        std::snprintf(id, sizeof(id), "pq%u", m.pipId);
        Json marker{{"id", id}, {"t", m.onDoor ? "door" : "quest"}, {"b", Round1(b, 0.5)},
                    {"dist", std::lround(dist)}, {"label", DoorGroupLabel(m.name, groupSize[i])},
                    {"active", groupActive[i] != 0}};
        // Engine markers (ids with the top bit set) carry the target's or door's own height.
        if ((m.pipId & 0x80000000u) && m.hasZ) {
#if defined(PHOSPHOR_AE)
            if (AE::Enabled("bMarkerHeights"))
#endif
            if (dist < 300) AddHeight(marker, m.z - pz, zThreshold);
        } else if (dist < 300) {
            // Height: the HUD's ▲/▼, for markers whose spot is inside the loaded area.
            auto it = markerZ.find(m.pipId);
            const bool moved = it != markerZ.end() && (std::abs(it->second.x - m.x) > 4 || std::abs(it->second.y - m.y) > 4);
            const bool stale = it == markerZ.end() || moved ||
                               std::chrono::steady_clock::now() - it->second.tried > std::chrono::seconds(10);
            if (stale) heightWanted.push_back({m.pipId, {m.x, m.y}});
            // Keep the last height during a scheduled refresh so the arrow does not blink off.
            if (it != markerZ.end() && !moved && it->second.z) AddHeight(marker, *it->second.z - pz, zThreshold);
        }
        out["markers"].push_back(std::move(marker));
    }
    if (!heightWanted.empty() && std::chrono::steady_clock::now() - lastZScan > std::chrono::seconds(2)) {
        lastZScan = std::chrono::steady_clock::now();
        if (heightWanted.size() > 24) heightWanted.resize(24);
        ResolveMarkerHeights(heightWanted);
    }

    // Map markers near the player, like the vanilla compass (exteriors only).
    if (!interior && locationRangeMeters > 0) {
        for (const auto& l : pip.locations) {
            if (!l.visible || l.name.empty()) continue;
            const double dx = l.x - px, dy = l.y - py;
            const double dist = std::sqrt(dx * dx + dy * dy) / 70.0;
            if (dist > locationRangeMeters) continue;
            double b = std::atan2(dx, dy) * 180.0 / std::numbers::pi;
            if (b < 0) b += 360;
            // Type numbers follow the Creation Kit's map marker list (0 Cave, 1 City, ... 15 Vault ...); settlements
            // come from the workshop flag; unknown numbers fall back to the name. Logged once per type.
            static std::map<int, bool> typesLogged;
            if (l.type >= 0 && typesLogged.size() < 80 && typesLogged.emplace(l.type, true).second)
                spdlog::info("Map marker type {}: {}", l.type, l.name);
            const char* t = MarkerType(l.type);
            if (l.workshop) t = "settlement";
            else if (t) {}
            else if (Contains(l.name, "vault")) t = "vault";
            else if (Contains(l.name, "cave") || Contains(l.name, "cavern") || Contains(l.name, "grotto") || Contains(l.name, "tunnel")) t = "cave";
            else if (Contains(l.name, "factory") || Contains(l.name, "plant") || Contains(l.name, "works") || Contains(l.name, "mill") ||
                     Contains(l.name, "industr") || Contains(l.name, "yard") || Contains(l.name, "refinery")) t = "factory";
            else if (Contains(l.name, "diamond city") || Contains(l.name, "goodneighbor") || Contains(l.name, "bunker hill") ||
                     Contains(l.name, "covenant") || Contains(l.name, "city")) t = "city";
            else t = "landmark";
            char id[24];
            std::snprintf(id, sizeof(id), "pl%u", l.pipId);
            Json marker{{"id", id}, {"t", t}, {"b", Round1(b, 0.5)}, {"dist", std::lround(dist)},
                        {"label", l.name}, {"disc", l.discovered}};
            if (const auto z = MapMarkerZ(l.markerFormId)) AddHeight(marker, *z - pz, zThreshold);
            out["markers"].push_back(std::move(marker));
        }
    }

    if (tracked) {
        Json objectives = Json::array();
        bool first = true;
        for (const auto& o : tracked->objectives) {
            Json item{{"t", CleanName(o.text, false)}, {"done", o.completed}};
            if (first && !o.completed && nearestTracked >= 0) { item["dist"] = std::lround(nearestTracked); first = false; }
            objectives.push_back(std::move(item));
        }
        Json others = Json::array();
        for (const auto& q : pip.quests)
            if (q.active && &q != tracked && q.formId != challenges && !q.text.empty()) others.push_back(CleanName(q.text, false));
        out["tracker"] = Json{{"title", CleanName(tracked->text, false)}, {"objectives", objectives}, {"others", others}};
    }
    return out;
}

Json ReadLoot(const std::function<std::string(const char*)>& keyFor, std::string* log) {
#if defined(PHOSPHOR_AE)
    AE_GATE("bQuickLoot", nullptr);
#endif
    auto* player = RE::PlayerCharacter::GetSingleton();
    auto* favorites = RE::FavoritesManager::GetSingleton();
#if defined(PHOSPHOR_AE)
    if (!player || !favorites) aeReadScope.Fail();
#endif
    if (!player || !favorites) return nullptr;
    auto& sink = favorites->quickContainerMode;
    RE::BSAutoLock lock{sink.dataLock};
    const auto& stored = sink.eventDataStruct.optionalValue;
    if (!stored.has_value()) return nullptr;
    const auto& d = stored.value();
    const auto container = d.containerRef.get();
    if (!container) return nullptr;
    // The stored event may outlive the box: also require the container in reach and in front of the player.
    const double dx = container->data.location.x - player->data.location.x;
    const double dy = container->data.location.y - player->data.location.y;
    if (std::sqrt(dx * dx + dy * dy) > 450) return nullptr;
    double facing = std::atan2(dx, dy) - player->data.angle.z;
    facing = std::remainder(facing, 2 * std::numbers::pi);
    if (std::abs(facing) > 1.3) return nullptr;

    Json items = Json::array();
    for (std::uint32_t i = 0; i < d.itemData.size(); ++i) {
        const auto& it = d.itemData[i];
        Json item{{"n", CleanName(ToUtf8(it.itemName.c_str()), false)}, {"q", std::max<std::uint32_t>(1, it.itemCount)}};
        if (it.isLegendary) item["leg"] = true;
        items.push_back(std::move(item));
    }
    Json keys = Json::array();
    const auto addKey = [&](const char* event, std::string text) {
        if (text.empty()) return;
        Json k{{"text", Untranslate(CleanName(ToUtf8(text), false))}};
        if (auto key = keyFor(event); !key.empty()) k["key"] = key;
        keys.push_back(std::move(k));
    };
    if (d.buttonAEnabled) addKey("Activate", d.aButtonText.c_str());
    if (d.buttonXEnabled) addKey("ReadyWeapon", "Transfer");
    if (d.perkButtonEnabled) addKey("Jump", d.perkButtonText.c_str());
    using Mode = RE::QuickContainerMode;
    const auto mode = d.mode.get();
    Json out{{"name", CleanName(ToUtf8(d.containerName.c_str()), false)}, {"items", items},
             {"sel", std::max(0, d.selectedClipIndex)}, {"keys", keys}};
    if (d.isLocked) out["locked"] = true;
    if (mode == Mode::kStealing || mode == Mode::kStealingPowerArmor) out["steal"] = true;
    if (log) {
        char line[160];
        std::snprintf(line, sizeof(line), "%08x items %u sel %d mode %d locked %d", container->GetFormID(),
                      d.itemData.size(), d.selectedClipIndex, static_cast<int>(mode), d.isLocked ? 1 : 0);
        *log = line;
    }
    return out;
}

namespace {
std::string SettingValue(RE::Setting& setting) {
    using T = RE::Setting::SETTING_TYPE;
    switch (setting.GetType()) {
    case T::kBinary: return setting.GetBinary() ? "true" : "false";
    case T::kInt: return std::to_string(setting.GetInt());
    case T::kUInt: return std::to_string(setting.GetUInt());
    case T::kFloat: { char b[32]; std::snprintf(b, sizeof(b), "%.2f", setting.GetFloat()); return b; }
    case T::kString: return "\"" + std::string(setting.GetString()) + "\"";
    default: return "?";
    }
}
bool SettingWanted(std::string_view key) {
    std::string k(key);
    for (auto& c : k) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    for (const char* part : {"compass", "hostile", "enemy", "marker"})
        if (k.find(part) != std::string::npos) return true;
    return false;
}
}

void LogCompassSettings() {
#if defined(PHOSPHOR_AE)
    AE_GATE("bCompass", );
#endif
    // Diagnostic: which game and INI settings govern the vanilla compass (enemy marker distance in particular).
    static bool done = false;
    if (done) return;
    done = true;
    int count = 0;
    std::string line;
    const auto add = [&](const char* source, RE::Setting* setting) {
        if (!setting || count >= 120) return;
        const auto key = setting->GetKey();
        if (key.empty() || !SettingWanted(key)) return;
        line += std::string(source) + " " + std::string(key) + "=" + SettingValue(*setting) + "; ";
        ++count;
    };
    if (auto* game = RE::GameSettingCollection::GetSingleton())
        for (auto& entry : game->settings) add("GMST", entry.second);
    if (auto* ini = RE::INISettingCollection::GetSingleton())
        for (auto* setting : ini->settings) add("INI", setting);
    if (auto* pref = RE::INIPrefSettingCollection::GetSingleton())
        for (auto* setting : pref->settings) add("Prefs", setting);
    spdlog::info("Compass-related settings ({}): {}", count, line);
}

Json ReadEnemies() {
#if defined(PHOSPHOR_AE)
    AE_GATE("bEnemies", Json::array());
#endif
    Json out = Json::array();
    auto* player = RE::PlayerCharacter::GetSingleton();
    auto* lists = RE::ProcessLists::GetSingleton();
#if defined(PHOSPHOR_AE)
    if (!player || !lists) aeReadScope.Fail();
#endif
    if (!player || !lists) return out;
    const double px = player->data.location.x, py = player->data.location.y;
    // Vanilla's enemy range: fPerceptionCompassBase + fPerceptionCompassMult x Perception, in game units (1576 + 256
    // per point, so about 26 m at Perception 1 and 59 m at 10). Survival sets both to 0 (no enemy dots); when the
    // player turns survival enemy markers on in MCM, the normal formula applies there too. Actors beyond the
    // high-process radius are in the middle-high list, so both lists are read; an actor is never in both.
    double base = 1576.0, mult = 256.0;
    if (auto* gmst = RE::GameSettingCollection::GetSingleton()) {
        auto* b = gmst->GetSetting("fPerceptionCompassBase");
        auto* m = gmst->GetSetting("fPerceptionCompassMult");
        if (b && b->GetType() == RE::Setting::SETTING_TYPE::kFloat && m && m->GetType() == RE::Setting::SETTING_TYPE::kFloat) {
            base = b->GetFloat();
            mult = m->GetFloat();
        }
    }
    auto* av = RE::ActorValue::GetSingleton();
    const double perception = av ? std::max(0.0F, Value(*player, av->perception)) : 1.0;
    const double zThreshold = HeightThreshold();
    const double kEnemyMeters = std::clamp((base + mult * perception) / 70.0, 10.0, 200.0);
    static double loggedRange = -1;
    if (std::abs(kEnemyMeters - loggedRange) >= 1.0) {
        spdlog::info("Enemy compass range {:.0f} m (base {:.0f} + {:.0f} x Perception {:.0f})", kEnemyMeters, base, mult, perception);
        loggedRange = kEnemyMeters;
    }
    std::vector<RE::BSPointerHandle<RE::Actor>> handles(lists->highActorHandles.begin(), lists->highActorHandles.end());
    handles.insert(handles.end(), lists->middleHighActorHandles.begin(), lists->middleHighActorHandles.end());
    for (const auto& handle : handles) {
        const auto actor = handle.get();
        if (!actor || actor.get() == player || actor->IsDisabled() || actor->IsDead(false) || !actor->IsInCombat()) continue;
        const double dx = actor->data.location.x - px, dy = actor->data.location.y - py;
        const double dist = std::sqrt(dx * dx + dy * dy) / 70.0;
        if (dist > kEnemyMeters) continue;
        // Like vanilla, only enemies fighting you: their combat target is you or one of your companions. Hostiles
        // fighting settlers, other factions or each other stay off the compass.
        const auto target = actor->currentCombatTarget.get();
        if (!target) continue;
        if (target.get() != player && !Mods::IsCompanion(*target)) continue;
        double b = std::atan2(dx, dy) * 180.0 / std::numbers::pi;
        if (b < 0) b += 360;
        char id[16];
        std::snprintf(id, sizeof(id), "e%08x", actor->GetFormID());
        Json marker{{"id", id}, {"t", "enemy"}, {"b", Round1(b, 1)}, {"dist", std::lround(dist)}};
#if defined(PHOSPHOR_AE)
        if (AE::Enabled("bMarkerHeights"))
#endif
        AddHeight(marker, actor->data.location.z - player->data.location.z, zThreshold);
        out.push_back(std::move(marker));
    }
    return out;
}

std::map<std::string, float> ReadIllnesses(float timescale) {
#if defined(PHOSPHOR_AE)
    AE_GATE("bSurvival", {});
#endif
    std::map<std::string, float> out;
    auto* player = RE::PlayerCharacter::GetSingleton();
    auto* list = player ? player->GetActiveEffectList() : nullptr;
    if (!list) return out;
    constexpr std::pair<std::string_view, const char*> kNames[] = {
        {"infect", "infection"}, {"parasit", "parasites"}, {"insomni", "insomnia"},
        {"letharg", "lethargy"}, {"weak", "weakness"}, {"fatigue", "fatigue"}};
    using Flag = RE::ActiveEffect::Flags;
    for (auto& entry : list->data) {
        auto* effect = entry.get();
        if (!effect || !effect->spell || effect->flags.any(Flag::kInactive, Flag::kDispelled, Flag::kWornOff)) continue;
        if (!effect->spell->As<RE::SpellItem>()) continue;  // illnesses are spells; consumables are on the tray
        const auto name = Name(effect->spell);
        for (const auto& [part, id] : kNames) {
            if (!Contains(name, part)) continue;
            // Effect timers run in real seconds; the HUD counts illnesses down in game hours.
            const float left = effect->duration > 0 ? std::max(0.0F, effect->duration - effect->elapsedSeconds) * timescale / 3600.0F : -1.0F;
            auto it = out.find(id);
            if (it == out.end() || left > it->second) out[id] = left;
            break;
        }
    }
    return out;
}

Json ReadParty(Json* allyMarkers) {
#if defined(PHOSPHOR_AE)
    AE_GATE("bCompanions", Json::array());
#endif
    Json out = Json::array();
    auto* player = RE::PlayerCharacter::GetSingleton();
    auto* lists = RE::ProcessLists::GetSingleton();
    auto* av = RE::ActorValue::GetSingleton();
#if defined(PHOSPHOR_AE)
    if (!player || !lists || !av) aeReadScope.Fail();
#endif
    if (!player || !lists || !av) return out;
    using Bool = RE::Actor::BOOL_FLAGS;
    for (const auto& handle : lists->highActorHandles) {
        const auto actor = handle.get();
        if (!actor || actor.get() == player || actor->IsDead(false)) continue;
        if (!Mods::IsCompanion(*actor)) continue;
        // The reference's display name carries renames (Liga lets you rename companions); fall back to the base NPC.
        const char* display = actor->GetDisplayFullName();
        const auto name = CleanName(display && *display ? ToUtf8(display) : Name(actor->GetObjectReference()), false);
        if (name.empty()) continue;
        const float max = std::max(1.0F, Permanent(*actor, av->health));
        const float hp = std::clamp(Value(*actor, av->health) / max, 0.0F, 1.0F);
        const auto life = static_cast<RE::LIFE_STATE>(actor->lifeState);
        const bool downed = life == RE::LIFE_STATE::kBleedout || life == RE::LIFE_STATE::kEssentialDown ||
                            life == RE::LIFE_STATE::kUnconscious;
        Json member{{"name", name}, {"hp", Round1(downed ? 0 : hp, 0.02)}, {"state", downed ? "downed" : "following"}};
        if (allyMarkers) {
            // The compass dot sits at the companion's real bearing, measured like enemies' (0 = north, clockwise).
            const double dx = actor->data.location.x - player->data.location.x;
            const double dy = actor->data.location.y - player->data.location.y;
            double b = std::atan2(dx, dy) * 180.0 / std::numbers::pi;
            if (b < 0) b += 360;
            Json marker{{"id", "ally:" + name}, {"t", "ally"}, {"b", Round1(b, 1)},
                        {"dist", std::lround(std::sqrt(dx * dx + dy * dy) / 70.0)}, {"label", name}};
#if defined(PHOSPHOR_AE)
            if (AE::Enabled("bMarkerHeights"))
#endif
            AddHeight(marker, actor->data.location.z - player->data.location.z, HeightThreshold());
            allyMarkers->push_back(std::move(marker));
        }
        if (Contains(name, "dogmeat")) member["type"] = "dog";
        if (const auto affinity = Mods::Affinity(*actor)) member["affinity"] = std::clamp(std::round(*affinity), -1000.0F, 1000.0F);
        Mods::LogFollowerState(*actor, name);
        out.push_back(std::move(member));
    }
    std::sort(out.begin(), out.end(), [](const Json& a, const Json& b) { return a["name"] < b["name"]; });
    return out;
}

namespace {
int NeedSlot(const char* need) { return need[0] == 'f' ? 0 : need[0] == 'w' ? 1 : 2; }
std::atomic<int> messageStages[3] = {-1, -1, -1};  // from the game's own stage messages; -1 = none
}

bool NoteSurvivalMessage(const std::string& text) {
    const auto stage = ParseSurvivalMessage(text);
    if (!stage) return false;
    messageStages[NeedSlot(stage->need)] = stage->stage;
    spdlog::info("Survival message '{}' -> {} stage {}", text, stage->need, stage->stage);
    return true;
}
void ClearSurvivalMessage(const char* need) { if (need) messageStages[NeedSlot(need)] = -1; }
std::array<int, 3> SurvivalMessageStages() { return {messageStages[0].load(), messageStages[1].load(), messageStages[2].load()}; }
void SetSurvivalMessageStages(const std::array<int, 3>& stages) {
    for (int i = 0; i < 3; ++i) messageStages[i] = stages[i] >= -1 && stages[i] <= 5 ? stages[i] : -1;
}

Json ReadSurvival() {
#if defined(PHOSPHOR_AE)
    AE_GATE("bSurvival", (Json{{"survival", Json{{"on", false}}}}));
#endif
    auto* player = RE::PlayerCharacter::GetSingleton();
#if defined(PHOSPHOR_AE)
    if (!player) aeReadScope.Fail();
#endif
    if (!player) return Json::object();
    const auto difficulty = static_cast<int>(player->GetDifficultyLevel());
    const bool on = difficulty >= static_cast<int>(RE::DIFFICULTY_LEVEL::kSurvival);
    Json out{{"survival", Json{{"on", on}}}};
    if (!on) return out;
    // Fatigue: the share of max AP lost to sleep deprivation (vanilla draws it red on the AP bar).
    if (auto* av = RE::ActorValue::GetSingleton(); av && av->fatigue) {
        const float apMax = std::max(1.0F, Permanent(*player, av->actionPoints));
        // Preserve the fraction: rounding it to 1% can move the recoverable AP cap by multiple points.
        out["survival"]["fatigue"] = std::clamp(Value(*player, av->fatigue) / apMax, 0.0F, 0.9F);
    }
    if (auto adrenaline = Mods::ReadAdrenaline(); !adrenaline.is_null()) out["survival"]["adrenaline"] = std::move(adrenaline);
    int stages[3] = {0, 0, 0};
    bool found[3] = {false, false, false};
    std::string seen;  // every non-consumable effect, for working out how this load order names its stages
    if (auto* list = player->GetActiveEffectList()) {
        using Flag = RE::ActiveEffect::Flags;
        for (auto& entry : list->data) {
            auto* effect = entry.get();
            if (!effect || !effect->spell || effect->flags.any(Flag::kInactive, Flag::kDispelled, Flag::kWornOff)) continue;
            if (effect->spell->As<RE::AlchemyItem>()) continue;
            // The stage can be named on the spell or on its magic effect.
            const auto spell = Name(effect->spell);
            auto* setting = effect->effect ? effect->effect->effectSetting : nullptr;
            const auto mgef = setting ? Name(setting) : std::string{};
            // Survival's need stages carry no name in some setups; their description still says which stage it is.
            std::string desc;
            if (setting && setting->magicItemDescription.c_str()) desc = ToUtf8(setting->magicItemDescription.c_str());
            if (desc.starts_with("<ID=")) desc.clear();
            if (seen.size() < 2500) {
                seen += spell;
                if (!mgef.empty() && mgef != spell) { seen += " / "; seen += mgef; }
                if (spell.empty() && mgef.empty()) {
                    char ids[96];
                    std::snprintf(ids, sizeof(ids), "[spell %08X mgef %08X av %s]", effect->spell->GetFormID(), setting ? setting->GetFormID() : 0,
                                  setting && setting->data.primaryAV ? setting->data.primaryAV->formEditorID.c_str() : "-");
                    seen += ids;
                    if (!desc.empty()) { seen += " '"; seen += desc.substr(0, 80); seen += "'"; }
                }
                seen += "; ";
            }
            const auto shortDesc = desc.substr(0, 40);  // "Parched: -1 Agility ..." style descriptions name the stage first
            for (const auto* name : {&spell, &mgef, &shortDesc}) {
                if (name->empty() || name->size() > 48) continue;
                for (const auto& s : kNeedStages) {
                    if (!Contains(*name, s.name)) continue;
                    const int slot = NeedSlot(s.need);
                    stages[slot] = std::max(stages[slot], s.stage);
                    found[slot] = true;
                    break;
                }
            }
        }
    }
    static bool avsLogged = false;
    if (!avsLogged) {
        avsLogged = true;
        // Survival's needs may live in actor values; list the candidates and the player's values once.
        if (auto* data = RE::TESDataHandler::GetSingleton()) {
            int shown = 0;
            for (auto* info : data->GetFormArray<RE::ActorValueInfo>()) {
                if (!info || shown >= 40) continue;
                const std::string edid = info->formEditorID.c_str() ? info->formEditorID.c_str() : "";
                if (!Contains(edid, "hunger") && !Contains(edid, "thirst") && !Contains(edid, "sleep") && !Contains(edid, "fatigue") &&
                    !Contains(edid, "hc_") && !Contains(edid, "food") && !Contains(edid, "water"))
                    continue;
                spdlog::info("Survival actor value {} ({:08X}) = {:.2f} / base {:.2f}", edid, info->GetFormID(), Value(*player, info),
                             Permanent(*player, info));
                ++shown;
            }
        }
    }
    static std::string seenLogged;
    if (seen != seenLogged) { spdlog::info("Survival effects on player: {}", seen.empty() ? "(none)" : seen); seenLogged = seen; }
    // Fall back on the game's own stage messages for needs no effect names.
    for (int i = 0; i < 3; ++i) if (const int m = messageStages[i]; !found[i] && m >= 0) stages[i] = m;
    out["survival"]["stages"] = Json{{"food", stages[0]}, {"water", stages[1]}, {"rest", stages[2]}};
    out["needs"] = Json{{"food", Round1(1.0 - stages[0] / 5.0, 0.01)}, {"water", Round1(1.0 - stages[1] / 5.0, 0.01)},
                        {"rest", Round1(1.0 - stages[2] / 5.0, 0.01)}};
    return out;
}

Json ReadGear() {
    auto* player = RE::PlayerCharacter::GetSingleton();
    auto* process = player ? player->currentProcess : nullptr;
    auto* middleHigh = process ? process->middleHigh : nullptr;
    std::uint32_t slots = 0;
    if (middleHigh) {
        RE::BSAutoLock lock{middleHigh->equippedItemsLock};
        for (auto& equipped : middleHigh->equippedItems) {
            auto* armor = equipped.item.object ? equipped.item.object->As<RE::TESObjectARMO>() : nullptr;
#if defined(PHOSPHOR_AE)
            if (armor) slots |= armor->bipedModelData.bipedObjectSlots;
#else
            if (armor) slots |= armor->RE::BGSBipedObjectForm::GetFilledSlots();
#endif
        }
    }
    const auto slot = [&](int biped) { return (slots & (1u << (biped - 30))) != 0; };
    const bool clothed = slot(33) || slot(36) || slot(37) || slot(38) || slot(39) || slot(40);
    return Json{{"clothed", clothed},
                {"armor", Json{{"head", slot(30) || slot(46)}, {"torso", slot(41)}, {"larm", slot(42)},
                               {"rarm", slot(43)}, {"lleg", slot(44)}, {"rleg", slot(45)}}}};
}

Json ReadWeight() {
    auto* player = RE::PlayerCharacter::GetSingleton();
    auto* av = RE::ActorValue::GetSingleton();
    if (!player || !av) return nullptr;
    return Json{{"cur", std::lround(player->GetWeightInContainer())}, {"max", std::lround(Value(*player, av->carryWeight))}};
}

namespace {
Json TargetJson(RE::Actor& actor, RE::ActorValue& av) {
    const char* display = actor.GetDisplayFullName();
    const auto name = CleanName(display && *display ? ToUtf8(display) : Name(actor.GetObjectReference()), false);
    const float max = std::max(1.0F, Permanent(actor, av.health));
    const float pct = actor.IsDead(false) ? 0.0F : std::clamp(Value(actor, av.health) / max, 0.0F, 1.0F);
    Json t{{"name", name}, {"level", static_cast<int>(actor.GetLevel())}, {"pct", Round1(pct, 0.01)}};
    if (Contains(name, "legendary")) t["legendary"] = true;
    if (auto mutations = Mods::Mutations(actor); !mutations.empty()) t["mutations"] = std::move(mutations);
    return t;
}
}

Json ReadTarget(std::uint32_t lastHitId, bool recentHit) {
#if defined(PHOSPHOR_AE)
    AE_GATE("bTarget", nullptr);
#endif
    auto* player = RE::PlayerCharacter::GetSingleton();
#if defined(PHOSPHOR_AE)
    if (!player) aeReadScope.Fail();
#endif
    auto* av = RE::ActorValue::GetSingleton();
    if (!player || !av) return nullptr;
    RE::NiPointer<RE::Actor> aimed;
    {
        RE::BSAutoLock lock{player->autoAimActor.dataLock};
        if (player->autoAimActor.optionalValue.has_value()) aimed = player->autoAimActor.optionalValue.value().get();
    }
    using Bool = RE::Actor::BOOL_FLAGS;
    if (aimed && aimed.get() != player && !aimed->IsDead(false) && !Mods::IsCompanion(*aimed)) {
        const double dx = aimed->data.location.x - player->data.location.x, dy = aimed->data.location.y - player->data.location.y;
        double facing = std::remainder(std::atan2(dx, dy) - player->data.angle.z, 2 * std::numbers::pi);
        const bool inFront = std::abs(facing) < 0.6 && std::sqrt(dx * dx + dy * dy) < 120 * 70;
        const bool relevant = aimed->GetHostileToActor(player) || aimed->IsInCombat() || (recentHit && aimed->GetFormID() == lastHitId);
        if (inFront && relevant) return TargetJson(*aimed, *av);
    }
    if (recentHit && lastHitId) {
        if (auto* hit = RE::TESForm::GetFormByID<RE::Actor>(lastHitId)) return TargetJson(*hit, *av);
    }
    return nullptr;
}

Json ReadStealth(int* raw) {
    auto* player = RE::PlayerCharacter::GetSingleton();
    auto* lists = RE::ProcessLists::GetSingleton();
    if (!player || !lists) return Json{{"sneaking", false}, {"mult", nullptr}};
    if (!player->IsSneaking()) return Json{{"sneaking", false}, {"mult", nullptr}};
    std::uint32_t los = 0;
    const int level = lists->RequestHighestDetectionLevelAgainstActor(player, los);
    if (raw) *raw = level;
    // The 0.9.16 perk query faulted on crouch in game. Do not run it from the HUD tick.
    // Explicit null clears any earlier preview/replay multiplier without changing detection.
    return Json{{"sneaking", true}, {"level", std::clamp(level, 0, 100)}, {"mult", nullptr}};
}

namespace {
using GValue = RE::Scaleform::GFx::Value;
// Available only inside CaptureNativeHud, while its UI task owns a strong HUDMenu reference.
thread_local RE::Scaleform::GFx::Movie* activeHudMovie = nullptr;
UiMailbox<NativeHudRequest, NativeHudSnapshot> nativeHud;
std::atomic<bool> nativeHudLoading{true};
RE::Scaleform::GFx::Movie* HudMovie() {
    return activeHudMovie;
}
bool Var(RE::Scaleform::GFx::Movie* movie, const char* path, GValue& out) {
#if defined(PHOSPHOR_AE)
    const bool ok = movie && movie->GetVariable(&out, path) && out.IsObject();
    AE::Observe(path, ok);
    if (ok) AE::SelfCheckOnce(path);
    return ok;
#else
    return movie && movie->GetVariable(&out, path) && out.IsObject();
#endif
}
std::string Member(const GValue& v, const char* name) {
    GValue m;
    if (v.IsObject() && v.GetMember(name, &m) && m.IsString() && m.GetString()) return ToUtf8(m.GetString());
    return {};
}
double Number(const GValue& v, const char* name, double fallback) {
    GValue m;
    if (!v.IsObject() || !v.GetMember(name, &m)) return fallback;
    if (m.IsNumber()) return m.GetNumber();
    if (m.IsInt()) return m.GetInt();
    if (m.IsUInt()) return m.GetUInt();
    return fallback;
}
bool Shown(const GValue& clip) {
    GValue vis;
    const bool visible = !(clip.GetMember("visible", &vis) && vis.IsBoolean()) || vis.GetBoolean();
    return visible && Number(clip, "alpha", 1.0) > 0.05;
}
std::string Trim(std::string s) {
    for (auto& c : s) if (c == '\r' || c == '\n' || c == '\t') c = ' ';
    const auto a = s.find_first_not_of(' ');
    if (a == std::string::npos) return {};
    return s.substr(a, s.find_last_not_of(' ') - a + 1);
}
// Every visible text under a display object, depth first.
void Texts(GValue obj, std::vector<std::string>& out, int depth) {
    if (!obj.IsObject() || depth > 6 || !Shown(obj)) return;
    if (auto text = Trim(Member(obj, "text")); !text.empty()) out.push_back(text);
    const int n = static_cast<int>(Number(obj, "numChildren", 0));
    for (int i = 0; i < n && i < 32; ++i) {
        GValue child;
        if (obj.Invoke("getChildAt", &child, std::array<GValue, 1>{GValue(i)})) Texts(child, out, depth + 1);
    }
}
constexpr const char* kSubtitles = "root.BottomCenterGroup_mc.SubtitleText_mc";
constexpr const char* kTutorial = "root.HUDNotificationsGroup_mc.TutorialText_mc";
constexpr const char* kMessages = "root.HUDNotificationsGroup_mc.Messages_mc";
constexpr const char* kRollover = "root.CenterGroup_mc.RolloverWidget_mc";
}

namespace {
// The vanilla HUD movie hidden as a whole (console "tm", cinematic fades): its root clip invisible or transparent.
bool HudHidden() {
#if defined(PHOSPHOR_AE)
    AE_GATE("bNativeHudText", false);
#endif
    auto* movie = HudMovie();
    GValue root;
    if (!movie || !movie->GetVariable(&root, "root") || !root.IsObject()) return false;
    const bool hidden = !Shown(root);
    static bool last = false;
    if (hidden != last) { spdlog::info("Vanilla HUD {}", hidden ? "hidden" : "shown"); last = hidden; }
    return hidden;
}
}

namespace {
HudText ReadHudText(bool gamepad) {
#if defined(PHOSPHOR_AE)
    AE_GATE("bNativeHudText", {});
#endif
    HudText out;
    auto* movie = HudMovie();
#if defined(PHOSPHOR_AE)
    if (!movie) aeReadScope.Fail();
#endif
    if (!movie) return out;
    out.ok = true;
    GValue v, f;
    if (Var(movie, kSubtitles, v) && Shown(v)) {
        if (v.GetMember("SubtitleText_tf", &f)) out.line = Trim(Member(f, "text"));
        if (v.GetMember("SpeakerName_tf", &f)) out.who = Trim(Member(f, "text"));
        out.subtitleShown = !out.line.empty();
    }
    if (Var(movie, kTutorial, v) && Shown(v)) {
        if (v.GetMember("TutorialText_tf", &f)) out.tutorial = Trim(Member(f, "text"));
        out.tutorialShown = !out.tutorial.empty();
    }
    if (Var(movie, kMessages, v)) {
        const int n = static_cast<int>(Number(v, "numChildren", 0));
        for (int i = 0; i < n && i < 16; ++i) {
            GValue item;
            if (!v.Invoke("getChildAt", &item, std::array<GValue, 1>{GValue(i)}) || !Shown(item)) continue;
            std::vector<std::string> parts;
            Texts(item, parts, 0);
            std::string text;
            for (const auto& p : parts) { if (!text.empty()) text += ' '; text += p; }
            if (!text.empty()) out.messages.push_back(text);
        }
    }
    if (Var(movie, kRollover, v) && Shown(v)) {
        if (v.GetMember("RolloverName_tf", &f)) {
            out.rolloverName = Trim(Member(f, "text"));
            const auto color = static_cast<std::uint32_t>(Number(f, "textColor", 0xFFFFFF));
            out.crime = ((color >> 16) & 0xFF) > 0xB0 && ((color >> 8) & 0xFF) < 0x60 && (color & 0xFF) < 0x60;
        }
        // Button hints. BSButtonHint only exposes getters (ButtonText, PCKey, ControllerButton, ButtonVisible);
        // its ButtonHintData is write-only and the bar's data vector is private. So walk the bar's display list for
        // visible hint clips, then fall back on the widget's own public ActivateButton / PlayHolotapeButton data.
        const auto add = [&](const GValue& h, const char* padMember) {
            GValue vis;
            // BSButtonHintBar can clear a pooled hint's data before removing its clip. ButtonVisible
            // checks for null data; ButtonText and PCKey do not. Fail closed before reading either.
            if (!h.IsObject() || !h.GetMember("ButtonVisible", &vis) || !vis.IsBoolean() || !vis.GetBoolean()) return;
            // The hint's own text field holds the translated label ("$AlternativeActivationPrompts_Consume" ->
            // "Consume"); fall back on the raw ButtonText when there is no field or it is still a key.
            std::string label;
            GValue field;
            if (h.GetMember("textField_tf", &field) && field.IsObject()) label = Trim(Member(field, "text"));
            if (label.empty() || label[0] == '$') label = Untranslate(Trim(Member(h, "ButtonText")));
            if (label.empty() || out.keys.size() >= 8) return;
            for (const auto& k : out.keys) if (k.second == label) return;
            std::string key = gamepad ? Member(h, padMember) : Member(h, "PCKey");
            if (gamepad && key.starts_with("Xenon_")) key = "gp:" + key.substr(6);
            while (!key.empty() && (key.back() == ')' || key.back() == ' ')) key.pop_back();  // PCKey reads "F)"
            out.keys.emplace_back(key, label);
        };
        GValue bar;
        if (v.GetMember("ButtonHintBar_mc", &bar) && bar.IsObject() && Shown(bar)) {
            std::function<void(GValue, int)> collect = [&](GValue clip, int depth) {
                const int n = static_cast<int>(Number(clip, "numChildren", 0));
                for (int i = 0; i < n && i < 16; ++i) {
                    GValue hint, text;
                    if (!clip.Invoke("getChildAt", &hint, std::array<GValue, 1>{GValue(i)}) || !hint.IsObject() || !Shown(hint)) continue;
                    // Identify hint clips by their public text field, never by invoking ButtonText.
                    if (hint.GetMember("textField_tf", &text) && text.IsObject()) add(hint, "ControllerButton");
                    else if (depth < 3) collect(hint, depth + 1);
                }
            };
            collect(bar, 0);
        }
        if (out.keys.empty() && !out.rolloverName.empty()) {
            for (const char* name : {"ActivateButton", "PlayHolotapeButton"}) {
                GValue data;
                if (v.GetMember(name, &data) && data.IsObject()) add(data, "XenonButton");
            }
        }
        {
            static int logged = 0;
            static std::string lastName;
            if (logged < 12 && !out.rolloverName.empty() && out.rolloverName != lastName) {
                std::string list;
                for (const auto& [k, l] : out.keys) list += (list.empty() ? "" : ", ") + (k.empty() ? std::string("?") : k) + ") " + l;
                spdlog::info("Rollover '{}': {} button hints [{}]", out.rolloverName, out.keys.size(), list);
                lastName = out.rolloverName;
                ++logged;
            }
        }
        out.rolloverShown = !out.rolloverName.empty() || !out.keys.empty();
    }
    return out;
}

void ShrinkVanillaText(bool hide, bool keepSubtitles) {
#if defined(PHOSPHOR_AE)
    if (!AE::Enabled("bHideVanilla") || !AE::Enabled("bNativeHudText")) return;
#endif
    auto* movie = HudMovie();
    if (!movie) return;
    for (const char* path : {kSubtitles, kTutorial, kMessages, kRollover}) {
        GValue v;
        if (!Var(movie, path, v)) continue;
        const double want = hide && !(keepSubtitles && path == kSubtitles) ? 0.001 : 1.0;
        if (std::abs(Number(v, "scaleX", 1.0) - want) > 0.0005) {
            v.SetMember("scaleX", GValue(want));
            v.SetMember("scaleY", GValue(want));
        }
    }
}

namespace {
constexpr const char* kCrosshair = "root.CenterGroup_mc.HUDCrosshair_mc";
constexpr const char* kCrosshairBase = "root.CenterGroup_mc.HUDCrosshair_mc.CrosshairBase_mc";
constexpr const char* kCrosshairTicks = "root.CenterGroup_mc.HUDCrosshair_mc.CrosshairBase_mc.CrosshairTicks_mc";
constexpr const char* kCrosshairClips = "root.CenterGroup_mc.HUDCrosshair_mc.CrosshairBase_mc.CrosshairClips_mc";
}

Json ReadCrosshair() {
#if defined(PHOSPHOR_AE)
    AE_GATE("bCrosshair", nullptr);
#endif
    // CrosshairBase keeps its radius private (_currentRadius; requestedRadius is write-only), but it places the four
    // CrosshairTicks_mc clips (Left/Right/Up/Down) at that radius, and those are public timeline instances.
    auto* movie = HudMovie();
    GValue ticks, left, right, up, down;
    if (!Var(movie, kCrosshairTicks, ticks)) return nullptr;
    if (!ticks.GetMember("Left", &left) || !ticks.GetMember("Right", &right) || !left.IsObject() || !right.IsObject()) return nullptr;
    const double lx = Number(left, "x", 0), rx = Number(right, "x", 0);
    double uy = 0, dy = 0;
    if (ticks.GetMember("Up", &up) && up.IsObject()) uy = Number(up, "y", 0);
    if (ticks.GetMember("Down", &down) && down.IsObject()) dy = Number(down, "y", 0);
    const double radius = std::max(0.0, (rx - lx) / 2);
    // HUDMenu.swf is 1280x720; the HUD stage is 1920x1080. Include the parents' scale on top.
    double scale = 1920.0 / 1280.0;
    for (const char* path : {"root.CenterGroup_mc", kCrosshair, kCrosshairBase, kCrosshairTicks}) {
        GValue clip;
        if (Var(movie, path, clip)) scale *= Number(clip, "scaleX", 1.0);
    }
    static double logged = -1;
    static int lines = 0;
    if (lines < 40 && (logged < 0 || std::abs(radius - logged) > 4)) {
        spdlog::info("Vanilla crosshair ticks L {:.1f} R {:.1f} U {:.1f} D {:.1f}, shown {} -> radius {:.1f} (scale {:.3f}) spread {:.1f} px",
                     lx, rx, uy, dy, Shown(ticks), radius, scale, radius * scale);
        logged = radius;
        ++lines;
    }
    // When vanilla shows its reticle (logged 2026-10-19): with a gun drawn CrosshairBase_mc shows CrosshairTicks_mc;
    // with no weapon, a holstered weapon or melee it swaps to CrosshairClips_mc, where only Dot_Dot (looking at
    // something usable) and the *_Activate clips come and go; menus and the Pip-Boy set HUDCrosshair_mc.visible false.
    // HUDCrosshair_mc's alpha is Phosphor's own hiding, so only its visible flag counts.
    bool crossVisible = true;
    if (GValue cross, vis; Var(movie, kCrosshair, cross) && cross.GetMember("visible", &vis) && vis.IsBoolean())
        crossVisible = vis.GetBoolean();
    const bool reticle = crossVisible && Shown(ticks);
    bool dot = reticle;
    if (GValue clips; !dot && crossVisible && Var(movie, kCrosshairClips, clips) && Shown(clips)) {
        for (const char* name : {"Dot_Dot", "Standard_Dot"}) {
            GValue clip;
            if (clips.GetMember(name, &clip) && clip.IsObject() && Shown(clip)) { dot = true; break; }
        }
    }
    return Json{{"spread", std::round(radius * scale * 2) / 2}, {"reticle", reticle}, {"dot", dot}};
}

namespace {
// Vanilla's flashlight icon (HUDMenu RightMeters_mc.FlashLightWidget_mc): the game shows it while the Pip-Boy light
// (or a mod light on that toggle) is on, so its visibility is the state. Shrunk rather than hidden, so it keeps updating.
constexpr const char* kFlashlight = "root.RightMeters_mc.FlashLightWidget_mc";
}

bool ReadFlashlight() {
#if defined(PHOSPHOR_AE)
    AE_GATE("bFlashlight", false);
#endif
    auto* movie = HudMovie();
    GValue v, vis;
    if (!Var(movie, kFlashlight, v)) return false;
    return v.GetMember("visible", &vis) && vis.IsBoolean() && vis.GetBoolean() && Number(v, "alpha", 1.0) > 0.05;
}

// Vanilla compass. AAF draws its wizard in a Sprite named "AAF_Surface" that it adds to CompassBar_mc (or to the
// crosshair until its script sees a visible compass), so suppressing PrismaUI's HUDCompass widget or shrinking the
// clip hid AAF's menu too (found 2026-10-18). The clip stays visible at full size; only the compass's own children
// fade out, and any layer named "AAF_..." is left alone.
constexpr const char* kCompass = "root.BottomCenterGroup_mc.CompassWidget_mc";
bool KeepInCompass(const std::string& name) { return name.rfind("AAF_", 0) == 0; }
void FadeCompassChildren(GValue& parent, bool hide, std::string& layout, int depth) {
    const int n = static_cast<int>(Number(parent, "numChildren", 0));
    for (int i = 0; i < n && i < 64; ++i) {
        GValue child;
        if (!parent.Invoke("getChildAt", &child, std::array<GValue, 1>{GValue(i)}) || !child.IsObject()) continue;
        const auto name = Member(child, "name");
        if (depth == 0 && name == "CompassBar_mc") {
            layout += "CompassBar_mc[ ";
            FadeCompassChildren(child, hide, layout, 1);
            layout += "] ";
            continue;
        }
        const bool keep = KeepInCompass(name);
        layout += (keep ? "+" : "-") + name + " ";
        if (keep) continue;
        const double want = hide ? 0.0 : 1.0;
        if (std::abs(Number(child, "alpha", 1.0) - want) > 0.01) child.SetMember("alpha", GValue(want));
    }
}
void ShrinkVanillaCompass(bool hide) {
#if defined(PHOSPHOR_AE)
    if (!AE::Enabled("bHideVanilla") || !AE::Enabled("bCompass")) return;
#endif
    static std::string lastLayout;
    static int layoutLogs = 0;
    static bool missingLogged = false;
    auto* movie = HudMovie();
    if (!movie) return;
    GValue v;
    if (!Var(movie, kCompass, v)) {
        if (!missingLogged) { missingLogged = true; spdlog::warn("Vanilla compass clip not found; it stays visible"); }
        return;
    }
    std::string layout;
    FadeCompassChildren(v, hide, layout, 0);
    if (layout != lastLayout && layoutLogs < 20) {
        lastLayout = layout;
        ++layoutLogs;
        spdlog::info("Vanilla compass children ({}; + kept, - faded): {}", hide ? "hiding" : "showing", layout);
    }
}

void ShrinkVanillaFlashlight(bool hide) {
#if defined(PHOSPHOR_AE)
    if (!AE::Enabled("bHideVanilla") || !AE::Enabled("bFlashlight")) return;
#endif
    auto* movie = HudMovie();
    GValue v;
    if (!Var(movie, kFlashlight, v)) return;
    const double want = hide ? 0.001 : 1.0;
    if (std::abs(Number(v, "scaleX", 1.0) - want) > 0.0005) {
        v.SetMember("scaleX", GValue(want));
        v.SetMember("scaleY", GValue(want));
    }
}

void HideVanillaCrosshair(bool hide) {
#if defined(PHOSPHOR_AE)
    if (!AE::Enabled("bHideVanilla") || !AE::Enabled("bCrosshair")) return;
#endif
    auto* movie = HudMovie();
    GValue v;
    if (!Var(movie, kCrosshair, v)) return;
    const double want = hide ? 0.0 : 1.0;
    if (std::abs(Number(v, "alpha", 1.0) - want) > 0.01) v.SetMember("alpha", GValue(want));
}

NativeHudSnapshot CaptureNativeHud(const NativeHudRequest& request) {
    // UI-thread-only ownership markers; identities are compared, never dereferenced or retained.
    static std::uintptr_t movieIdentity = 0;
    static NativeHudRequest appliedHiding;
    NativeHudSnapshot snapshot;
#if defined(PHOSPHOR_AE)
    // Compass hiding can run with every native HUD reader disabled.
    if (!request.capture && !request.hideCompass && !appliedHiding.hideText && !appliedHiding.hideCrosshair &&
        !appliedHiding.hideFlashlight && !appliedHiding.hideCompass) {
#else
    if (!request.capture && !appliedHiding.hideText && !appliedHiding.hideCrosshair && !appliedHiding.hideFlashlight &&
        !appliedHiding.hideCompass) {
#endif
        snapshot.visibilityApplied = true;
        return snapshot;
    }
    auto* ui = RE::UI::GetSingleton();
    if (!ui || nativeHudLoading.load()) return snapshot;
    for (const char* name : {"MainMenu", "LoadingMenu", "FaderMenu"})
        if (ui->GetMenuOpen(RE::BSFixedString(name))) return snapshot;
    // Hold the menu and movie alive until every local GFx value has been destroyed.
    auto menu = ui->GetMenu(RE::BSFixedString("HUDMenu"));
    if (!menu || !menu->uiMovie) {
        movieIdentity = 0;
        appliedHiding = {};
        snapshot.visibilityApplied = true; // No surviving movie needs restoration.
        return snapshot;
    }
    auto movie = menu->uiMovie;
    const auto identity = reinterpret_cast<std::uintptr_t>(movie.get());
    if (identity != movieIdentity) { movieIdentity = identity; appliedHiding = {}; }
    struct MovieScope {
        explicit MovieScope(RE::Scaleform::GFx::Movie* value) { activeHudMovie = value; }
        ~MovieScope() { activeHudMovie = nullptr; }
    } scope(movie.get());
    if (request.capture) {
        snapshot.text = ReadHudText(request.gamepad);
        snapshot.crosshair = ReadCrosshair();
        snapshot.flashlight = ReadFlashlight();
        snapshot.hidden = HudHidden();
    }
    if (request.hideText || appliedHiding.hideText) ShrinkVanillaText(request.hideText, request.keepSubtitles);
    if (request.hideCrosshair || appliedHiding.hideCrosshair) HideVanillaCrosshair(request.hideCrosshair);
    if (request.hideFlashlight || appliedHiding.hideFlashlight) ShrinkVanillaFlashlight(request.hideFlashlight);
    if (request.hideCompass || appliedHiding.hideCompass) ShrinkVanillaCompass(request.hideCompass);
    appliedHiding = request;
    snapshot.visibilityApplied = true;
    return snapshot;
}

void ScheduleNativeHud() {
    const auto* tasks = F4SE::GetTaskInterface();
    if (!tasks) { nativeHud.CancelQueue(); return; }
    try {
        tasks->AddUITask([] {
            const auto work = nativeHud.Begin();
            if (!work) return;
            std::optional<NativeHudSnapshot> result;
            try { result = CaptureNativeHud(work->request); }
            catch (const std::exception& e) { spdlog::error("Native HUD UI capture failed: {}", e.what()); }
            catch (...) { spdlog::error("Native HUD UI capture failed"); }
            if (nativeHud.Complete(*work, std::move(result))) ScheduleNativeHud();
        });
    } catch (...) { nativeHud.CancelQueue(); throw; }
}
} // UI-only Flash implementation

NativeHudSnapshot LatestNativeHud() { return nativeHud.Latest().value_or(NativeHudSnapshot{}); }
void RequestNativeHud(NativeHudRequest request) {
#if defined(PHOSPHOR_AE)
    request.hideText &= AE::Enabled("bHideVanilla") && AE::Enabled("bNativeHudText");
    request.hideCrosshair &= AE::Enabled("bHideVanilla") && AE::Enabled("bCrosshair");
    request.hideFlashlight &= AE::Enabled("bHideVanilla") && AE::Enabled("bFlashlight");
    request.hideCompass &= AE::Enabled("bHideVanilla") && AE::Enabled("bCompass");
    request.capture &= AE::Enabled("bNativeHudText") || AE::Enabled("bCrosshair") || AE::Enabled("bFlashlight");
#endif
    if (nativeHud.Submit(request)) ScheduleNativeHud();
}
void ResetNativeHud() { nativeHud.Reset(); }
void SetNativeHudLoading(bool loading) {
    nativeHudLoading.store(loading);
    nativeHud.Reset();
}

void LogXpSettings() {
    auto* settings = RE::GameSettingCollection::GetSingleton();
    if (!settings) return;
    for (const char* name : {"fXPLevelUpBase", "fXPLevelUpMult", "iXPLevelUpBase", "iXPLevelUpMult", "fXPLevelUp"}) {
        auto* setting = settings->GetSetting(name);
        if (!setting) continue;
        if (setting->GetType() == RE::Setting::SETTING_TYPE::kFloat) spdlog::info("Game setting {} = {}", name, setting->GetFloat());
        else if (setting->GetType() == RE::Setting::SETTING_TYPE::kInt) spdlog::info("Game setting {} = {}", name, setting->GetInt());
    }
}
#if defined(PHOSPHOR_AE)
void LogAeSelfCheck(const Json& snapshot, const Pipboy::Snapshot& pip) {
    spdlog::info("AE self-check: runtime {}; F4SE {}; PrismaUI V12 ABI 139 acquired", AE::runtime, AE::f4seVersion);
    for (const char* key : {"level", "hp", "hpMax", "ap", "apMax", "rads", "weapon", "clip", "clipMax", "reserve", "survival", "food", "water", "sleep"}) {
        if (snapshot.contains(key)) spdlog::info("AE self-check: {} = {}", key, snapshot[key].dump());
    }
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) { spdlog::warn("AE self-check: player unavailable"); return; }
    const auto pos = player->data.location;
    spdlog::info("AE self-check: position x={} y={} z={}; player angle z={} degrees", pos.x, pos.y, pos.z,
        player->data.angle.z * 180.0 / std::numbers::pi);
    auto* cell = player->GetParentCell();
    spdlog::info("AE self-check: cell='{}'; worldspace='{}'", Name(cell), Name(cell ? cell->worldSpace : nullptr));
    if (AE::Enabled("bCompass")) {
        auto* camera = RE::Main::WorldRootCamera();
        if (camera) {
            const auto& m = camera->worldToCam;
            spdlog::info("AE self-check: camera heading={} degrees; HUD heading={}",
                std::fmod(std::atan2(m[3][0], m[3][1]) * 180.0 / std::numbers::pi + 360.0, 360.0), Heading());
        } else spdlog::warn("AE self-check: camera unavailable");
    } else spdlog::info("AE self-check: camera/compass disabled");
    spdlog::info("AE self-check: Pip-Boy enabled={} connected={} quests={} markers={} locations={}",
        AE::Enabled("bPipboyFeed"), pip.connected, pip.quests.size(), pip.markers.size(), pip.locations.size());
    if (AE::Enabled("bCompass") && pip.connected && cell && cell->IsExterior()) {
        const Pipboy::Location* nearest = nullptr;
        double distance = 1e30;
        for (const auto& loc : pip.locations) {
            if (!loc.visible || loc.name.empty()) continue;
            const double d = std::hypot(loc.x - pos.x, loc.y - pos.y) / 70.0;
            if (d < distance) { distance = d; nearest = &loc; }
        }
        if (nearest) {
            auto z = MapMarkerZ(nearest->markerFormId); // Honors bMarkerHeights before resolving any form.
            spdlog::info("AE self-check: nearest map location='{}' distance={} m dz={} m", nearest->name, distance,
                z ? std::to_string((*z - pos.z) / 70.0) : "unavailable/disabled");
        } else spdlog::info("AE self-check: nearest map location unavailable");
    } else spdlog::info("AE self-check: nearest map location unavailable (feed/interior/disabled)");
    if (AE::Enabled("bEnemies") || AE::Enabled("bCompanions")) {
        auto* lists = RE::ProcessLists::GetSingleton();
        if (lists) spdlog::info("AE self-check: actors high={} middle-high={}", lists->highActorHandles.size(), lists->middleHighActorHandles.size());
        else spdlog::warn("AE self-check: actor lists unavailable");
    } else spdlog::info("AE self-check: actor lists disabled");
    spdlog::info("AE self-check: {}", AE::modDetection);
}
#endif
}
