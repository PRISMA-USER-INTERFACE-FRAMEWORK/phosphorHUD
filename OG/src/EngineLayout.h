#pragma once
#include <RE/Fallout.h>
#include <Windows.h>

#include <cstddef>
#include <cstdint>
#include <string>

namespace Phosphor::EngineLayout {
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
    RE::BSTArray<SpaceNodeM> spaces;
    RE::BSTArray<TeleportLinkM> doors;
    RE::NiPoint3 start;
    RE::NiPoint3 end;
    bool worldMapOnly;
};
static_assert(sizeof(TeleportPathM) == 0x50);
struct RefDataM {
    TeleportPathM teleportPath;
    RE::ObjectRefHandle reference;
    void* pathMessageQueue;
    bool forceRebuildQueued;
};
struct QuestTargetM {
    std::uint8_t flags;
    void* conditions;
    std::uint32_t targetAlias;
    RE::BGSKeyword* keyword;
    RE::BSTArray<RefDataM*> target;
};
static_assert(sizeof(QuestTargetM) == 0x38);
struct QuestTargetArrayM {
    RE::BSTArray<QuestTargetM*> targets;
    std::uint64_t nextRepath;
};
static_assert(sizeof(QuestTargetArrayM) == 0x20);
struct QuestObjectiveM {
    void* displayText;
    RE::TESQuest* ownerQuest;
    QuestTargetM** targets;
    std::uint32_t numTargets;
    std::uint16_t index;
    bool initialized;
    char state;
    std::uint32_t flags;
};
static_assert(sizeof(QuestObjectiveM) == 0x28);
constexpr std::size_t kMarkerNameEntry = 0x08, kMarkerFlags = 0x10, kMarkerType = 0x12;
constexpr std::uint8_t kMarkerVisible = 0x01;
constexpr std::uint8_t kMarkerDiscovered = 0x02;
constexpr std::uint16_t kQuestTracked = 0x0800;
constexpr std::int8_t kQuestTypeMisc = 6;

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

inline std::string PoolText(const void* entry) {
#if defined(PHOSPHOR_AE)
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
#if defined(PHOSPHOR_AE)
inline RE::BSSpinLock& EquippedItemsLock(RE::MiddleHighProcessData& middleHigh) { return middleHigh.equippedItemsLock; }
inline RE::BSTArray<RE::EquippedItem>& EquippedItems(RE::MiddleHighProcessData& middleHigh) { return middleHigh.equippedItems; }
inline RE::BSTValueEventSource<RE::PlayerAutoAimActorEvent>& AutoAimActor(RE::PlayerCharacter& player) { return player.autoAimActor; }
inline std::int8_t& PerkCount(RE::PlayerCharacter& player) { return player.perkCount; }
inline RE::ObjectRefHandle& PlayerMapMarker(RE::PlayerCharacter& player) { return player.playerMapMarker; }
#else
inline constexpr std::size_t kMiddleHighEquippedLock = 0x280;
inline constexpr std::size_t kMiddleHighEquippedItems = 0x288;
inline constexpr std::size_t kPlayerAutoAimActor = 0xC50;
inline constexpr std::size_t kPlayerPerkCount = 0xCF1;
inline constexpr std::size_t kPlayerMapMarker = 0xBC4;

template <class T>
T& AtFlatPosition(void* base, std::size_t position) {
    return *reinterpret_cast<T*>(static_cast<std::byte*>(base) + position);
}
inline RE::BSSpinLock& EquippedItemsLock(RE::MiddleHighProcessData& middleHigh) {
    return AtFlatPosition<RE::BSSpinLock>(&middleHigh, kMiddleHighEquippedLock);
}
inline RE::BSTArray<RE::EquippedItem>& EquippedItems(RE::MiddleHighProcessData& middleHigh) {
    return AtFlatPosition<RE::BSTArray<RE::EquippedItem>>(&middleHigh, kMiddleHighEquippedItems);
}
inline RE::BSTValueEventSource<RE::PlayerAutoAimActorEvent>& AutoAimActor(RE::PlayerCharacter& player) {
    return AtFlatPosition<RE::BSTValueEventSource<RE::PlayerAutoAimActorEvent>>(&player, kPlayerAutoAimActor);
}
inline std::int8_t& PerkCount(RE::PlayerCharacter& player) { return AtFlatPosition<std::int8_t>(&player, kPlayerPerkCount); }
inline RE::ObjectRefHandle& PlayerMapMarker(RE::PlayerCharacter& player) {
    return AtFlatPosition<RE::ObjectRefHandle>(&player, kPlayerMapMarker);
}
#if defined(ENABLE_FALLOUT_VR)
static_assert(offsetof(RE::MiddleHighProcessData, equippedItemsLock) == kMiddleHighEquippedLock + 0x8);
static_assert(offsetof(RE::MiddleHighProcessData, equippedItems) == kMiddleHighEquippedItems + 0x8);
static_assert(offsetof(RE::PlayerCharacter, autoAimActor) == kPlayerAutoAimActor + 0x470);
static_assert(offsetof(RE::PlayerCharacter, perkCount) == kPlayerPerkCount + 0x470);
static_assert(offsetof(RE::PlayerCharacter, playerMapMarker) == kPlayerMapMarker + 0x470);
#else
static_assert(offsetof(RE::MiddleHighProcessData, equippedItemsLock) == kMiddleHighEquippedLock);
static_assert(offsetof(RE::MiddleHighProcessData, equippedItems) == kMiddleHighEquippedItems);
static_assert(offsetof(RE::PlayerCharacter, autoAimActor) == kPlayerAutoAimActor);
static_assert(offsetof(RE::PlayerCharacter, perkCount) == kPlayerPerkCount);
static_assert(offsetof(RE::PlayerCharacter, playerMapMarker) == kPlayerMapMarker);
#endif
static_assert(offsetof(RE::PlayerCharacter, questTargetLock) == 0x630);
static_assert(offsetof(RE::PlayerCharacter, questTargets) == 0x7F0);
static_assert(offsetof(RE::PlayerCharacter, objectives) == 0x7D8);
static_assert(offsetof(RE::PlayerCharacter, currentMapMarkers) == 0x738);
static_assert(offsetof(RE::PlayerCharacter, cachedWorldspace) == 0x8B0);
static_assert(offsetof(RE::PlayerCharacter, exteriorPosition) == 0x8B8);
static_assert(offsetof(RE::Actor, currentProcess) == 0x300);
static_assert(offsetof(RE::AIProcess, middleHigh) == 0x08);
#endif
}
#if defined(PHOSPHOR_AE)
#include "../ae/EngineLayoutChecks.h"
#endif
