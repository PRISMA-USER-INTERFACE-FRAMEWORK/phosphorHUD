#pragma once
// Engine structures that CommonLibF4-NG leaves opaque, mirrored from libxse/commonlibf4 (AE) and checked on OG
// 1.10.163 with the engine probe (2026-10-20: quest targets, door paths, objectives, map marker data all matched
// the Pip-Boy feed). Pointers coming from these mirrors are tested with Readable() before use.
#include <RE/Fallout.h>
#include <Windows.h>

#include <cstdint>
#include <string>

namespace Phosphor::EngineLayout {
struct TeleportLinkM {
    RE::TESObjectREFR* door;               // 00
    RE::NiPoint3 doorTeleportLocation;     // 08
};
static_assert(sizeof(TeleportLinkM) == 0x18);
struct SpaceNodeM {
    bool worldspace;                       // 00
    RE::TESWorldSpace* worldSpace;         // 08
    RE::TESObjectCELL* interiorCell;       // 10
};
static_assert(sizeof(SpaceNodeM) == 0x18);
struct TeleportPathM {
    RE::BSTArray<SpaceNodeM> spaces;       // 00
    RE::BSTArray<TeleportLinkM> doors;     // 18: first door is in the player's current space
    RE::NiPoint3 start;                    // 30
    RE::NiPoint3 end;                      // 3C: world position of the target when worldMapOnly
    bool worldMapOnly;                     // 48
};
static_assert(sizeof(TeleportPathM) == 0x50);
struct RefDataM {
    TeleportPathM teleportPath;            // 00
    RE::ObjectRefHandle reference;         // 50
    void* pathMessageQueue;                // 58
    bool forceRebuildQueued;               // 60
};
struct QuestTargetM {
    std::uint8_t flags;                    // 00
    void* conditions;                      // 08
    std::uint32_t targetAlias;             // 10
    RE::BGSKeyword* keyword;               // 18
    RE::BSTArray<RefDataM*> target;        // 20
};
static_assert(sizeof(QuestTargetM) == 0x38);
struct QuestTargetArrayM {
    RE::BSTArray<QuestTargetM*> targets;   // 00
    std::uint64_t nextRepath;              // 18
};
static_assert(sizeof(QuestTargetArrayM) == 0x20);
struct QuestObjectiveM {
    void* displayText;                     // 00 BGSLocalizedString: pool entry
    RE::TESQuest* ownerQuest;              // 08
    QuestTargetM** targets;                // 10
    std::uint32_t numTargets;              // 18
    std::uint16_t index;                   // 1C
    bool initialized;                      // 1E
    char state;                            // 1F
    std::uint32_t flags;                   // 20
};
static_assert(sizeof(QuestObjectiveM) == 0x28);
// MapMarkerData: TESFullName at 00 (name entry at 08), then flags 10, originalFlags 11, type 12.
constexpr std::size_t kMarkerNameEntry = 0x08, kMarkerFlags = 0x10, kMarkerType = 0x12;
constexpr std::uint8_t kMarkerVisible = 0x01;      // "known" on the Pip-Boy map; not reliably set for nearby undiscovered places
constexpr std::uint8_t kMarkerDiscovered = 0x02;   // shown as discovered on the Pip-Boy map
constexpr std::uint16_t kQuestTracked = 0x0800;    // TESQuest::data.flags: tracked in the Pip-Boy
constexpr std::int8_t kQuestTypeMisc = 6;          // TESQuest::data.questType: Miscellaneous

inline bool Readable(const void* p, std::size_t n) {
    if (!p || !n) return false;
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

// Text of a string-pool entry (BSFixedString's storage), with readability checks.
inline std::string PoolText(const void* entry) {
#if defined(PHOSPHOR_AE)
    // Bound shallow chains and validate the entire string, rather than only its first byte.
    auto* leaf = static_cast<const RE::BSStringPool::Entry*>(entry);
    for (unsigned depth = 0; depth < 64; ++depth) {
        if (!Readable(leaf, sizeof(*leaf))) return {};
        if (!leaf->shallow()) {
            if (leaf->wide() || leaf->_length > 16384) return {};
            const auto* text = reinterpret_cast<const char*>(leaf + 1);
            if (!Readable(text, static_cast<std::size_t>(leaf->_length) + 1)) return {};
            return std::string(text, leaf->_length);
        }
        leaf = leaf->_right;
    }
    return {};
#else
    if (!entry || !Readable(entry, 0x18)) return {};
    const auto* leaf = static_cast<const RE::BSStringPool::Entry*>(entry)->leaf();
    if (!leaf || !Readable(leaf, 0x18)) return {};
    const char* text = leaf->data<char>();
    return text && Readable(text, 1) ? std::string(text) : std::string{};
#endif
}

inline RE::ExtraMapMarker* MapMarkerExtra(RE::TESObjectREFR* ref) {
    if (!ref || !ref->extraList) return nullptr;
    auto* extra = ref->extraList->GetByType(RE::EXTRA_DATA_TYPE::kMapMarker);
    return extra && Readable(extra, sizeof(RE::ExtraMapMarker)) ? static_cast<RE::ExtraMapMarker*>(extra) : nullptr;
}
inline const std::uint8_t* MapMarkerBytes(RE::TESObjectREFR* ref) {
    auto* extra = MapMarkerExtra(ref);
    const void* data = extra ? static_cast<const void*>(extra->mapMarkerData) : nullptr;
    return data && Readable(data, 0x18) ? static_cast<const std::uint8_t*>(data) : nullptr;
}
}  // namespace Phosphor::EngineLayout
#if defined(PHOSPHOR_AE)
#include "../ae/EngineLayoutChecks.h"
#endif
