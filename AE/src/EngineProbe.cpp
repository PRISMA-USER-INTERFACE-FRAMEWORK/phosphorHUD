#include "EngineProbe.h"
#include "Pipboy.h"

#if defined(PHOSPHOR_AE)
#include "../ae/AeRuntime.h"
#include "EngineLayout.h"
#endif
#include <F4SE/F4SE.h>
#include <RE/Fallout.h>
#include <Windows.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <format>
#include <numbers>
#include <string>
#include <vector>

namespace Phosphor::EngineProbe {
namespace {
// Mirrors of layouts that CommonLibF4-NG leaves opaque, taken from libxse/commonlibf4 (AE). Unverified on OG:
// checking them is the point of this probe. Every pointer is tested for readability before use.
#if defined(PHOSPHOR_AE)
using Phosphor::EngineLayout::TeleportLinkM;
using Phosphor::EngineLayout::SpaceNodeM;
using Phosphor::EngineLayout::TeleportPathM;
using Phosphor::EngineLayout::RefDataM;
using Phosphor::EngineLayout::QuestTargetM;
using Phosphor::EngineLayout::QuestTargetArrayM;
using Phosphor::EngineLayout::QuestObjectiveM;
#else
struct TeleportLinkM {
    RE::TESObjectREFR* door;
    RE::NiPoint3 doorTeleportLocation;
};
static_assert(sizeof(TeleportLinkM) == 0x18);
struct SpaceNodeM {
    bool worldspace;
    RE::TESWorldSpace* worldSpace;
    RE::TESObjectCELL* interiorCell;
};
static_assert(sizeof(SpaceNodeM) == 0x18);
struct TeleportPathM {
    RE::BSTArray<SpaceNodeM> spaces;     // 00
    RE::BSTArray<TeleportLinkM> doors;   // 18
    RE::NiPoint3 start;                  // 30
    RE::NiPoint3 end;                    // 3C
    bool worldMapOnly;                   // 48
};
static_assert(sizeof(TeleportPathM) == 0x50);
struct RefDataM {
    TeleportPathM teleportPath;          // 00
    RE::ObjectRefHandle reference;       // 50
    void* pathMessageQueue;              // 58
    bool forceRebuildQueued;             // 60
};
struct QuestTargetM {
    std::uint8_t flags;                  // 00
    void* conditions;                    // 08
    std::uint32_t targetAlias;           // 10
    RE::BGSKeyword* keyword;             // 18
    RE::BSTArray<RefDataM*> target;      // 20
};
static_assert(sizeof(QuestTargetM) == 0x38);
struct QuestTargetArrayM {
    RE::BSTArray<QuestTargetM*> targets; // 00
    std::uint64_t nextRepath;            // 18
};
static_assert(sizeof(QuestTargetArrayM) == 0x20);
struct QuestObjectiveM {
    void* displayText;                   // 00 BGSLocalizedString (pool entry)
    RE::TESQuest* ownerQuest;            // 08
    QuestTargetM** targets;              // 10
    std::uint32_t numTargets;            // 18
    std::uint16_t index;                 // 1C
    bool initialized;                    // 1E
    char state;                          // 1F
    std::uint32_t flags;                 // 20
};
static_assert(sizeof(QuestObjectiveM) == 0x28);

#endif

#if defined(PHOSPHOR_AE)
using Phosphor::EngineLayout::Readable;
#else
bool Readable(const void* p, std::size_t n) {
    if (!p) return false;
    __try {
        volatile char sink = 0;
        const volatile char* bytes = static_cast<const volatile char*>(p);
        for (std::size_t i = 0; i < n; i += 8) sink = bytes[i];
        sink = bytes[n - 1];
        (void)sink;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

#endif

std::string Hex(const void* p, std::size_t n) {
    std::string out;
    if (!Readable(p, n)) return "<unreadable>";
    char buf[4];
    for (std::size_t i = 0; i < n; ++i) {
        std::snprintf(buf, sizeof(buf), "%02X", static_cast<const unsigned char*>(p)[i]);
        out += buf;
        if (i % 4 == 3 && i + 1 < n) out += ' ';
    }
    return out;
}

// A pool string by entry pointer, as BSFixedString does it, with readability checks.
std::string PoolText(void* entry) {
#if defined(PHOSPHOR_AE)
    return Phosphor::EngineLayout::PoolText(entry).substr(0, 160);
#else
    if (!entry || !Readable(entry, 0x18)) return "<none>";
    const auto* e = static_cast<const RE::BSStringPool::Entry*>(entry);
    const auto* leaf = e->leaf();
    if (!leaf || !Readable(leaf, 0x18)) return "<bad leaf>";
    const char* text = leaf->data<char>();
    return text && Readable(text, 1) ? std::string(text).substr(0, 160) : "<bad>";
#endif
}

std::string FormName(const RE::TESForm* form) {
    if (!form) return "";
    return std::string(RE::TESFullName::GetFullName(*form));
}

std::string RefLine(RE::TESObjectREFR* ref, double px, double py) {
    if (!ref) return "null";
    const auto p = ref->data.location;
    const double dx = p.x - px, dy = p.y - py;
    double b = std::atan2(dx, dy) * 180.0 / std::numbers::pi;
    if (b < 0) b += 360;
    auto* cell = ref->GetParentCell();
    const char* name = ref->GetDisplayFullName();
    return std::format("{:08X} '{}' base {:08X} at ({:.0f},{:.0f},{:.0f}) {} b {:.0f} dist {:.0f} m",
        ref->GetFormID(), name ? name : "", ref->GetObjectReference() ? ref->GetObjectReference()->GetFormID() : 0,
        p.x, p.y, p.z, cell ? (cell->IsInterior() ? std::format("interior {:08X}", cell->GetFormID()) : std::string("exterior")) : "nocell",
        b, std::sqrt(dx * dx + dy * dy) / 70.0);
}

void LogMapMarker(const char* tag, RE::TESObjectREFR* ref, double px, double py) {
    if (!ref) return;
    RE::BSExtraData* extra = ref->extraList ? ref->extraList->GetByType(RE::EXTRA_DATA_TYPE::kMapMarker) : nullptr;
    void* data = nullptr;
#if defined(PHOSPHOR_AE)
    if (extra && Readable(extra, sizeof(RE::ExtraMapMarker))) data = static_cast<RE::ExtraMapMarker*>(extra)->mapMarkerData;
#else
    if (extra && Readable(extra, 0x20)) data = *reinterpret_cast<void**>(reinterpret_cast<char*>(extra) + 0x18);
#endif
    std::string name = "<no data>";
#if defined(PHOSPHOR_AE)
    if (data && Readable(data, sizeof(RE::MapMarkerData))) {
        name = Phosphor::EngineLayout::MarkerName(data);
        spdlog::info("Engine probe: marker type at 0x12 = {}, native header member (0x14) = {}",
            Phosphor::EngineLayout::MarkerType(data), Phosphor::EngineLayout::NativeMarkerType(data));
    }
#else
    if (data && Readable(data, 0x18)) name = PoolText(*reinterpret_cast<void**>(static_cast<char*>(data) + 0x08));
#endif
    spdlog::info("Engine probe: {} {} | marker '{}' bytes10 {}", tag, RefLine(ref, px, py), name,
        data ? Hex(static_cast<char*>(data) + 0x10, 8) : std::string("-"));
}
}  // namespace

void Run(const Pipboy::Snapshot& pip, const char* why) {
#if defined(PHOSPHOR_AE)
    if (!Phosphor::AE::Enabled("bCompass")) return;
    AE_GATE("bEngineQuests", );
#endif
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) return;
    const double px = player->data.location.x, py = player->data.location.y;
    auto* cell = player->GetParentCell();
    spdlog::info("Engine probe ({}): player ({:.0f},{:.0f},{:.0f}) {} heading {:.0f}", why, px, py, player->data.location.z,
        cell ? (cell->IsInterior() ? std::format("interior {:08X} '{}'", cell->GetFormID(), FormName(cell)) : std::string("exterior")) : "nocell",
        player->data.angle.z * 180.0 / std::numbers::pi);

    // 1. Displayed objectives.
    struct Obj { void* objective; std::uint32_t instance; std::int32_t state; };
    std::vector<Obj> objectives;
    for (const auto& o : player->objectives)
        objectives.push_back({o.objective, o.instanceID, static_cast<std::int32_t>(o.enstanceState.underlying())});
    spdlog::info("Engine probe: {} player objectives", objectives.size());
    int shown = 0;
    for (const auto& o : objectives) {
        if (shown++ >= 40) break;
        auto* m = static_cast<QuestObjectiveM*>(o.objective);
        if (!Readable(m, sizeof(QuestObjectiveM))) { spdlog::info("Engine probe:   objective {} unreadable", o.objective); continue; }
        auto* quest = m->ownerQuest;
        const bool qok = Readable(quest, 0x120);
        spdlog::info("Engine probe:   objective inst {} state {} | quest {:08X} '{}' flags {:04X} type {} | idx {} init {} st {} fl {:X} targets {} | '{}'",
            o.instance, o.state, qok ? quest->GetFormID() : 0, qok ? FormName(quest) : "?", qok ? quest->data.flags : 0,
            qok ? quest->data.questType : 0, m->index, m->initialized, static_cast<int>(m->state), m->flags, m->numTargets,
            PoolText(m->displayText));
    }

    // 2. Quest targets the player is guided to.
    std::vector<std::pair<RE::TESQuest*, void*>> targetMap;
    {
        RE::BSAutoLock lock{player->questTargetLock};
        for (auto& entry : player->questTargets) targetMap.emplace_back(entry.first, entry.second);
    }
    spdlog::info("Engine probe: {} quests with targets", targetMap.size());
    int lines = 0;
    for (const auto& [quest, raw] : targetMap) {
        auto* arr = static_cast<QuestTargetArrayM*>(raw);
        if (!Readable(quest, 0x120) || !Readable(arr, sizeof(QuestTargetArrayM))) { spdlog::info("Engine probe:   quest entry unreadable"); continue; }
        spdlog::info("Engine probe:   quest {:08X} '{}' flags {:04X}: {} targets (raw {})", quest->GetFormID(), FormName(quest),
            quest->data.flags, arr->targets.size(), Hex(arr, 0x20));
#if defined(PHOSPHOR_AE)
        if (!Phosphor::EngineLayout::ArrayReadable(arr->targets)) continue;
#endif
        for (auto* t : arr->targets) {
            if (++lines > 60) break;
            if (!Readable(t, sizeof(QuestTargetM))) { spdlog::info("Engine probe:     target unreadable"); continue; }
            spdlog::info("Engine probe:     target flags {:02X} alias {} refs {}", t->flags, t->targetAlias, t->target.size());
#if defined(PHOSPHOR_AE)
            if (!Phosphor::EngineLayout::ArrayReadable(t->target)) continue;
#endif
            for (auto* r : t->target) {
                if (!Readable(r, 0x60)) { spdlog::info("Engine probe:       refdata unreadable"); continue; }
#if defined(PHOSPHOR_AE)
                if (!Readable(r, sizeof(RefDataM)) || !Phosphor::EngineLayout::ArrayReadable(r->teleportPath.doors)) continue;
#endif
                auto ref = r->reference.get();
                spdlog::info("Engine probe:       ref {} | path doors {} spaces {} start ({:.0f},{:.0f}) end ({:.0f},{:.0f}) mapOnly {}",
                    RefLine(ref.get(), px, py), r->teleportPath.doors.size(), r->teleportPath.spaces.size(),
                    r->teleportPath.start.x, r->teleportPath.start.y, r->teleportPath.end.x, r->teleportPath.end.y, r->teleportPath.worldMapOnly);
                int d = 0;
                for (const auto& link : r->teleportPath.doors) {
                    if (d++ >= 4) break;
                    spdlog::info("Engine probe:         door {} -> ({:.0f},{:.0f},{:.0f})",
                        Readable(link.door, 0x110) ? RefLine(link.door, px, py) : std::string("unreadable"),
                        link.doorTeleportLocation.x, link.doorTeleportLocation.y, link.doorTeleportLocation.z);
                }
            }
        }
    }

    // 3. Custom map marker.
    if (auto marker = player->playerMapMarker.get()) spdlog::info("Engine probe: custom marker {}", RefLine(marker.get(), px, py));
    else spdlog::info("Engine probe: no custom marker");

    // 4. Map markers: the player's current list and the worldspace's persistent cell.
    std::vector<RE::ObjectRefHandle> current(player->currentMapMarkers.begin(), player->currentMapMarkers.end());
    spdlog::info("Engine probe: {} currentMapMarkers", current.size());
    int m = 0;
    for (auto& h : current) { if (m++ >= 20) break; if (auto r = h.get()) LogMapMarker("current", r.get(), px, py); }
    auto* world = cell && !cell->IsInterior() ? cell->worldSpace : nullptr;
    if (world && world->persistentCell) {
#if defined(PHOSPHOR_AE)
        std::vector<RE::NiPointer<RE::TESObjectREFR>> held;
        {
            RE::BSAutoLock guard{world->persistentCell->spinLock};
            held.assign(world->persistentCell->references.begin(), world->persistentCell->references.end());
        }
        const auto total = held.size();
        std::vector<RE::TESObjectREFR*> markers;
        for (const auto& ref : held)
            if (Readable(ref.get(), sizeof(RE::TESObjectREFR)) && Phosphor::EngineLayout::MapMarkerBytes(ref.get()))
                markers.push_back(ref.get());
#else
        std::vector<RE::TESObjectREFR*> markers;
        int total = 0;
        {
            RE::BSAutoLock guard{world->persistentCell->spinLock};
            for (auto& ref : world->persistentCell->references) {
                ++total;
                if (ref && ref->extraList && ref->extraList->HasType(RE::EXTRA_DATA_TYPE::kMapMarker)) markers.push_back(ref.get());
            }
        }
#endif
        std::sort(markers.begin(), markers.end(), [&](auto* a, auto* b) {
            const auto da = std::hypot(a->data.location.x - px, a->data.location.y - py);
            const auto db = std::hypot(b->data.location.x - px, b->data.location.y - py);
            return da < db;
        });
        spdlog::info("Engine probe: worldspace '{}' persistent refs {}, map markers {}", FormName(world), total, markers.size());
        for (std::size_t i = 0; i < markers.size() && i < 12; ++i) LogMapMarker("persistent", markers[i], px, py);
    }

    // 5. What the Pip-Boy feed says, for comparison.
    spdlog::info("Engine probe: Pip-Boy feed connected {} quests {} markers {} locations {}", pip.connected, pip.quests.size(),
        pip.markers.size(), pip.locations.size());
    for (const auto& q : pip.quests) {
        if (!q.active) continue;
        spdlog::info("Engine probe:   pip active quest {:08X} '{}' objectives {}", q.formId, q.text, q.objectives.size());
        for (const auto& o : q.objectives) spdlog::info("Engine probe:     pip objective '{}' completed {} active {}", o.text, o.completed, o.active);
    }
    for (const auto& mk : pip.markers) {
        const double dx = mk.x - px, dy = mk.y - py;
        double b = std::atan2(dx, dy) * 180.0 / std::numbers::pi;
        if (b < 0) b += 360;
        spdlog::info("Engine probe:   pip marker '{}' ({:.0f},{:.0f}) door {} quests {} b {:.0f} dist {:.0f} m", mk.name, mk.x, mk.y, mk.onDoor,
            mk.quests.size(), b, std::sqrt(dx * dx + dy * dy) / 70.0);
    }
    std::vector<const Pipboy::Location*> nearby;
    for (const auto& l : pip.locations) nearby.push_back(&l);
    std::sort(nearby.begin(), nearby.end(), [&](auto* a, auto* b) { return std::hypot(a->x - px, a->y - py) < std::hypot(b->x - px, b->y - py); });
    for (std::size_t i = 0; i < nearby.size() && i < 12; ++i)
        spdlog::info("Engine probe:   pip location '{}' type {} disc {} vis {} workshop {} marker {:08X} ({:.0f},{:.0f}) dist {:.0f} m",
            nearby[i]->name, nearby[i]->type, nearby[i]->discovered, nearby[i]->visible, nearby[i]->workshop, nearby[i]->markerFormId,
            nearby[i]->x, nearby[i]->y, std::hypot(nearby[i]->x - px, nearby[i]->y - py) / 70.0);
    spdlog::info("Engine probe: done");
}
}  // namespace Phosphor::EngineProbe
