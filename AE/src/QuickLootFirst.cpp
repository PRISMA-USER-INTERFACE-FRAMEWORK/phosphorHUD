#include "QuickLootFirst.h"

#include <F4SE/F4SE.h>
#include <RE/Fallout.h>
#include <Windows.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <format>
#include <string>
#include <vector>

#if defined(PHOSPHOR_AE)
// The AE edition keeps the game's own behavior until this layout has been checked on AE.
namespace Phosphor::QuickLootFirst {
void DataReady() {}
void Maintain() {}
void Configure(bool, int, int) {}
void Update(bool) {}
}
#else
namespace Phosphor::QuickLootFirst {
namespace {
// BGSEntryPointPerkEntry (not described by CommonLibF4): byte entry point, byte function, byte condition-tab count,
// then the function data, the per-tab condition lists and the owning perk. Every value is checked before use.
constexpr std::size_t kEntryPointByte = 0x10, kTabCountByte = 0x12, kFunctionData = 0x18, kConditions = 0x20,
                      kOwnerPerk = 0x28, kEntrySize = 0x30;
constexpr std::uint8_t kActivateEntryPoint = 0x0E;   // BGSEntryPoint::ENTRY_POINT::kActivate
constexpr std::size_t kTargetTab = 1;                // Activate tabs: 0 perk owner, 1 target
constexpr std::uint16_t kGetDead = 46, kGetGlobalValue = 74, kGetIsObjectType = 432;  // condition function indices
constexpr std::uintptr_t kObjectTypeContainer = 3;   // GetIsObjectType: 3 = container (vanilla Dogmeat/Cait checks)

std::atomic<bool> ready{false};          // installed and calibrated: Update may close the gate
bool dataLoaded = false, attempted = false;  // game thread only
std::atomic<bool> enabled{true};
std::atomic<int> keyCode{0}, keyModifiers{0};
RE::TESGlobal* gate = nullptr;           // 1 = perk choices allowed everywhere, 0 = not on containers/bodies
RE::TESGlobal* dakPressed = nullptr;     // Dynamic Activation Key's "key is pressed" global, when installed
bool lastOpen = true;

bool Readable(const void* pointer, std::size_t size) {
    if (!pointer) return false;
    auto address = reinterpret_cast<std::uintptr_t>(pointer);
    while (size) {
        MEMORY_BASIC_INFORMATION info{};
        if (!VirtualQuery(reinterpret_cast<const void*>(address), &info, sizeof(info))) return false;
        if (info.State != MEM_COMMIT || (info.Protect & (PAGE_NOACCESS | PAGE_GUARD)) || !info.Protect) return false;
        const auto end = reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize;
        const auto chunk = std::min<std::uintptr_t>(size, end - address);
        address += chunk;
        size -= chunk;
    }
    return true;
}
template <class T> T Read(const std::uint8_t* base, std::size_t offset) {
    T value{};
    std::memcpy(&value, base + offset, sizeof(T));
    return value;
}
std::uint16_t FunctionOf(const RE::TESConditionItem* item) {
    std::uint16_t id = 0;
    std::memcpy(&id, &item->data.functionData.function, sizeof(id));
    return id;
}
// A condition list from the game: readable items with plausible function indices, at most 256 long.
bool PlausibleList(const RE::TESConditionItem* item, int& getDeadSeen) {
    for (int n = 0; item; ++n, item = item->next) {
        if (n > 256 || !Readable(item, sizeof(RE::TESConditionItem)) || FunctionOf(item) >= 0x400) return false;
        if (FunctionOf(item) == kGetDead) ++getDeadSeen;
    }
    return true;
}
RE::TESConditionItem* MakeItem(std::uint16_t function, void* param, float value, bool orNext) {
    auto* item = new RE::TESConditionItem{};
    std::memcpy(&item->data.functionData.function, &function, sizeof(function));
    item->data.functionData.param[0] = param;
    item->data.value = value;
    item->data.condition = RE::ENUM_COMPARISON_CONDITION::kEqual;
    item->data.compareOr = orNext ? 1 : 0;
    // object stays 0 (subject): on the target tab the subject is the activation target, like the mods' own GetDead.
    return item;
}
// (gate == 1 OR GetIsObjectType(container) == 0) AND (gate == 1 OR GetDead == 0), followed by `rest`.
RE::TESConditionItem* MakeGateChain(RE::TESConditionItem* rest) {
    auto* notDead = MakeItem(kGetDead, nullptr, 0.0F, false);
    notDead->next = rest;
    auto* gateForBodies = MakeItem(kGetGlobalValue, gate, 1.0F, true);
    gateForBodies->next = notDead;
    auto* notContainer = MakeItem(kGetIsObjectType, reinterpret_cast<void*>(kObjectTypeContainer), 0.0F, false);
    notContainer->next = gateForBodies;
    auto* gateForContainers = MakeItem(kGetGlobalValue, gate, 1.0F, true);
    gateForContainers->next = notContainer;
    return gateForContainers;
}
std::string Hex(const std::uint8_t* bytes, std::size_t size) {
    std::string text;
    for (std::size_t i = 0; i < size; ++i) text += std::format("{:02X}{}", bytes[i], (i % 8 == 7) ? " " : "");
    return text;
}
bool KeyDown(int code) { return code > 0 && (GetAsyncKeyState(code) & 0x8000) != 0; }
}  // namespace

void Configure(bool on, int code, int modifiers) {
    const bool changed = enabled.load() != on || keyCode.load() != code || keyModifiers.load() != modifiers;
    enabled.store(on);
    keyCode.store(code);
    keyModifiers.store(modifiers);
    if (changed)
        spdlog::info("Quick loot first: {} (perk-choices key {}{})", on ? "on" : "off", code > 0 ? std::format("VK {:02X}", code) : "unbound",
                     code > 0 && modifiers ? std::format(", modifiers {}", modifiers) : "");
}

void Update(bool foreground) {
    if (!ready.load() || !gate) return;
    const int code = keyCode.load(), modifiers = keyModifiers.load();
    bool held = foreground && KeyDown(code);
    if (held && (modifiers & 1)) held = KeyDown(VK_SHIFT);
    if (held && (modifiers & 2)) held = KeyDown(VK_CONTROL);
    if (held && (modifiers & 4)) held = KeyDown(VK_MENU);
    const bool dak = dakPressed && dakPressed->value >= 0.5F;
    const bool open = !enabled.load() || code <= 0 || held || dak;
    gate->value = open ? 1.0F : 0.0F;
    if (open != lastOpen) {
        lastOpen = open;
        spdlog::debug("Quick loot first: perk choices on containers/bodies {}", open ? "allowed" : "held back");
    }
}

static void Install() {
    auto* data = RE::TESDataHandler::GetSingleton();
    auto* player = RE::PlayerCharacter::GetSingleton();
    auto* factory = RE::ConcreteFormFactory<RE::TESGlobal>::GetFormFactory();
    if (!data || !player || !factory) { spdlog::warn("Quick loot first: game data unavailable; perk choices unchanged"); return; }

    // 1. Check the perk-entry layout on every entry-point entry before touching any of them.
    const auto entryVtable = RE::VTABLE::BGSEntryPointPerkEntry[0].address();
    const auto choiceVtable = RE::VTABLE::BGSEntryPointFunctionDataActivateChoice[0].address();
    struct Target { std::uint8_t* entry; std::size_t tabs; };
    std::vector<Target> targets;
    std::size_t entries = 0, ownerOk = 0, activate = 0, choiceElsewhere = 0, skipped = 0, perksTouched = 0;
    int getDeadSeen = 0;
    std::string sample;
    for (auto* perk : data->GetFormArray<RE::BGSPerk>()) {
        if (!perk) continue;
        bool touched = false;
        for (auto* entry : perk->perkEntries) {
            auto* raw = reinterpret_cast<std::uint8_t*>(entry);
            if (!Readable(raw, kEntrySize) || Read<std::uintptr_t>(raw, 0) != entryVtable) continue;
            ++entries;
            if (Read<RE::BGSPerk*>(raw, kOwnerPerk) == perk) ++ownerOk;
            const auto* function = Read<const std::uint8_t*>(raw, kFunctionData);
            const bool choice = Readable(function, 8) && Read<std::uintptr_t>(function, 0) == choiceVtable;
            if (raw[kEntryPointByte] != kActivateEntryPoint) { if (choice) ++choiceElsewhere; continue; }
            ++activate;
            if (sample.size() < 400) sample += std::format(" [{:08X}: {}]", perk->GetFormID(), Hex(raw, kEntrySize));
            const std::size_t tabs = raw[kTabCountByte];
            auto* lists = Read<RE::TESCondition*>(raw, kConditions);
            bool listsOk = tabs > kTargetTab && tabs <= 8 && (!lists || Readable(lists, tabs * sizeof(RE::TESCondition)));
            for (std::size_t t = 0; listsOk && lists && t < tabs; ++t) listsOk = PlausibleList(lists[t].head, getDeadSeen);
            if (!choice || !listsOk) { ++skipped; continue; }
            targets.push_back({raw, tabs});
            touched = true;
        }
        if (touched) ++perksTouched;
    }
    spdlog::info("Quick loot first: {} entry-point perk entries ({} owner matches), {} Activate entries, {} usable, {} skipped, "
                 "{} activate choices elsewhere, GetDead seen {} times", entries, ownerOk, activate, targets.size(), skipped,
                 choiceElsewhere, getDeadSeen);
    spdlog::debug("Quick loot first: sample Activate entries{}", sample);
    if (entries == 0 || ownerOk != entries || choiceElsewhere != 0 || targets.empty() || getDeadSeen == 0) {
        spdlog::warn("Quick loot first: perk entries do not match the expected layout; perk choices unchanged");
        return;
    }

    // 2. Create the gate and check that the game's own GetGlobalValue sees it.
    gate = factory->Create();
    if (!gate) { spdlog::warn("Quick loot first: could not create the gate; perk choices unchanged"); return; }
    RE::TESConditionItem probe{};
    std::uint16_t id = kGetGlobalValue;
    std::memcpy(&probe.data.functionData.function, &id, sizeof(id));
    probe.data.functionData.param[0] = gate;
    probe.data.value = 1.0F;
    probe.data.condition = RE::ENUM_COMPARISON_CONDITION::kEqual;
    RE::TESCondition check{};
    check.head = &probe;
    gate->value = 1.0F;
    const bool seesOpen = check.IsTrue(player, player);
    gate->value = 0.0F;
    const bool seesClosed = !check.IsTrue(player, player);
    gate->value = 1.0F;
    check.head = nullptr;
    if (!seesOpen || !seesClosed) {
        spdlog::warn("Quick loot first: the gate check failed (open {}, closed {}); perk choices unchanged", seesOpen, seesClosed);
        gate = nullptr;
        return;
    }

    // 3. Gate every Activate entry's target tab. The gate is open, so nothing changes until a key is bound.
    for (const auto& target : targets) {
        auto* lists = Read<RE::TESCondition*>(target.entry, kConditions);
        if (!lists) {
            lists = new RE::TESCondition[target.tabs]{};
            std::memcpy(target.entry + kConditions, &lists, sizeof(lists));
        }
        lists[kTargetTab].head = MakeGateChain(lists[kTargetTab].head);
    }
    dakPressed = data->LookupForm<RE::TESGlobal>(0x10, "Dynamic Activation Key.esp");
    ready.store(true);
    spdlog::info("Quick loot first: ready; {} perk choices in {} perks wait for the perk-choices key on containers and bodies{}",
                 targets.size(), perksTouched, dakPressed ? " (Dynamic Activation Key's key also works)" : "");
}

void DataReady() { dataLoaded = true; }

void Maintain() {
    if (attempted || !dataLoaded || !enabled.load() || keyCode.load() <= 0) return;
    attempted = true;  // one attempt per game session; a failed check leaves everything unchanged
    Install();
}
}  // namespace Phosphor::QuickLootFirst
#endif
