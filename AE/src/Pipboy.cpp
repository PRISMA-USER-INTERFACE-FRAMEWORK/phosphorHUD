#if defined(PHOSPHOR_AE)
#include "../ae/AeRuntime.h"
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include "Pipboy.h"
#include "Live.h"
#include <spdlog/spdlog.h>
#include <atomic>
#include <chrono>
#include <cstring>
#include <fstream>
#include <map>
#include <mutex>
#include <optional>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <variant>

// Protocol (community documented, e.g. NimVek/pipboy PROTOCOL.md): TCP 27000, little-endian packets of
// `uint32 size, uint8 channel, payload[size]`. Channel 0 keep-alive, 1 accepted, 2 refused (busy),
// 3 data update. A data update is a run of entries `uint8 type, uint32 id, value`; the database is a flat
// table of nodes and lists/objects refer to other nodes by id. Node 0 is the root object.

namespace Phosphor::Pipboy {
namespace {
using List = std::vector<std::uint32_t>;
using Object = std::map<std::string, std::uint32_t>;
using Value = std::variant<std::monostate, bool, std::int64_t, double, std::string, List, Object>;

std::mutex g_lock;
Snapshot g_snapshot;
std::stop_source g_stop;
std::atomic<bool> g_started{false};
std::string g_dumpPath;

class Database {
public:
    std::unordered_map<std::uint32_t, Value> nodes;

    // Applies one data update. Returns false on a malformed packet.
    bool Apply(const std::uint8_t* p, std::size_t n) {
        std::size_t i = 0;
        auto need = [&](std::size_t k) { return i + k <= n; };
        auto u8 = [&]() { return p[i++]; };
        auto u16 = [&]() { std::uint16_t v; std::memcpy(&v, p + i, 2); i += 2; return v; };
        auto u32 = [&]() { std::uint32_t v; std::memcpy(&v, p + i, 4); i += 4; return v; };
        auto cstr = [&](std::string& out) {
            const void* end = std::memchr(p + i, 0, n - i);
            if (!end) return false;
            const auto len = static_cast<const std::uint8_t*>(end) - (p + i);
            out.assign(reinterpret_cast<const char*>(p + i), static_cast<std::size_t>(len));
            i += static_cast<std::size_t>(len) + 1;
            return true;
        };
        while (i < n) {
            if (!need(5)) return false;
            const auto type = u8();
            const auto id = u32();
            switch (type) {
            case 0: if (!need(1)) return false; nodes[id] = u8() != 0; break;
            case 1: if (!need(1)) return false; nodes[id] = static_cast<std::int64_t>(static_cast<std::int8_t>(u8())); break;
            case 2: if (!need(1)) return false; nodes[id] = static_cast<std::int64_t>(u8()); break;
            case 3: if (!need(4)) return false; nodes[id] = static_cast<std::int64_t>(static_cast<std::int32_t>(u32())); break;
            case 4: if (!need(4)) return false; nodes[id] = static_cast<std::int64_t>(u32()); break;
            case 5: { if (!need(4)) return false; float f; std::memcpy(&f, p + i, 4); i += 4; nodes[id] = static_cast<double>(f); break; }
            case 6: { std::string s; if (!cstr(s)) return false; nodes[id] = std::move(s); break; }
            case 7: {
                if (!need(2)) return false;
                const auto count = u16();
                if (!need(std::size_t(count) * 4)) return false;
                List list(count);
                for (auto& ref : list) ref = u32();
                nodes[id] = std::move(list);
                break;
            }
            case 8: {
                if (!need(2)) return false;
                Object* object = std::get_if<Object>(&nodes[id]);
                if (!object) { nodes[id] = Object{}; object = std::get_if<Object>(&nodes[id]); }
                const auto inserts = u16();
                for (std::uint16_t k = 0; k < inserts; ++k) {
                    if (!need(4)) return false;
                    const auto ref = u32();
                    std::string name;
                    if (!cstr(name)) return false;
                    (*object)[name] = ref;
                }
                if (!need(2)) return false;
                const auto removes = u16();
                if (!need(std::size_t(removes) * 4)) return false;
                for (std::uint16_t k = 0; k < removes; ++k) {
                    const auto ref = u32();
                    for (auto it = object->begin(); it != object->end(); ++it)
                        if (it->second == ref) { object->erase(it); break; }
                }
                break;
            }
            default: return false;
            }
        }
        return true;
    }

