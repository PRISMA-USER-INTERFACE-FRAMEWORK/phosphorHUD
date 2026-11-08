# 07 · Fallout 4 NG 1.10.984 support: what is verified

The OG DLL is compiled against the VR-configured CommonLibF4 tree (`commonlib/VR`, 696f5f5) and also loads on NG 1.10.984. Source key: GL = Garden CommonLib headers (flat tree `include/RE/IDs.h` and the `VR/` tree), NGBIN = the NG Address Library `version-1-10-984-0.bin` read directly, DEC = decompile of `Fallout4.exe` 1.10.984 through f4-re-mcp `learn_from_target` (source 5, used because GL, the RE Docs vault and the RE server had no NG layout record for these members). The RE server (source 3) and the RE Docs vault were not queried for these facts.

CommonLib diff `696f5f5..57eb7a55`: only `.github/workflows/code-notes-guard.yml`, a code-notes markdown file and a comment-only change in `UI.h`. No ID, layout or API change, so the pin does not matter for NG.

## Member offsets on NG (finding A)

| Member | OG offset | NG 1.10.984 | Source |
|---|---|---|---|
| `MiddleHighProcessData::equippedItemsLock` | 0x280 | 0x280 verified | DEC: the getter at NG RVA 0x0CB76E0 returns `middleHigh + 0x280` |
| `MiddleHighProcessData::equippedItems` | 0x288 | 0x288 verified (data +0x288, size +0x298) | DEC: the consumer of that getter reads the array at +0x288 and its size at +0x298 |
| `PlayerCharacter::perkCount` | 0xCF1 | 0xCF1 verified | DEC: NG RVA 0x0CEFFB0 (id 2233187, the perk-count setter) writes `player + 0xCF1` |
| `PlayerCharacter::autoAimActor` | 0xC50 | UNVERIFIED, lock read skipped on NG | GL flat tree has one layout for OG and NG (`PlayerCharacter.h`, `sizeof == 0xE10`, no NG conditional), which is circumstantial only. No NG function touching +0xC50 was decompiled. |
| `PlayerCharacter::playerMapMarker` | 0xBC4 | UNVERIFIED | same as above |
| `PlayerCharacter::questTargetLock` 0x630, `questTargets` 0x7F0, `objectives` 0x7D8, `currentMapMarkers` 0x738, `cachedWorldspace` 0x8B0, `exteriorPosition` 0x8B8 | as listed | UNVERIFIED | read by `EngineQuests::Read` (runs on NG, `Plugin.cpp`) and the engine probe; not decompiled on NG |

Guard added for the equipped-items reads: `EngineLayout::EquippedItemsPlausible` (`src/EngineLayout.h`). On OG it returns true. On NG (`REL::Module::IsNG()`) it returns false, and `ReadEquipped` and `ReadGear` then return empty without locking, when any of these fails:

- the lock and the array header at the flat positions are not readable (`Readable`)
- the array size is larger than its capacity
- the capacity is above 0x1000
- the size is non-zero and the `size * sizeof(EquippedItem)` bytes at `data` are not readable

It is read-only and never takes the lock before the checks pass. The reads that take a lock on an UNVERIFIED NG offset are skipped on NG instead: a wrong offset would make `BSAutoLock` spin the game thread.

- `ReadTarget` (`Live.cpp`): the `autoAimActor` lock read is skipped, so the auto-aim target is empty and the existing fallbacks run.
- `EngineQuests::Read` (`EngineQuests.cpp`): the `questTargetLock` and `questTargets` copy is skipped, so the compass target list stays empty.
- `EngineProbe::Run` (`EngineProbe.cpp`): the same `questTargets` block is skipped.

Why skipped: one attempt to verify +0xC50 on NG found nothing. The NG vtable of `BSTValueEventSource<PlayerAutoAimActorEvent>` (RTTI id 2773493, COL found in the NG exe, vtable RVA 0x2369AC0) has no rip-relative reference in `.text`, and the one function named `ApplyAutoAimActor` (NG id 2233944) does not touch player+0xC50. Plain reads of the other unverified members are not gated: a wrong offset there reads garbage, guarded by `Readable`, and does not spin. `playerMapMarker` 0xBC4 is such a plain read and is UNVERIFIED on NG. Remove the skips once a NG decompile confirms +0xC50 and +0x630.

