# 07 · Fallout 4 NG 1.10.984 support: what is verified

The OG DLL is compiled against the VR-configured CommonLibF4 tree (`commonlib/VR`, 696f5f5) and also loads on NG 1.10.984. Source key: GL = Garden CommonLib headers (flat tree `include/RE/IDs.h` and the `VR/` tree), NGBIN = the NG Address Library `version-1-10-984-0.bin` read directly, DEC = decompile of `Fallout4.exe` 1.10.984 through f4-re-mcp `learn_from_target` (source 5, used because GL and the RE Docs vault had no NG layout record for these members and the RE server was not reachable). The RE Docs vault was searched (OG/AE records only) and the RE server was not reachable; see the sources paragraph after the table.

CommonLib diff `696f5f5..57eb7a55`: only `.github/workflows/code-notes-guard.yml`, a code-notes markdown file and a comment-only change in `UI.h`. No ID, layout or API change, so the pin does not matter for NG.

## Member offsets on NG (finding A)

| Member | OG offset | NG 1.10.984 | Source |
|---|---|---|---|
| `MiddleHighProcessData::equippedItemsLock` | 0x280 | 0x280 verified | DEC: the getter at NG RVA 0x0CB76E0 returns `middleHigh + 0x280` |
| `MiddleHighProcessData::equippedItems` | 0x288 | 0x288 verified (data +0x288, size +0x298) | DEC: the consumer of that getter reads the array at +0x288 and its size at +0x298 |
| `PlayerCharacter::perkCount` | 0xCF1 | 0xCF1 verified | DEC: NG RVA 0x0CEFFB0 (id 2233187, the perk-count setter) writes `player + 0xCF1` |
| `PlayerCharacter::autoAimActor` | 0xC50 | 0xC50 PROVEN | DEC: the NG constructor (RVA 0x0CCC850, id 2232883, `rsi = this`) does `lea rdi,[rsi+0xC50]` (0x0CCD390), `lea rcx,[rip+..]` = 0x2369AC0 (0x0CCD39F), `mov [rdi],rcx`, `[rdi+8]`, `byte [rdi+0xC]=1`, `[rdi+0x10]=[rdi+0x14]=0` (0x0CCD485 to 0x0CCD492). 0x2369AC0 is the `BSTValueEventSource<PlayerAutoAimActorEvent>` vtable (NG RTTI id 2773493). Its slot 1 (RVA 0x0D0C240, id 2233712) calls the spin-lock at `this+0x10` (owner `+0x10`, count `+0x14`), reads the engaged flag at `this+0xC` and the handle at `this+8`. So `autoAim.dataLock` is player+0xC60. |
| `PlayerCharacter::playerMapMarker` | 0xBC4 | 0xBC4 PROVEN | DEC: the constructor writes `mov dword [rsi+0xBC4],edx` (0x0CCD434) between the 8-byte stores at +0xBBC and +0xBC8 (`playerMarkerPath`); NG RVA 0x0CDD620 (id 2233022) reads `[this+0xBC4]` as an `ObjectRefHandle` (`& 0x1FFFFF` index, handle-table check), releases it, and stores the invalid handle back. |
| `PlayerCharacter::questTargetLock` | 0x630 | 0x630 PROVEN | DEC: NG RVA 0x0CDB720 (id 2233002) takes the lock at `this+0x630` (`+0x634` is the count, decremented on exit) around a hash lookup on `this+0x7F0`; the constructor zeroes both words (0x0CCCB26) after the 8-byte `actorToDisplayOnHUDLock` at 0x628. |
| `PlayerCharacter::questTargets` | 0x7F0 | 0x7F0 PROVEN | same function; the constructor (RVA 0x0CCC850, id 2232883; the individual store address was not recorded) builds the `BSTHashMap` sentinel at +0x808 (0x7F0 + 0x18), and the next two hash maps at 0x820 and 0x850 follow the same shape. |
| `PlayerCharacter::objectives` | 0x7D8 | 0x7D8 PROVEN | DEC: NG RVA 0x0CDACE0 (id 2232997) reads the array size at `this+0x7E8` and data at `this+0x7D8`, stride 0x10 (`shl rsi,4`); the constructor initialises a `BSTArray` pair at 0x7D8. |
| `PlayerCharacter::currentMapMarkers` | 0x738 | 0x738 PROVEN | DEC: the constructor initialises six `BSTArray` blocks at 0x738, 0x750, 0x768, 0x780, 0x798, 0x7B0, the flat layout; the destructor (RVA 0x0CCDD60, id 2232884) frees the array at `[this+0x738]` with its size word at `+0x748`. |
| `PlayerCharacter::cachedWorldspace` 0x8B0, `exteriorPosition` 0x8B8 | as listed | 0x8B0 and 0x8B8 PROVEN | DEC: the constructor stores 0 to `[rsi+0x8B0]` right after the `NiTMap` vtable at +0x890 (0x0CCCE20) and three floats at 0x8B8 (0x0CCCE2F); the NG function at RVA 0x0CDD980 (id 2233028) writes `[this+0x8B0]` as a pointer and then 12 bytes from `[this+0x8B8]`, `+0x8BC`, `+0x8C0`. |