    const Value* Get(std::uint32_t id) const { auto it = nodes.find(id); return it == nodes.end() ? nullptr : &it->second; }
    const Object* Obj(std::uint32_t id) const { auto v = Get(id); return v ? std::get_if<Object>(v) : nullptr; }
    const List* Lst(std::uint32_t id) const { auto v = Get(id); return v ? std::get_if<List>(v) : nullptr; }
    const Value* Child(const Object* o, const char* key) const {
        if (!o) return nullptr;
        auto it = o->find(key);
        return it == o->end() ? nullptr : Get(it->second);
    }
    std::optional<std::uint32_t> ChildId(const Object* o, const char* key) const {
        if (!o) return std::nullopt;
        auto it = o->find(key);
        if (it == o->end()) return std::nullopt;
        return it->second;
    }
    static std::optional<double> Num(const Value* v) {
        if (!v) return std::nullopt;
        if (auto b = std::get_if<bool>(v)) return *b ? 1.0 : 0.0;
        if (auto i = std::get_if<std::int64_t>(v)) return static_cast<double>(*i);
        if (auto d = std::get_if<double>(v)) return *d;
        return std::nullopt;
    }
    static bool Flag(const Value* v) { auto n = Num(v); return n && *n != 0; }
    static std::string Str(const Value* v) { auto s = v ? std::get_if<std::string>(v) : nullptr; return s ? Phosphor::Live::ToUtf8(*s) : std::string{}; }