## Engine calls reachable on NG (finding B)

Single `REL::ID` resolves through `IDDB::id2offset` with `lower_bound` and no exact check on non-VR runtimes, so an ID that is absent from the NG bin silently resolves to the next higher entry. Only `RelocationID` and `VariantID` are NG-safe.

| Call or global (OG sources) | Header ID form | NG status |
|---|---|---|
| `RE::RTDynamicCast`, behind `RE::fallout_cast` (`Live.cpp` `ReadEquipped`, `Mines.cpp` `AsMine`) | single `REL::ID(84112)` in the `VR/` tree `RTTI.h`; the flat tree has `RTDynamicCast{84112, 2725673, 4818455}` | OG-only. Not exact in the NG bin. Fixed: `EngineLayout::NgSafeCast` uses `RelocationID(84112, 2725673)`; NG id 2725673 is RVA 0x2166BAE, `VCRUNTIME140.DLL::__RTDynamicCast` (DEC). |
| `RTTI` of the cast types (`EquippedWeaponData`, `EquippedItemData`, `Projectile`, `TESObjectREFR`) | `RTTI_IDs.h` RelocationID | NG-safe |
| `QuickLootFirst.cpp` `VTABLE::BGSEntryPointPerkEntry[0]`, `VTABLE::BGSEntryPointFunctionDataActivateChoice[0]` | single IDs in `VTABLE_IDs.h` | Both resolve exactly in the NGBIN and the COL type names match in the NG `Fallout4.exe`; no change |
| `GetSingleton` of `PlayerCharacter`, `UI`, `UIMessageQueue`, `ProcessLists`, `TESDataHandler`, `PlayerCamera`, `Main`, `Calendar`, `BGSSaveLoadManager`, `GameSettingCollection`, `INISettingCollection`, `INIPrefSettingCollection` | `RelocationID(f4, ng)` | NG-safe |
| `FavoritesManager` singleton (`Live.cpp`) | `RelocationID(198281, 2694399)`, matches flat `VariantID` | NG-safe (PR #7) |
| `GetWeightInContainer` (`Live.cpp`) | `RelocationID(1377567, 2201001)`, matches flat | NG-safe (PR #7) |
| `TESDeathEvent::GetEventSource` (`Mods.cpp`) | `RelocationID(1465690, 2201833)`, matches flat | NG-safe |
| `TESCellAttachDetachEvent::GetEventSource` (`Mines.cpp`, NG branch) | `RelocationID(311270, 2201823)`, matches flat | NG-safe (PR #7); `RegisterForCellAttachDetach` is only the non-NG branch |
| `TESHitEvent` source (`PluginEvents.cpp`) | `RelocationID(1411899, 2201886)`, matches flat | NG-safe (PR #7). The `VR/` tree `Events.h:747` pair for `TESHitEvent` is wrong and is not used. |

The scan behind this table intersected the 558 single-ID sites in the `VR/` tree with the call names in `OG/src`, then triaged each hit by hand. It does not follow calls made inside engine code or virtual slots. Virtual calls (`IsDead`, `GetHostileToActor` and similar) use vtable slots from the flat layout, which is one layout for OG and NG, and are UNVERIFIED on NG.

## AE copies

`AE/src` carries the same source as dead code under `#if !defined(PHOSPHOR_AE)` and `#else`. AE-compiled code is unchanged by every NG edit; the NG-only branches in `AE/src` are never compiled into the AE DLL.
`AE/SOURCE_MANIFEST.json` stores a sha256 of the LF working-tree bytes for the changed copies. The file mixes LF and CRLF hashes for other entries; hash `AE/src/Core.cpp` first to check the method.
`AE/ae/verify_og_source.py` fingerprints the OG-active tokens against `og-source-0.9.22.json`. PR #7 and this change both alter the OG-active tokens, so that check, if run, reports the changed files until the baseline is regenerated.
