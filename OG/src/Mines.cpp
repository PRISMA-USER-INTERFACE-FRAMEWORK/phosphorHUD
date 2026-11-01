#include "Mines.h"
#include "Live.h"
#include "EventInbox.h"
#include <F4SE/F4SE.h>
#include <RE/Fallout.h>
#include <Windows.h>
#include <spdlog/spdlog.h>
#if defined(PHOSPHOR_AE)
#include "../ae/AeRuntime.h"
#endif
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <map>
#include <mutex>
#include <numbers>
#include <set>

namespace Phosphor::Mines {
namespace {
using Clock = std::chrono::steady_clock;
constexpr double kUnitsPerMeter = 70.0;
constexpr double kShowRangeMeters = 30.0;

// Only form ids cross threads. Never keep the event itself: its NiPointer would hold the reference alive across a
// save load, and dropping it after the game has torn the world down frees the reference behind the engine's back.
struct AttachNote { std::uint32_t id = 0; bool attached = false; };
EventInbox<AttachNote> attachEvents{4096};
std::set<std::uint32_t> tracked;  // game thread only
bool sinkRegistered = false;

RE::Projectile* AsMine(RE::TESObjectREFR* ref) {
    if (!ref) return nullptr;
    auto* projectile = RE::fallout_cast<RE::Projectile*>(ref);
    return projectile && projectile->IsMine() ? projectile : nullptr;
}

class AttachWatch final : public RE::BSTEventSink<RE::TESCellAttachDetachEvent> {
    RE::BSEventNotifyControl ProcessEvent(const RE::TESCellAttachDetachEvent& event,
                                         RE::BSTEventSource<RE::TESCellAttachDetachEvent>*) override {
#if defined(PHOSPHOR_AE)
        if (!AE::Enabled("bMines")) return RE::BSEventNotifyControl::kContinue;
#endif
        try {
#if defined(PHOSPHOR_AE)
            if (!event.refr) return RE::BSEventNotifyControl::kContinue;
            attachEvents.Push(AttachNote{event.refr->GetFormID(), event.isAttaching});
#else
            if (!event.reference) return RE::BSEventNotifyControl::kContinue;
            attachEvents.Push(AttachNote{event.reference->GetFormID(), event.attached});
#endif
        } catch (...) {}
        return RE::BSEventNotifyControl::kContinue;
    }
} attachWatch;

struct Seen {
    bool triggered = false;
    double x = 0, y = 0;
    Clock::time_point logged{};
    float lastTimer = -1;
};
std::map<std::uint32_t, Seen> seen;
Clock::time_point lastScan{};

void DrainAttachEvents() {
    auto batch = attachEvents.Take();
    if (batch.dropped) {
        spdlog::warn("Cell event queue full: skipped {} events; rescanning attached cells", batch.dropped);
        lastScan = {};
    }
    for (const auto& note : batch.events) {
        if (!note.attached) { tracked.erase(note.id); continue; }
        auto* ref = RE::TESForm::GetFormByID<RE::TESObjectREFR>(note.id);
        if (ref && !ref->IsDeleted() && AsMine(ref)) tracked.insert(note.id);
    }
}

// Rebuild from attached cells, including exterior neighbors: reset/overflow can lose attach events.
void ScanAttachedCells() {
    auto* player = RE::PlayerCharacter::GetSingleton();
    auto* cell = player ? player->GetParentCell() : nullptr;
    if (!cell) return;
    std::set<std::uint32_t> found;
    const auto scan = [&found](RE::TESObjectCELL* current) {
        RE::BSAutoLock guard{current->spinLock};
        for (auto& ref : current->references) {
            if (AsMine(ref.get())) found.insert(ref->GetFormID());
        }
    };
    scan(cell);
    if (cell->IsExterior()) {
        if (auto* data = RE::TESDataHandler::GetSingleton()) {
            for (auto* neighbor : data->GetFormArray<RE::TESObjectCELL>()) {
                if (!neighbor || neighbor == cell || !neighbor->IsExterior() ||
#if defined(PHOSPHOR_AE)
                    neighbor->cellState != RE::TESObjectCELL::CELL_STATE::kAttached) continue;
#else
                    neighbor->cellState.get() != RE::TESObjectCELL::CELL_STATE::kAttached) continue;
#endif
                if (neighbor->worldSpace == cell->worldSpace) scan(neighbor);
            }
        }
    }
    tracked = std::move(found);  // also recover missed detach events after inbox overflow
}

double Norm(double degrees) {
    degrees = std::fmod(degrees, 360.0);
    return degrees < 0 ? degrees + 360.0 : degrees;
}

// The game window's client size, for mapping screen fractions onto the HUD's letterboxed 1920x1080 stage.
std::pair<double, double> WindowSize() {
    static HWND window = nullptr;
    if (!window || !IsWindow(window)) window = FindWindowA("Fallout4", nullptr);
    RECT r{};
    if (window && GetClientRect(window, &r) && r.right > 0 && r.bottom > 0) return {double(r.right), double(r.bottom)};
    return {1920.0, 1080.0};
}

// World point -> fractions of the screen (0,0 top left). False when the point is behind the camera.
bool Project(const RE::NiPoint3& p, double& fx, double& fy) {
    auto* camera = RE::Main::WorldRootCamera();
    if (!camera) return false;
    const auto& m = camera->worldToCam;
    const double w = m[3][0] * p.x + m[3][1] * p.y + m[3][2] * p.z + m[3][3];
    if (w <= 0.00001) return false;
    const double x = (m[0][0] * p.x + m[0][1] * p.y + m[0][2] * p.z + m[0][3]) / w;
    const double y = (m[1][0] * p.x + m[1][1] * p.y + m[1][2] * p.z + m[1][3]) / w;
    const auto& port = camera->port;
    const double width = std::abs(port.right - port.left) > 0.01 ? port.right - port.left : 1.0;
    const double height = std::abs(port.top - port.bottom) > 0.01 ? port.top - port.bottom : 1.0;
    const double left = std::abs(port.right - port.left) > 0.01 ? port.left : 0.0;
    const double bottom = std::abs(port.top - port.bottom) > 0.01 ? port.bottom : 0.0;
    fx = (x * 0.5 + 0.5) * width + left;
    fy = 1.0 - ((y * 0.5 + 0.5) * height + bottom);
    return true;
}
}