    Json Dump(std::uint32_t id, int depth, std::unordered_set<std::uint32_t>& seen) const {
        const auto* v = Get(id);
        if (!v || depth > 24 || !seen.insert(id).second) return nullptr;
        Json out;
        if (auto o = std::get_if<Object>(v)) {
            out = Json::object();
            for (const auto& [k, ref] : *o) out[Phosphor::Live::ToUtf8(k)] = k == "Inventory" ? Json("(skipped)") : Dump(ref, depth + 1, seen);
        } else if (auto l = std::get_if<List>(v)) {
            out = Json::array();
            for (auto ref : *l) out.push_back(Dump(ref, depth + 1, seen));
        } else if (auto s = std::get_if<std::string>(v)) out = Phosphor::Live::ToUtf8(*s);
        else if (auto b = std::get_if<bool>(v)) out = *b;
        else if (auto i = std::get_if<std::int64_t>(v)) out = *i;
        else if (auto d = std::get_if<double>(v)) out = *d;
        seen.erase(id);
        return out;
    }
};

void Extract(const Database& db, Snapshot& out) {
    const auto* root = db.Obj(0);
    const auto map = db.ChildId(root, "Map");
    // Custom pins are separate objects, not entries in World.Quests or World.Locations.
    const auto custom = [&](const char* space) {
        const auto* area = map ? db.Obj(db.ChildId(db.Obj(*map), space).value_or(UINT32_MAX)) : nullptr;
        const auto* point = db.Obj(db.ChildId(area, "Custom").value_or(UINT32_MAX));
        return ReadCustomWaypoint(Database::Num(db.Child(point, "X")), Database::Num(db.Child(point, "Y")),
                                  Database::Flag(db.Child(point, "Visible")));
    };
    out.customWorld = custom("World");
    out.customLocal = custom("Local");
    const auto* world = map ? db.Obj(db.ChildId(db.Obj(*map), "World").value_or(UINT32_MAX)) : nullptr;
    if (world) {
        if (const auto* player = db.Obj(db.ChildId(world, "Player").value_or(UINT32_MAX))) {
            auto x = Database::Num(db.Child(player, "X")), y = Database::Num(db.Child(player, "Y"));
            out.hasPlayer = x && y;
            if (out.hasPlayer) { out.playerX = *x; out.playerY = *y; }
        }
        if (const auto* list = db.Lst(db.ChildId(world, "Locations").value_or(UINT32_MAX))) {
            for (auto ref : *list) {
                const auto* l = db.Obj(ref);
                if (!l) continue;
                auto x = Database::Num(db.Child(l, "X")), y = Database::Num(db.Child(l, "Y"));
                if (!x || !y) continue;
                Location loc;
                loc.pipId = ref;
                loc.x = *x; loc.y = *y;
                loc.name = Database::Str(db.Child(l, "Name"));
                loc.type = static_cast<int>(Database::Num(db.Child(l, "type")).value_or(-1));
                loc.discovered = Database::Flag(db.Child(l, "Discovered"));
                loc.visible = !db.Child(l, "Visible") || Database::Flag(db.Child(l, "Visible"));
                // Workshop fields may be present (false) on every location; a population entry or ownership marks a settlement.
                loc.workshop = Database::Flag(db.Child(l, "WorkshopOwned")) || db.Child(l, "WorkshopPopulation") != nullptr;
                loc.markerFormId = static_cast<std::uint32_t>(Database::Num(db.Child(l, "LocationMarkerFormId")).value_or(0));
                out.locations.push_back(std::move(loc));
            }
        }
        if (const auto* list = db.Lst(db.ChildId(world, "Quests").value_or(UINT32_MAX))) {
            for (auto ref : *list) {
                const auto* m = db.Obj(ref);
                if (!m) continue;
                QuestMarker marker;
                marker.pipId = ref;
                auto x = Database::Num(db.Child(m, "X")), y = Database::Num(db.Child(m, "Y")), z = Database::Num(db.Child(m, "Height"));
                if (!x || !y) continue;
                marker.x = *x; marker.y = *y;
                if (z) { marker.z = *z; marker.hasZ = true; }
                marker.onDoor = Database::Flag(db.Child(m, "OnDoor"));
                marker.name = Database::Str(db.Child(m, "Name"));
                if (const auto* ids = db.Lst(db.ChildId(m, "QuestId").value_or(UINT32_MAX)))
                    for (auto q : *ids) if (auto n = Database::Num(db.Get(q))) marker.quests.push_back(static_cast<std::uint32_t>(*n));
                out.markers.push_back(std::move(marker));
            }
        }
    }
    if (const auto* quests = db.Lst(db.ChildId(root, "Quests").value_or(UINT32_MAX))) {
        for (auto ref : *quests) {
            const auto* q = db.Obj(ref);
            if (!q || !Database::Flag(db.Child(q, "enabled"))) continue;
            Quest quest;
            quest.formId = static_cast<std::uint32_t>(Database::Num(db.Child(q, "formID")).value_or(0));
            quest.text = Database::Str(db.Child(q, "text"));
            quest.active = Database::Flag(db.Child(q, "active"));
            if (const auto* objectives = db.Lst(db.ChildId(q, "objectives").value_or(UINT32_MAX))) {
                for (auto oref : *objectives) {
                    const auto* o = db.Obj(oref);
                    if (!o || !Database::Flag(db.Child(o, "enabled"))) continue;
                    Objective objective;
                    objective.text = Database::Str(db.Child(o, "text"));
                    objective.completed = Database::Flag(db.Child(o, "completed"));
                    objective.active = db.Child(o, "active") ? Database::Flag(db.Child(o, "active")) : !objective.completed;
                    if (!objective.text.empty()) quest.objectives.push_back(std::move(objective));
                }
            }
            out.quests.push_back(std::move(quest));
        }
    }
}

bool RecvAll(SOCKET s, std::uint8_t* buffer, std::size_t n, std::stop_token& stop) {
    std::size_t got = 0;
    while (got < n) {
        if (stop.stop_requested()) return false;
        const int r = recv(s, reinterpret_cast<char*>(buffer + got), static_cast<int>(n - got), 0);
        if (r == SOCKET_ERROR && WSAGetLastError() == WSAETIMEDOUT) continue;
        if (r <= 0) return false;
        got += static_cast<std::size_t>(r);
    }
    return true;
}

void RunFeed(std::stop_token stop) {
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) { spdlog::error("Pip-Boy feed: Winsock unavailable"); return; }
    bool warned = false;
    while (!stop.stop_requested()) {
        SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(27000);
        inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);
        if (s == INVALID_SOCKET || connect(s, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
#if defined(PHOSPHOR_AE)
            AE::Observe("bPipboyFeed", false);
#endif
            if (s != INVALID_SOCKET) closesocket(s);
            if (!warned) {
                spdlog::warn("Pip-Boy feed not reachable on 127.0.0.1:27000. Quest markers and the tracker need "
                             "Settings > Gameplay > Pip-Boy App turned on. Retrying every 5 s.");
                warned = true;
            }
            for (int k = 0; k < 50 && !stop.stop_requested(); ++k) std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        DWORD timeout = 1000;
        setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
        spdlog::info("Pip-Boy feed connected");
        warned = false;
        Database db;
        bool dumped = false;
        const auto connectedAt = std::chrono::steady_clock::now();
        std::vector<std::uint8_t> payload;
        auto lastKeepAlive = std::chrono::steady_clock::now();
        while (!stop.stop_requested()) {
            if (std::chrono::steady_clock::now() - lastKeepAlive >= std::chrono::seconds(1)) {
                const std::uint8_t keepAlive[5] = {0, 0, 0, 0, 0};
                send(s, reinterpret_cast<const char*>(keepAlive), 5, 0);
                lastKeepAlive = std::chrono::steady_clock::now();
            }
            std::uint8_t header[5];
            if (!RecvAll(s, header, 5, stop)) break;
            std::uint32_t size;
            std::memcpy(&size, header, 4);
            const auto channel = header[4];
            if (size > 64 * 1024 * 1024) { spdlog::error("Pip-Boy feed: oversized packet ({} bytes); reconnecting", size); break; }
            payload.resize(size);
            if (size && !RecvAll(s, payload.data(), size, stop)) break;
            if (channel == 2) { spdlog::info("Pip-Boy feed busy (another app connected?)"); break; }
            if (channel != 3) continue;
            if (!db.Apply(payload.data(), payload.size())) { spdlog::error("Pip-Boy feed: malformed update; reconnecting"); break; }
            Snapshot snapshot;
            snapshot.connected = true;
            Extract(db, snapshot);
#if defined(PHOSPHOR_AE)
            AE::Observe("bPipboyFeed", true);
#endif
            {
                std::scoped_lock lock(g_lock);
                snapshot.version = g_snapshot.version + 1;
                g_snapshot = std::move(snapshot);
            }
            const auto since = std::chrono::steady_clock::now() - connectedAt;
            if (!dumped && !g_dumpPath.empty() && (since > std::chrono::seconds(20) || (db.nodes.size() > 2000 && since > std::chrono::seconds(4)))) {
                // One look at the whole tree (minus the inventory) for finding fields later stages can use.
                std::unordered_set<std::uint32_t> seen;
                std::ofstream(g_dumpPath, std::ios::binary) << db.Dump(0, 0, seen).dump(1, ' ', false, Json::error_handler_t::replace);
                spdlog::info("Pip-Boy feed: {} nodes; tree written to {}", db.nodes.size(), g_dumpPath);
                dumped = true;
            }
        }
        closesocket(s);
        {
            std::scoped_lock lock(g_lock);
            g_snapshot.connected = false;
            ++g_snapshot.version;
        }
        if (!stop.stop_requested()) {
            spdlog::info("Pip-Boy feed disconnected; retrying");
            for (int k = 0; k < 30 && !stop.stop_requested(); ++k) std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    WSACleanup();
}

// The feed runs on its own thread, so an exception escaping it would end the whole game at once (std::terminate,
// with no crash log). Anything thrown there (for example running out of memory on a large update) is logged and the
// feed starts over instead.
void Run(std::stop_token stop) {
    while (!stop.stop_requested()) {
        try {
            RunFeed(stop);
            return;
        } catch (const std::exception& e) {
            spdlog::error("Pip-Boy feed failed: {}; restarting in 3 s", e.what());
        } catch (...) {
            spdlog::error("Pip-Boy feed failed with an unknown exception; restarting in 3 s");
        }
        try {
            std::scoped_lock lock(g_lock);
            const auto version = g_snapshot.version;
            g_snapshot = Snapshot{};
            g_snapshot.version = version + 1;
        } catch (...) {}
        for (int k = 0; k < 30 && !stop.stop_requested(); ++k) std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}
}

void Start(const std::string& dumpPath) {
#if defined(PHOSPHOR_AE)
    if (!Phosphor::AE::Enabled("bPipboyFeed")) return;
#endif
    if (g_started.exchange(true)) return;
    g_dumpPath = dumpPath;
    // Detached: joining a thread during DLL unload can deadlock on the loader lock.
    std::thread([token = g_stop.get_token()] { Run(token); }).detach();
}
void Stop() { g_stop.request_stop(); }
Snapshot Latest() {
#if defined(PHOSPHOR_AE)
    if (!Phosphor::AE::Enabled("bPipboyFeed")) return {};
#endif
    std::scoped_lock lock(g_lock);
    return g_snapshot;
}
}
