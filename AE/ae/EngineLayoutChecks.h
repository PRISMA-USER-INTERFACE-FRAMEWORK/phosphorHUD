#pragma once
// Compiled against the pinned AE tree. No runtime probes or engine calls here.
#include <RE/B/BGSInstancedQuestObjective.h>
#include <RE/B/BGSQuestObjective.h>
#include <RE/M/MapMarkerData.h>
#include <RE/P/ParentSpaceNode.h>
#include <RE/Q/QuestTargetArray.h>
#include <RE/T/TESQuestTarget.h>
#include <RE/T/TeleportLink.h>
#include <RE/T/TeleportPath.h>
#include <cstddef>
#include <cstring>
#include <type_traits>

namespace Phosphor::EngineLayout {
#define AE_SIZE(mirror, native) static_assert(sizeof(mirror) == sizeof(RE::native))
#define AE_MEMBER(mirror, native, member) static_assert(offsetof(mirror, member) == offsetof(RE::native, member))
AE_SIZE(TeleportLinkM, TeleportLink);
AE_MEMBER(TeleportLinkM, TeleportLink, door);
AE_MEMBER(TeleportLinkM, TeleportLink, doorTeleportLocation);
AE_SIZE(SpaceNodeM, ParentSpaceNode);
AE_MEMBER(SpaceNodeM, ParentSpaceNode, worldspace);
AE_MEMBER(SpaceNodeM, ParentSpaceNode, worldSpace);
AE_MEMBER(SpaceNodeM, ParentSpaceNode, interiorCell);
AE_SIZE(TeleportPathM, TeleportPath);
AE_MEMBER(TeleportPathM, TeleportPath, spaces);
AE_MEMBER(TeleportPathM, TeleportPath, doors);
AE_MEMBER(TeleportPathM, TeleportPath, start);
AE_MEMBER(TeleportPathM, TeleportPath, end);
AE_MEMBER(TeleportPathM, TeleportPath, worldMapOnly);
AE_SIZE(RefDataM, TESQuestTarget::REF_DATA);
AE_MEMBER(RefDataM, TESQuestTarget::REF_DATA, teleportPath);
AE_MEMBER(RefDataM, TESQuestTarget::REF_DATA, reference);
AE_MEMBER(RefDataM, TESQuestTarget::REF_DATA, pathMessageQueue);
AE_MEMBER(RefDataM, TESQuestTarget::REF_DATA, forceRebuildQueued);
AE_SIZE(QuestTargetM, TESQuestTarget);
AE_MEMBER(QuestTargetM, TESQuestTarget, flags);
AE_MEMBER(QuestTargetM, TESQuestTarget, conditions);
AE_MEMBER(QuestTargetM, TESQuestTarget, targetAlias);
AE_MEMBER(QuestTargetM, TESQuestTarget, keyword);
AE_MEMBER(QuestTargetM, TESQuestTarget, target);
AE_SIZE(QuestTargetArrayM, QuestTargetArray);
AE_MEMBER(QuestTargetArrayM, QuestTargetArray, nextRepath);
static_assert(offsetof(QuestTargetArrayM, targets) == 0);
static_assert(std::is_base_of_v<RE::BSTArray<RE::TESQuestTarget*>, RE::QuestTargetArray>);
static_assert(sizeof(decltype(QuestTargetArrayM::targets)) == sizeof(RE::BSTArray<RE::TESQuestTarget*>));
AE_SIZE(QuestObjectiveM, BGSQuestObjective);
AE_MEMBER(QuestObjectiveM, BGSQuestObjective, displayText);
AE_MEMBER(QuestObjectiveM, BGSQuestObjective, ownerQuest);
AE_MEMBER(QuestObjectiveM, BGSQuestObjective, targets);
AE_MEMBER(QuestObjectiveM, BGSQuestObjective, numTargets);
AE_MEMBER(QuestObjectiveM, BGSQuestObjective, index);
AE_MEMBER(QuestObjectiveM, BGSQuestObjective, initialized);
AE_MEMBER(QuestObjectiveM, BGSQuestObjective, state);
AE_MEMBER(QuestObjectiveM, BGSQuestObjective, flags);
#undef AE_SIZE
#undef AE_MEMBER
#define AE_OFFSET(type, member, offset) static_assert(offsetof(RE::type, member) == offset)
AE_OFFSET(PlayerCharacter, questTargetLock, 0x630);
AE_OFFSET(PlayerCharacter, currentMapMarkers, 0x738);
AE_OFFSET(PlayerCharacter, objectives, 0x7D8); // Header's 7D9 comment is a typo; compiler layout is 7D8.
AE_OFFSET(PlayerCharacter, questTargets, 0x7F0);
AE_OFFSET(PlayerCharacter, cachedWorldspace, 0x8B0);
AE_OFFSET(PlayerCharacter, exteriorPosition, 0x8B8);
AE_OFFSET(PlayerCharacter, playerMapMarker, 0xBC4);
AE_OFFSET(TESWorldSpace, persistentCell, 0x70);
AE_OFFSET(TESObjectCELL, references, 0x70);
AE_OFFSET(TESObjectCELL, spinLock, 0xC0);
AE_OFFSET(TESQuest, currentInstanceID, 0x50);
AE_OFFSET(TESQuest, aliases, 0x70);
AE_OFFSET(TESQuest, aliasedLocMap, 0x88);
AE_OFFSET(TESQuest, aliasAccessLock, 0xD0);
AE_OFFSET(TESQuest, data, 0xF0);
AE_OFFSET(QUEST_DATA, flags, 4);
AE_OFFSET(QUEST_DATA, questType, 7);
AE_OFFSET(BGSBaseAlias, aliasName, 8);
AE_OFFSET(BGSBaseAlias, aliasID, 0x18);
AE_OFFSET(ExtraMapMarker, mapMarkerData, 0x18);
AE_OFFSET(TESFullName, fullName, 8);
AE_OFFSET(BSStringPool::Entry, _flags, 8);
AE_OFFSET(BSStringPool::Entry, _right, 0x10);
AE_OFFSET(BGSKeywordForm, keywords, 0x10);
AE_OFFSET(BGSKeywordForm, numKeywords, 0x18);
AE_OFFSET(BGSInstancedQuestObjective, objective, 0);
AE_OFFSET(BGSInstancedQuestObjective, instanceID, 8);
AE_OFFSET(BGSInstancedQuestObjective, enstanceState, 0xC);
#undef AE_OFFSET
static_assert(sizeof(RE::MapMarkerData) == 0x18);

// The pinned class omits a public: label. Explicit template instantiation may name private
// members (C++ [temp.explicit]); use native member pointers instead of guessing byte offsets.
struct MarkerFlagsMember { using Type = std::uint8_t RE::MapMarkerData::*; friend Type Member(MarkerFlagsMember); };
struct MarkerTypeMember { using Type = RE::MARKER_TYPE RE::MapMarkerData::*; friend Type Member(MarkerTypeMember); };
template<class Tag, typename Tag::Type member> struct NativeMarkerMember {
    friend typename Tag::Type Member(Tag) { return member; }
};
template struct NativeMarkerMember<MarkerFlagsMember, &RE::MapMarkerData::flags>;
template struct NativeMarkerMember<MarkerTypeMember, &RE::MapMarkerData::type>;
inline auto MarkerFlags(const void* data) { return static_cast<const RE::MapMarkerData*>(data)->*Member(MarkerFlagsMember{}); }
// Map marker type: uint16 at 0x12. The pinned header declares MARKER_TYPE as uint32, which the compiler places at
// 0x14 (its own comment says 12). OG game memory (engine probe, 2026-10-20) has the type at 0x12 and zeros at
// 0x14-0x17, e.g. 03 00 2D 00 00000000 = type 45, matching the Pip-Boy feed. Read the verified offset; the native
// member stays available for the probe so AE testers can confirm.
inline int MarkerType(const void* data) {
    std::uint16_t type = 0;
    std::memcpy(&type, static_cast<const char*>(data) + 0x12, sizeof(type));
    return type;
}
inline int NativeMarkerType(const void* data) { return static_cast<int>(static_cast<const RE::MapMarkerData*>(data)->*Member(MarkerTypeMember{})); }
inline std::string MarkerName(const void* data) {
    const auto& name = static_cast<const RE::MapMarkerData*>(data)->fullName;
    const RE::BSStringPool::Entry* entry = nullptr;
    static_assert(sizeof(name) == sizeof(entry));
    std::memcpy(&entry, &name, sizeof(entry));
    return PoolText(entry);
}
template<class T> bool ArrayReadable(const RE::BSTArray<T>& array) {
    return array.empty() || (array.size() <= 65536 && Readable(array.data(), array.size() * sizeof(T)));
}
}