void RegisterSinks() {
#if defined(PHOSPHOR_AE)
    AE_GATE("bMines", );
#endif
    if (sinkRegistered) return;
#if defined(PHOSPHOR_AE)
    auto* source = RE::TESCellAttachDetachEvent::GetEventSource();
    if (!source) { AE::Observe("bMines event source", false); return; }
    source->RegisterSink(&attachWatch);
#else
    if (REL::Module::IsNG()) { spdlog::warn("Mines: not verified on NG; cell attach sink not registered"); return; }
    RE::RegisterForCellAttachDetach(&attachWatch);
#endif
    sinkRegistered = true;
}

void Reset() {
    SetEventCapture(false);
    seen.clear();
    tracked.clear();
    lastScan = {};
}

void SetEventCapture(bool enabled) { attachEvents.SetEnabled(enabled); }

Json Read(std::vector<std::string>* exploded) {
#if defined(PHOSPHOR_AE)
    AE_GATE("bMines", Json::array());
#endif
    DrainAttachEvents();
    Json out = Json::array();
    auto* player = RE::PlayerCharacter::GetSingleton();
#if defined(PHOSPHOR_AE)
    if (!player) aeReadScope.Fail();
#endif
    if (!player) return out;
    const auto now = Clock::now();
    if (now - lastScan > std::chrono::seconds(1)) { ScanAttachedCells(); lastScan = now; }

    const std::vector<std::uint32_t> ids(tracked.begin(), tracked.end());
    const auto [ww, wh] = WindowSize();
    const double scale = std::min(ww / 1920.0, wh / 1080.0);
    const double offX = (ww - 1920.0 * scale) / 2, offY = (wh - 1080.0 * scale) / 2;
    const auto& pp = player->data.location;
    const double heading = Live::Heading();

    std::set<std::uint32_t> alive;
    for (const auto id : ids) {
        auto* ref = RE::TESForm::GetFormByID<RE::TESObjectREFR>(id);
        auto* mine = ref && !ref->IsDeleted() && !ref->IsDisabled() && ref->GetParentCell() ? AsMine(ref) : nullptr;
        if (!mine) {
            tracked.erase(id);
            continue;
        }
        alive.insert(id);
        auto& s = seen[id];
        // Mines the player laid aren't a threat to the player.
        if (auto shooter = mine->shooter.get(); shooter && shooter.get() == player) continue;
        const auto& mp = mine->data.location;
        const double dx = mp.x - pp.x, dy = mp.y - pp.y, dz = mp.z - pp.z;
        const double dist = std::sqrt(dx * dx + dy * dy + dz * dz) / kUnitsPerMeter;
        if (dist > kShowRangeMeters) { s.triggered = false; continue; }

        auto* base = mine->GetObjectReference() ? mine->GetObjectReference()->As<RE::BGSProjectile>() : nullptr;
        const float total = base ? base->data.explosionTimer : 0.0F;
        const float proximity = base ? base->data.explosionProximity : 0.0F;
        const bool sound = mine->sndCountdown.soundID != 0 && mine->sndCountdown.soundID != 0xFFFFFFFF;
        const bool counting = total > 0 && mine->explosionTimer > 0.0F && mine->explosionTimer < total - 0.01F;
        const bool triggered = sound || counting;
        const double nearRange = std::max(6.0, 2.0 * proximity / kUnitsPerMeter);
        // Raw values, while close, to confirm how the fuse reads in game.
        if (dist < 12 && (triggered != s.triggered || (triggered && now - s.logged > std::chrono::milliseconds(500)) ||
                          (!triggered && now - s.logged > std::chrono::seconds(10)))) {
            spdlog::info("Mine {:08X} {:.1f} m: timer {:.2f} / {:.2f}, proximity {:.0f}, countdown sound {:08X}, flags {:X}, age {:.1f}",
                         id, dist, mine->explosionTimer, total, proximity, mine->sndCountdown.soundID, mine->flags, mine->age);
            s.logged = now;
        }
        s.triggered = triggered;
        if (!triggered && dist > nearRange) continue;

        double b = Norm(std::atan2(dx, dy) * 180.0 / std::numbers::pi);
        char key[16];
        std::snprintf(key, sizeof(key), "%08x", id);
        std::string name;
        if (mine->weaponSource.object) name = Live::CleanName(Live::ToUtf8(RE::TESFullName::GetFullName(*mine->weaponSource.object)), true);
        if (name.empty() && base) name = Live::CleanName(Live::ToUtf8(RE::TESFullName::GetFullName(*base)), true);
        if (name.empty()) name = "Mine";
        Json m{{"id", key}, {"name", name}, {"dist", std::round(dist * 10) / 10}, {"disarm", dist <= 2.2},
               {"b", std::round(b)}};
        if (triggered && total > 0) {
            m["total"] = std::round(total * 10) / 10;
            m["left"] = std::round(std::clamp(mine->explosionTimer > 0 ? mine->explosionTimer : total, 0.0F, total) * 10) / 10;
        }
        double fx = 0, fy = 0;
        RE::NiPoint3 aim = mp;
        aim.z += 8;
        if (Project(aim, fx, fy) && fx >= 0 && fx <= 1 && fy >= 0 && fy <= 1) {
            const double sx = (fx * ww - offX) / scale, sy = (fy * wh - offY) / scale;
            m["x"] = std::round(std::clamp(sx, 0.0, 1920.0));
            m["y"] = std::round(std::clamp(sy, 0.0, 1080.0));
            s.x = m["x"].get<double>(); s.y = m["y"].get<double>();
        } else {
            m["onScreen"] = false;
            m["bearing"] = std::round(Norm(b - heading));
        }
        out.push_back(std::move(m));
    }
    // Gone while its fuse was running: it went off.
    for (auto it = seen.begin(); it != seen.end();) {
        if (alive.contains(it->first)) { ++it; continue; }
        if (it->second.triggered && exploded) {
            char key[16];
            std::snprintf(key, sizeof(key), "%08x", it->first);
            exploded->push_back(key);
        }
        it = seen.erase(it);
    }
    std::sort(out.begin(), out.end(), [](const Json& a, const Json& b) { return a["dist"].get<double>() < b["dist"].get<double>(); });
    if (out.size() > 8) out.erase(out.begin() + 8, out.end());
    return out;
}
}