Guard added for the equipped-items reads: `EngineLayout::EquippedItemsPlausible` (`src/EngineLayout.h`). On OG it returns true. On NG (`REL::Module::IsNG()`) it returns false, and `ReadEquipped` and `ReadGear` then return empty without locking, when any of these fails:

- the lock and the array header at the flat positions are not readable (`Readable`)
- the array size is larger than its capacity
- the capacity is above 0x1000
- the size is non-zero and the `size * sizeof(EquippedItem)` bytes at `data` are not readable

It is read-only and never takes the lock before the checks pass. The locked reads of `autoAimActor` (`ReadTarget` in `Live.cpp`) and of `questTargetLock` with `questTargets` (`EngineQuests::Read`, `EngineProbe::Run`) are no longer skipped on NG: their offsets are PROVEN in the table above. A wrong offset there would have made `BSAutoLock` spin the game thread, which is why they were skipped until proven.

How the NG rows were proven: the `Fallout4.exe` 1.10.984 (md5 11d863fe378e3ea35b567f028c1e67d1, the file in `D:\FO4-NG-1.10.984`) is Steam-packed on disk. The disassembly came from its Steamless-unpacked copy (`Fallout4.984.exe.unpacked.exe`, `.text` bytes match the f4-re-mcp decompile at RVA 0x0CEFFB0). Function RVAs come from `version-1-10-984-0.bin` (pairs of id and RVA after one header qword). Source order: GL had one layout and no NG conditional; the RE Docs vault had only OG/AE records for these members (`questTargets` 0x7F0 with the lock at 0x630 on OG in `F4RR-co/RE-Work/Runtime/03-FINDINGS.md`) and no NG layout; the RE server was not reachable (`cloudflared access token` returned nothing without an interactive login); the NG decompile and disassembly are the source of every PROVEN row. The earlier `ApplyAutoAimActor` attempt missed the reference because the vtable is loaded only in the constructor (`lea rcx,[rip+0x169C71A]` at 0x0CCD39F), which the previous search did not cover.

Structural cross-check: the NG constructor (RVA 0x0CCC850) writes every `[this+disp]` with disp >= 0x600 that the OG constructor (RVA 0x0E97440, reached from the PlayerCharacter factory at 0x0D3E620 that allocates 0xE10 bytes) writes. The NG constructor adds stores for unrolled initialisers and orders some stores differently, which is compiler scheduling, not layout.

`playerMapMarker` 0xBC4 is a plain `ObjectRefHandle` read and was already ungated; it is now PROVEN as well. Every PlayerCharacter member listed in the table above is now PROVEN on NG 1.10.984. Still unproven on NG: virtual slots (see below) and any member not listed in this table.

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
