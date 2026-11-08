#include "EngineQuests.h"

#if defined(PHOSPHOR_AE)
#include "../ae/AeRuntime.h"
#endif
#include "EngineLayout.h"
#include "Live.h"

#include <F4SE/F4SE.h>
#include <RE/Fallout.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstring>
#include <format>
#include <map>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Phosphor::EngineQuests {
namespace {
using namespace Phosphor::EngineLayout;
using Clock = std::chrono::steady_clock;

struct CachedMarker {
    std::uint32_t formId = 0;
    double x = 0, y = 0;
    std::string name;
    int type = -1;
    bool workshop = false;
};

std::uint32_t cachedSpace = UINT32_MAX;
std::vector<CachedMarker> markerCache;
std::map<std::pair<const void*, std::uint32_t>, std::string> textCache;
Clock::time_point textCacheAt{};
int parseCalls = 0;

int readNo = 0;
void Step(const char* what) {
    if (readNo <= 2) spdlog::info("Engine quests: read {} {}", readNo, what);
}

std::string Lower(std::string text) {
    for (auto& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return text;
}

// Display name of one quest alias by name: a reference alias's current reference, or a location alias's location.
// Read from the quest's alias list and maps; no engine text function is called (BGSQuestInstanceText::ParseString
// corrupted game memory when called with a hand-built string on OG).
std::string AliasName(RE::TESQuest* quest, const std::string& wanted) {
    const std::string key = Lower(wanted);
#if defined(PHOSPHOR_AE)
    std::vector<RE::BGSBaseAlias*> aliases;
    {
        RE::BSAutoReadLock lock{quest->aliasAccessLock};
        aliases.assign(quest->aliases.begin(), quest->aliases.end());
    }
    for (auto* alias : aliases) {
#else
    for (auto* alias : quest->aliases) {
#endif
        if (!Readable(alias, sizeof(RE::BGSBaseAlias))) continue;
        const char* name = alias->aliasName.c_str();
        if (!name || Lower(name) != key) continue;
        const auto id = alias->aliasID;
#if defined(PHOSPHOR_AE)
        RE::BGSLocation* location = nullptr;
        {
            RE::BSAutoReadLock lock{quest->aliasAccessLock};
            if (const auto it = quest->aliasedLocMap.find(id); it != quest->aliasedLocMap.end()) location = it->second;
        }
        if (Readable(location, sizeof(RE::BGSLocation))) return std::string(location->fullName.c_str());
#else
        {
            RE::BSAutoReadLock lock{quest->aliasAccessLock};
            if (const auto it = quest->aliasedLocMap.find(id); it != quest->aliasedLocMap.end() && it->second)
                return std::string(RE::TESFullName::GetFullName(*it->second));
        }
#endif
#if defined(PHOSPHOR_AE)
        const auto handle = quest->GetAliasedRef(id);
#else
        RE::BSPointerHandle<RE::TESObjectREFR> handle;
        quest->GetAliasedRef(&handle, id);
#endif
        if (auto ref = handle.get()) {
            const char* display = ref->GetDisplayFullName();
            if (display && *display) return display;
            if (auto* base = ref->GetObjectReference()) return std::string(RE::TESFullName::GetFullName(*base));
        }
        return {};
    }
    return {};
}

// Fills <Alias=Name>, <Alias.Attribute=Name> and <Global=EditorID>; other tokens are left as they are.
std::string Resolve(const std::string& raw, RE::TESQuest* quest) {
    if (raw.find('<') == std::string::npos || !quest) return raw;
    std::string out;
    std::size_t pos = 0;
    while (pos < raw.size()) {
        const auto open = raw.find('<', pos);
        if (open == std::string::npos) { out += raw.substr(pos); break; }
        const auto close = raw.find('>', open);
        if (close == std::string::npos) { out += raw.substr(pos); break; }
        out += raw.substr(pos, open - pos);
        const std::string token = raw.substr(open + 1, close - open - 1);
        const auto eq = token.find('=');
        const std::string kind = Lower(token.substr(0, std::min(eq, token.find('.'))));
        std::string value;
        bool handled = false;
        if (eq != std::string::npos && kind == "alias") {
            value = AliasName(quest, token.substr(eq + 1));
            handled = true;
        } else if (eq != std::string::npos && kind == "global") {
            const std::string id = token.substr(eq + 1);
            if (auto* global = RE::TESForm::GetFormByEditorID<RE::TESGlobal>(RE::BSFixedString(id.c_str()))) {
                value = std::to_string(static_cast<long long>(std::lround(global->GetValue())));
                handled = true;
            }
        }
        out += handled ? value : raw.substr(open, close - open + 1);
        pos = close + 1;
    }
    if (parseCalls < 8) {
        ++parseCalls;
        spdlog::info("Engine quests: text '{}' -> '{}'", raw, out);
    }
    return out;
}

std::string CachedText(const void* key, std::uint32_t instance, const std::string& raw, RE::TESQuest* quest) {
    auto& slot = textCache[{key, instance}];
    if (slot.empty()) slot = Phosphor::Live::ToUtf8(Resolve(raw, quest));
    return slot;
}

std::string QuestName(RE::TESQuest* quest, std::uint32_t instance) {
    return CachedText(quest, instance, std::string(RE::TESFullName::GetFullName(*quest)), quest);
}

// Names of settlement locations (keyword LocTypeWorkshopSettlement), read once from the location forms. Only data is
// read: no virtual calls on references (calling TESObjectREFR::GetEditorLocation here crashed OG, 2026-10-20).
const std::vector<std::string>& SettlementNames() {
    static std::vector<std::string> names;
    static bool built = false;
    if (built) return names;
    built = true;
    auto* data = RE::TESDataHandler::GetSingleton();
    if (!data) return names;
    for (auto* location : data->GetFormArray<RE::BGSLocation>()) {
        if (!location) continue;
        bool settlement = false;
        static_cast<const RE::BGSKeywordForm*>(location)->ForEachKeyword([&](RE::BGSKeyword* keyword) {
            const std::string_view id = keyword ? keyword->GetFormEditorID() : "";
            if (id == "LocTypeWorkshopSettlement") { settlement = true; return RE::BSContainer::ForEachResult::kStop; }
            return RE::BSContainer::ForEachResult::kContinue;
        });
        if (!settlement) continue;
        auto name = Lower(Phosphor::Live::ToUtf8(std::string(RE::TESFullName::GetFullName(*location))));
        if (!name.empty()) names.push_back(std::move(name));
    }
    spdlog::info("Engine quests: {} settlement locations", names.size());
    return names;
}

bool IsSettlementName(const std::string& markerName) {
    const auto key = Lower(markerName);
    const auto& names = SettlementNames();
    return std::find(names.begin(), names.end(), key) != names.end();
}

void ScanMarkers(RE::TESWorldSpace* world) {
    markerCache.clear();
    cachedSpace = world ? world->GetFormID() : 0;
    if (!world || !world->persistentCell) return;
#if defined(PHOSPHOR_AE)
    std::vector<RE::NiPointer<RE::TESObjectREFR>> refs;
    {
        RE::BSAutoLock guard{world->persistentCell->spinLock};
        refs.assign(world->persistentCell->references.begin(), world->persistentCell->references.end());
    }
    for (const auto& held : refs) {
        auto* ref = held.get();
        if (!Readable(ref, sizeof(RE::TESObjectREFR))) continue;
#else
    std::vector<RE::TESObjectREFR*> refs;
    {
        RE::BSAutoLock guard{world->persistentCell->spinLock};
        for (auto& ref : world->persistentCell->references)
            if (ref && ref->extraList && ref->extraList->HasType(RE::EXTRA_DATA_TYPE::kMapMarker)) refs.push_back(ref.get());
    }
    for (auto* ref : refs) {
#endif
        const auto* bytes = MapMarkerBytes(ref);
        if (!bytes) continue;
        CachedMarker c;
        c.formId = ref->GetFormID();
        c.x = ref->data.location.x;
        c.y = ref->data.location.y;
#if defined(PHOSPHOR_AE)
        c.name = Phosphor::Live::ToUtf8(MarkerName(bytes));
#else
        c.name = Phosphor::Live::ToUtf8(PoolText(*reinterpret_cast<void* const*>(bytes + kMarkerNameEntry)));
#endif
#if defined(PHOSPHOR_AE)
        c.type = MarkerType(bytes);
#else
        c.type = bytes[kMarkerType];
#endif
        c.workshop = IsSettlementName(c.name);
        if (!c.name.empty()) markerCache.push_back(std::move(c));
    }
    spdlog::info("Engine quests: {} named map markers in worldspace {:08X}", markerCache.size(), cachedSpace);
}

std::uint32_t MarkerId(std::uint32_t quest, std::uint32_t alias, std::uint32_t ref) {
    std::uint32_t h = 2166136261u;
    for (std::uint32_t v : {quest, alias, ref}) {
        for (int i = 0; i < 4; ++i) {
            h ^= (v >> (i * 8)) & 0xFF;
            h *= 16777619u;
        }
    }
    return h | 0x80000000u;  // never collides with the feed's small node ids
}
}  // namespace

void Reset() {
    markerCache.clear();
    cachedSpace = UINT32_MAX;
    textCache.clear();
}

Pipboy::Snapshot Read() {
#if defined(PHOSPHOR_AE)
    if (!Phosphor::AE::Enabled("bCompass")) return {};
    AE_GATE("bEngineQuests", {});
#endif
    ++readNo;
    Step("start");
    Pipboy::Snapshot out;
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) return out;
    out.connected = true;
    auto* cell = player->GetParentCell();
    const bool interior = cell && cell->IsInterior();
    const RE::NiPoint3 outside = interior ? player->exteriorPosition : player->data.location;
    out.hasPlayer = true;
    out.playerX = outside.x;
    out.playerY = outside.y;
    const auto now = Clock::now();
    if (now - textCacheAt > std::chrono::seconds(3)) {
        textCache.clear();  // alias fills change as quests advance
        textCacheAt = now;
    }

    // Quests and objectives the Pip-Boy lists: displayed (1), completed and displayed (3), failed and displayed (5).
    struct Shown { QuestObjectiveM* objective; std::uint32_t instance; int state; std::size_t order; };
    std::vector<Shown> shown;
    std::size_t order = 0;
    for (const auto& o : player->objectives) {
        const int state = static_cast<int>(o.enstanceState.underlying());
        if (state == 1 || state == 3 || state == 5)
            shown.push_back({reinterpret_cast<QuestObjectiveM*>(o.objective), o.instanceID, state, order});
        ++order;
    }
    struct Group { Pipboy::Quest quest; std::size_t newest = 0; std::vector<std::pair<std::uint16_t, Pipboy::Objective>> objectives; };
    std::map<std::uint32_t, Group> groups;
    std::unordered_map<const void*, std::string> openTargets;  // target -> text of the open objective it belongs to
    for (const auto& s : shown) {
        auto* m = s.objective;
        if (!Readable(m, sizeof(QuestObjectiveM))) continue;
        auto* quest = m->ownerQuest;
        if (!Readable(quest, 0x120)) continue;
        const bool misc = quest->data.questType == kQuestTypeMisc;
        const std::uint32_t id = misc ? 0 : quest->GetFormID();
        auto& g = groups[id];
        if (g.quest.text.empty()) {
            g.quest.formId = id;
            g.quest.text = misc ? std::string("Miscellaneous") : QuestName(quest, s.instance);
        }
        if (quest->data.flags & kQuestTracked) g.quest.active = true;
        g.newest = std::max(g.newest, s.order);
        Pipboy::Objective objective;
        objective.text = CachedText(m, s.instance, PoolText(m->displayText), quest);
        if (objective.text.empty()) continue;
        objective.completed = s.state != 1;
        objective.active = !objective.completed;
        if (objective.active && m->numTargets && m->numTargets < 64 && Readable(m->targets, m->numTargets * sizeof(void*)))
            for (std::uint32_t i = 0; i < m->numTargets; ++i) openTargets[m->targets[i]] = objective.text;
        g.objectives.emplace_back(m->index, std::move(objective));
    }
    Step("objectives read");
    std::vector<Group*> ordered;
    for (auto& [id, g] : groups) {
        std::stable_sort(g.objectives.begin(), g.objectives.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
        for (auto& [index, objective] : g.objectives) g.quest.objectives.push_back(std::move(objective));
        ordered.push_back(&g);
    }
    std::sort(ordered.begin(), ordered.end(), [](const Group* a, const Group* b) { return a->newest > b->newest; });
    for (auto* g : ordered) out.quests.push_back(std::move(g->quest));

    Step("quests built");
    // Compass targets of tracked quests, pointed at the first door when the target is in another space.
    std::vector<std::pair<RE::TESQuest*, QuestTargetArrayM*>> targets;
    {
        RE::BSAutoLock lock{player->questTargetLock};
        for (auto& entry : player->questTargets)
            targets.emplace_back(entry.first, reinterpret_cast<QuestTargetArrayM*>(entry.second));
    }
    for (auto& [quest, list] : targets) {
        if (!Readable(quest, 0x120) || !(quest->data.flags & kQuestTracked) || !Readable(list, sizeof(QuestTargetArrayM))) continue;
        const bool misc = quest->data.questType == kQuestTypeMisc;
#if defined(PHOSPHOR_AE)
        if (!ArrayReadable(list->targets)) continue;
#endif
        bool matchedAny = false;
        for (auto* t : list->targets) matchedAny = matchedAny || openTargets.contains(t);
        const std::string fallback = QuestName(quest, quest->currentInstanceID);
        for (auto* t : list->targets) {
            if (!Readable(t, sizeof(QuestTargetM))) continue;
            const auto named = openTargets.find(t);
            if (matchedAny && named == openTargets.end()) continue;  // a target of an objective the player cannot see
#if defined(PHOSPHOR_AE)
            if (!ArrayReadable(t->target)) continue;
#endif
            std::uint32_t slot = 0;
            for (auto* r : t->target) {
                ++slot;
                if (!Readable(r, sizeof(RefDataM))) continue;
                const auto& path = r->teleportPath;
#if defined(PHOSPHOR_AE)
                if (!ArrayReadable(path.doors)) continue;
#endif
                const auto ref = r->reference.get();
                Pipboy::QuestMarker marker;
                if (!path.doors.empty()) {
                    auto* door = path.doors[0].door;
                    if (!Readable(door, 0x110)) continue;
                    marker.x = door->data.location.x;
                    marker.y = door->data.location.y;
#if defined(PHOSPHOR_AE)
                    if (Phosphor::AE::Enabled("bMarkerHeights")) {
#endif
                    marker.z = door->data.location.z;
                    marker.hasZ = true;
#if defined(PHOSPHOR_AE)
                    }
#endif
                    marker.onDoor = true;
                } else if (path.worldMapOnly) {
                    marker.x = path.end.x;
                    marker.y = path.end.y;
                } else if (ref) {
                    marker.x = ref->data.location.x;
                    marker.y = ref->data.location.y;
#if defined(PHOSPHOR_AE)
                    if (Phosphor::AE::Enabled("bMarkerHeights")) {
#endif
                    marker.z = ref->data.location.z;
                    marker.hasZ = true;
#if defined(PHOSPHOR_AE)
                    }
#endif
                } else {
                    continue;
                }
                marker.name = named != openTargets.end() ? named->second : fallback;
                marker.quests.push_back(quest->GetFormID());
                if (misc) marker.quests.push_back(0);
                marker.pipId = MarkerId(quest->GetFormID(), t->targetAlias, ref ? ref->GetFormID() : slot);
                out.markers.push_back(std::move(marker));
            }
        }
    }

    Step("targets read");
    // The player's custom map marker.
    if (auto pin = player->playerMapMarker.get()) {
        auto* pinCell = pin->GetParentCell();
        const CustomWaypoint waypoint{pin->data.location.x, pin->data.location.y};
        if (pinCell && pinCell->IsInterior()) out.customLocal = waypoint;
        else out.customWorld = waypoint;
    }

    // Map locations of the current (or last exterior) worldspace.
    RE::TESWorldSpace* world = !interior && cell ? cell->worldSpace : player->cachedWorldspace;
    const std::uint32_t space = world ? world->GetFormID() : 0;
    if (space != cachedSpace) { Step("scanning map markers"); ScanMarkers(world); Step("map markers scanned"); }
    int flagCounts[4] = {0, 0, 0, 0};  // [discovered*2 + flag 0x01]
    for (const auto& c : markerCache) {
        auto* ref = RE::TESForm::GetFormByID<RE::TESObjectREFR>(c.formId);
        if (!ref || ref->IsDisabled()) continue;
        const auto* bytes = MapMarkerBytes(ref);
        if (!bytes) continue;
        Pipboy::Location location;
        location.pipId = c.formId;
        location.x = c.x;
        location.y = c.y;
        location.name = c.name;
        location.type = c.type;
#if defined(PHOSPHOR_AE)
        location.discovered = (MarkerFlags(bytes) & kMarkerDiscovered) != 0;
#else
        location.discovered = (bytes[kMarkerFlags] & kMarkerDiscovered) != 0;
#endif
        // Every enabled, named marker is a compass candidate; Live keeps those within iLocationMarkerMeters and the HUD
        // draws undiscovered ones hollow and grey (MCM "Show undiscovered locations on compass" hides them). 0.9.22
        // required flag 0x01 here, but a user log (2026-10-22) showed nearby undiscovered places never carried it, so
        // the flag is only logged per worldspace for diagnosis. Quest-gated markers stay hidden: they are disabled refs.
        location.visible = true;
#if defined(PHOSPHOR_AE)
        const bool known = (MarkerFlags(bytes) & kMarkerVisible) != 0;
#else
        const bool known = (bytes[kMarkerFlags] & kMarkerVisible) != 0;
#endif
        ++flagCounts[(location.discovered ? 2 : 0) + (known ? 1 : 0)];
        location.workshop = c.workshop;
        location.markerFormId = c.formId;
        out.locations.push_back(std::move(location));
    }
    static std::uint32_t flagsLogged = UINT32_MAX;
    if (flagsLogged != cachedSpace && !out.locations.empty()) {
        flagsLogged = cachedSpace;
        spdlog::info("Engine quests: worldspace {:08X} locations: {} undiscovered (flag 0x01 on {}), {} discovered (flag 0x01 on {})",
                     cachedSpace, flagCounts[0] + flagCounts[1], flagCounts[1], flagCounts[2] + flagCounts[3], flagCounts[3]);
    }
    Step("done");
    static std::uint64_t version = 0;
    out.version = ++version;
    return out;
}

void Compare(const Pipboy::Snapshot& engine, const Pipboy::Snapshot& feed) {
    static int runs = 0;
    if (runs >= 12 || !feed.connected) return;
    ++runs;
    int issues = 0;
    const auto note = [&](const std::string& line) {
        if (issues++ < 25) spdlog::info("Engine vs feed: {}", line);
    };
    for (const auto& q : feed.quests) {
        if (!q.active) continue;
        const Pipboy::Quest* e = nullptr;
        for (const auto& x : engine.quests) if (x.formId == q.formId) e = &x;
        if (!e) { note(std::format("tracked quest {:08X} '{}' missing", q.formId, q.text)); continue; }
        if (e->text != q.text) note(std::format("quest {:08X} name feed '{}' engine '{}'", q.formId, q.text, e->text));
        if (!e->active) note(std::format("quest {:08X} '{}' not tracked in engine", q.formId, q.text));
        for (const auto& o : q.objectives) {
            bool found = false;
            for (const auto& eo : e->objectives) found = found || (eo.text == o.text && eo.completed == o.completed);
            if (!found) note(std::format("quest '{}' objective '{}' (done {}) missing or different", q.text, o.text, o.completed));
        }
    }
    int matched = 0;
    for (const auto& m : feed.markers) {
        const Pipboy::QuestMarker* best = nullptr;
        double bestDist = 1e18;
        for (const auto& e : engine.markers) {
            const double d = std::hypot(e.x - m.x, e.y - m.y);
            if (d < bestDist) { bestDist = d; best = &e; }
        }
        if (best && bestDist < 300 && best->name == m.name) ++matched;
        else note(std::format("marker '{}' ({:.0f},{:.0f}) door {}: nearest engine '{}' at {:.0f} units", m.name, m.x, m.y, m.onDoor,
                              best ? best->name : std::string("-"), bestDist));
    }
    for (const auto& e : engine.markers) {
        bool known = false;
        for (const auto& m : feed.markers) known = known || std::hypot(e.x - m.x, e.y - m.y) < 300;
        if (!known) note(std::format("extra engine marker '{}' ({:.0f},{:.0f}) door {}", e.name, e.x, e.y, e.onDoor));
    }
    int locMatched = 0, locDiff = 0;
    for (const auto& l : feed.locations) {
        const Pipboy::Location* e = nullptr;
        for (const auto& x : engine.locations) if (x.markerFormId == l.markerFormId) { e = &x; break; }
        if (!e) continue;
        if (e->name == l.name && e->type == l.type && e->discovered == l.discovered && e->workshop == l.workshop) ++locMatched;
        else if (locDiff++ < 10)
            note(std::format("location {:08X} feed '{}' t{} d{} v{} w{} | engine '{}' t{} d{} v{} w{}", l.markerFormId, l.name, l.type, l.discovered,
                             l.visible, l.workshop, e->name, e->type, e->discovered, e->visible, e->workshop));
    }
    const auto same = [](const std::optional<CustomWaypoint>& a, const std::optional<CustomWaypoint>& b) {
        return a.has_value() == b.has_value() && (!a || std::hypot(a->x - b->x, a->y - b->y) < 50);
    };
    if (!same(engine.customWorld, feed.customWorld)) note("custom waypoint (world) differs");
    spdlog::info("Engine vs feed #{}: quests {}/{} markers matched {}/{} (engine {}) locations {}/{} matched {} differ {} | {} issues",
                 runs, engine.quests.size(), feed.quests.size(), matched, feed.markers.size(), engine.markers.size(),
                 engine.locations.size(), feed.locations.size(), locMatched, locDiff, issues);
}
}  // namespace Phosphor::EngineQuests
