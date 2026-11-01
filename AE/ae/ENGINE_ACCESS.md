# AE engine-access inventory

## 0.9.22-ae1 engine quest port (2026-10-20)

`EngineQuests::Read` and `EngineProbe::Run` now compile real implementations on AE. Both return before singleton access unless `bCompass && bEngineQuests`; Plugin::QuestData also checks those switches before its cache and otherwise returns the optional feed. `[Main] bEngineQuestData=0` selects feed-only operation; `[Main] bEngineProbe=0` remains the default and Ctrl+F11 dispatches on the game task. Engine target/door Z reads additionally honor `bMarkerHeights`. These switches do not disable unrelated base HUD readers.

`ae/EngineLayoutChecks.h` compiles mirror sizes AND member offsets against the pinned native types: TeleportLink, ParentSpaceNode, TeleportPath, TESQuestTarget::REF_DATA, TESQuestTarget, BGSQuestObjective. QuestTargetArray's array is its sole nonvirtual base at zero; the mirror checks base type/size, zero offset, nextRepath offset and total size. Native field offsets also checked:

| Native owner | Fields / compiled offsets |
|---|---|
| PlayerCharacter | questTargetLock 630; currentMapMarkers 738; objectives 7D8 (header's 7D9 comment is erroneous); questTargets 7F0; cachedWorldspace 8B0; exteriorPosition 8B8; playerMapMarker BC4 |
| TESWorldSpace / TESObjectCELL | persistentCell 70; references 70 and spinLock C0 |
| TESQuest | currentInstanceID 50; aliases 70; aliasedLocMap 88; aliasAccessLock D0; data F0; data.flags +4 and questType +7 |
| BGSBaseAlias | aliasName 08, aliasID 18 |
| ExtraMapMarker / TESFullName | mapMarkerData 18; fullName 08 |
| BGSInstancedQuestObjective | objective 00, instanceID 08, enstanceState 0C |
| BSStringPool::Entry | size 18, flags 08, shallow right pointer 10; native leaf/data semantics reviewed; AE walks at most 64 links and validates the full bounded string |
| BGSKeywordForm | keywords 10, numKeywords 18; ForEachKeyword is a nonvirtual inline array walk |

Offsets in this table are hexadecimal. Existing native fields (reference location/cell/extraList, form ID/flags, data-handler form arrays, global value) remain library-defined accesses. `IsDisabled` is an inline form flag check. `GetDisplayFullName` is a relocated nonvirtual function, used for alias/probe descriptions; no GetEditorLocation call or engine ParseString call was added. Static TESFullName::GetFullName may invoke the native full-name interface on forms; the map scan reads marker name storage directly and never calls virtual methods on references. AE `GetAliasedRef(aliasID)` returns a native handle by value; the OG out-parameter ABI remains isolated.

Alias lists/location entries and persistent-cell reference arrays are copied under locks; name/extra-data reads occur after release. The scan retains NiPointers until complete. Quest target map entries are copied under questTargetLock; subsequent pointed-to objects are readability checked, and AE target/door arrays are bounded/checked before iteration. These checks cannot eliminate lifetime races or validate engine call semantics; in-game evidence is still required.

**Map marker type (decided 2026-10-20, replacing the native-field choice):** MapMarkerData's pinned declaration comments the type at 0x12 but declares a uint32 enum, which MSVC lays out at 0x14. OG 1.10.163 memory holds a 16-bit type at 0x12 (for example `03 00 2D 00 00000000`: flags 0x03, type 45), and the OG engine-vs-feed comparison matched 240/240 locations using that reading. The AE build therefore reads uint16 at 0x12 (`EngineLayout::MarkerType`). The native member is still read, through typed member pointers (the upstream class accidentally has private fields; explicit template instantiation grants access without modifying the dependency), but only for the Ctrl+F11 diagnostic (`NativeMarkerType`), which logs both values plus raw bytes. Flags (0x10) use the same member-pointer mechanism and names use public TESFullName. Whether AE 1.11.240 keeps the OG layout is unverified; wrong map icons remain the specific open risk.

Address Library helper mappings checked against 1.11.240 (`ae/engine-addresses-0.9.22.json`, database SHA-256 65985cc2259384a13cffb538d74776e422e62b0cd3766485a000620242b72b06, 652,306 entries). Presence is not proof of ABI/runtime behavior. There are no missing IDs among these helpers:

| Native helper | ID | 1.11.240 RVA |
|---|---:|---|
| PlayerCharacter::Singleton | 4798212 | 0x31ede50 |
| TESDataHandler::Singleton | 4796135 | 0x30e7100 |
| TESQuest::GetAliasedRef | 2207810 | 0x671fa0 |
| TESObjectREFR::GetDisplayFullName | 2201126 | 0x513630 |
| BSSpinLock::lock | 2192245 | 0x2dd800 |
| BSSpinLock::BSReadWriteLock::lock_read | 2267897 | 0x1659400 |
| BSPointerHandle::BSPointerHandleManagerInterface::GetSmartPointer | 2188681 | 0x22d060 |
| TESForm::AllForms | 4796465 | 0x30ebec8 |
| TESForm::AllFormsMapLock | 4796476 | 0x30ebf18 |
| TESForm::AllFormsByEditorID | 4796466 | 0x30ebed0 |
| TESForm::AllFormsEditorIDMapLock | 4796477 | 0x30ebf20 |
| TESFullName::SparseFullNameMap | 2661402 | 0x2edcb00 |
| BSStringPool::GetEntry_char | 2268729 | 0x167dd20 |
| BSStringPool::Entry::Release | 2268720 | 0x167d4a0 |

TESFullName sparse map applies the library's -8 addend. Handle resolution, string interning/release, singleton and lock helpers are transitive reader dependencies. RTTI/cast and existing runtime infrastructure retain the earlier full library audit; no new manual numeric relocation was introduced.

## 0.9.21-ae1 review (2026-10-20)

ReadParty optionally builds ally markers using the same actor/player `data.location` fields, north-clockwise atan2 bearing, metres conversion and AddHeight helper already used by ReadEnemies. The existing bCompanions gate precedes player/process/actor access and all marker output. On AE, bMarkerHeights separately gates z/HeightThreshold/AddHeight. No new engine types, members or explicit Address Library IDs. Marker arrays are appended after ReadQuests replaces the list on initial and slow snapshots. Existing loaded high-process actor coverage and name-based companion identity remain; distant/unloaded companions and duplicate names are not newly solved.

The feed warning reads copied Pip-Boy snapshot state and steady-clock time, not an additional engine field. AE bPipboyFeed=0 suppresses it. It is once per process launch, after more than 20 seconds from HUD startup when disconnected. It does not track ever-connected state, so can also appear after a later disconnect. The AE companion service requires the separately supplied [Pip-Boy Companion App Enabler (AE)](https://www.nexusmods.com/fallout4/mods/108133); its 1.1.0 changelog lists 1.11.240. No service/engine patch is included here. Live feed protocol/coordinate and actor lifecycle behavior remain unverified for this candidate.

## 0.9.20-ae1 review (2026-10-20)

Independent source review against 0.9.18-ae1 found no new explicit Address Library IDs. New calls reuse the pinned library APIs below; this does not verify live relocation semantics. Claude's armor, compass and crosshair gates were retained. All experimental switches disabled from startup block these new accesses; existing base HUD engine reads and infrastructure still run.

| New access | Gate and library/source evidence | Remaining AE assumption |
|---|---|---|
| Armor form lookup, globals and inventory stock | Resolve, HasArmorPlates and ReadArmorPlates start behind `bModData`. Pinned `TESObjectMISC.h` defines kMISC and TESBoundObject base; `TESGlobal::GetValue` reads value at +0x30. `TESDataHandler::LookupForm<T>` resolves the plugin/local ID and validates type. | Warzone records and meanings remain inferred from OG. Three globals enable the wheel; missing plate item gives stock zero despite the older source comment saying all four are required. |
| Player armor-plate inventory count | `TESObjectREFR::inventoryList` +0xF8; `BGSInventoryList::data` +0x58, rwLock +0x78; item object +0x00, stackData +0x08; GetCount sums stack counts. Uses scoped BSAutoReadLock on the F4SE game-task path. | Same layout dependency as the existing ammo count and legendary loot scan; no new inventory layout dependency. New typed MISC lookup uses class/type identity, no MISC payload fields. Live list/stack lifetimes remain unverified. |
| Compass child alpha writes | Request masking and ShrinkVanillaCompass both require `bHideVanilla && bCompass`. Root `root.BottomCenterGroup_mc.CompassWidget_mc`; two-level traversal capped at 64 children per level, recursion only into `CompassBar_mc`; keeps direct `AAF_*` children at either traversed level. Uses existing UI task and strong menu/movie lifetime. | Clip names and hierarchy are OG observations. Missing root leaves compass visible; renamed CompassBar can be faded as an ordinary child, including AAF descendants. Engine calls themselves are not bounded by the child-count limit. |
| Reticle/dot visibility | ReadCrosshair's `bCrosshair` gate covers HUDCrosshair_mc.visible, CrosshairTicks_mc visibility and CrosshairClips_mc.Dot_Dot/Standard_Dot. | Missing ticks return null; hidden valid ticks plus missing dot clips produce reticle=false/dot=false. This does not guarantee a visible fallback. |
| Widget suppression | `bHideVanilla` remains master, existing per-widget gates remain; HUDCompass is always excluded from Prisma suppression. Power armor requests disable suppression. | Prisma behavior and live power armor transitions unverified. |

The AE-only CaptureNativeHud early-exit correction permits an incoming hideCompass request even when all three native readers are disabled. Previously that combination returned before applying compass hiding. The actual-code probe tests 2,048 switch/request/restore combinations per build mode, including all-off. It counts entry to UI access, not real GFx behavior. OG-active tokens are unchanged by this correction. Existing camera isolation remains in place.

Warzone local records: `Warzone - Armor Plates System.esp`, globals current `0xF99`, maximum `0xF9A`, per plate `0x011`; MISC plate item `0x005`. All optional and resolved at data-ready. No new power armor operation was added.

## Earlier inventory

2026-10-16; baseline Phosphor 0.9.17. This is a source audit, not AE runtime acceptance.

0.9.18-ae1 follow-up (2026-10-17): `Live::ReadSlow` now reads `PlayerCamera::currentState->id` to send `context.thirdPerson`. The pinned AE library defines currentState at TESCamera +0x28, TESCameraState::id at +0x20, and k3rdPerson=8; these are library-verified and reuse the camera classification already used by Heading. Runtime behavior with custom cameras remains unverified. `Pipboy::Extract` copies Map.World.Custom and Map.Local.Custom Visible/X/Y into the snapshot; `ReadQuests` converts the selected point into the existing pin marker contract. These field names were observed in an OG exterior capture only. AE feed semantics and interior/door routing remain unverified. Existing bPipboyFeed/bCompass switches still gate the feed/marker path; no new native Flash operation is introduced.

Candidate: libxse/commonlibf4 `7c8c6f8a349ac85abf6fc8a7e5a71732d9339954`, commonlib-shared `29fbdb0e2dc548c9ab22f6964981d75090dc9094`. Separate AE binary, initially limited to 1.11.240. Do not use the OG library's IDs on AE.

`library-verified` means a definition/relocation exists in that pinned AE library; it does **not** mean tested in Fallout. `assumed` means game content or semantics inferred from OG. `unknown` needs AE files/logs. Line numbers below refer to the unmodified 0.9.17 baseline saved in `.build/ae-feasibility/before/src/`.

## Engine boundaries

OG resolution throughout is the existing CommonLibF4-NG headers plus the matching OG Address Library, except the explicit hit-event override. Pointer fields, virtual calls and container iteration depend on layouts, not just Address Library offsets.

| File, function, baseline line | Reads/calls; OG resolution | AE source status and remaining risk |
|---|---|---|
| PluginEvents.cpp, HitEventSource, 11 | Manual `REL::ID(1411899)`; known OG event-source getter | library-verified replacement: `RE::TESHitEvent::GetEventSource`, AE ID **2201886** in `RE/IDs.h`. Never reuse 1411899 on AE. |
| PluginEvents.cpp, HitWatch::ProcessEvent, 22 | Event cause/target NiPointers; player singleton, As<Actor>, form IDs | library-verified `TESHitEvent`, `HitData`, reference/actor layouts; event lifetime/thread behavior still needs testing. |
| PluginView.cpp, HandleMessage, 230 | F4SE messages; UI/menu and hit sink registration; data-ready callbacks | library-verified F4SE Interfaces, UI, BSTEvent. AE uses exported version data and Load, not Query alone. |
| PluginView.cpp, MenuWatch::ProcessEvent, 204 | Menu name/opening, generation/queue state | library-verified MenuOpenCloseEvent; same menu names assumed. |
| PluginView.cpp, Queue, 27; CreateView, 138; DomReady, 122; HandleMessage, 230 | Prisma V12 callbacks, thread dispatch, view and widget APIs | library-verified public ABI: header identical after comment/whitespace normalization; V12=139. Live AE callbacks unverified. |
| Plugin.cpp, KeyFor, 30; PluginView.cpp, SetWidgets, 52 | Prisma button prompts and vanilla widget suppression | ABI verified; widget class names and suppression semantics assumed on AE; no suppression in power armor. |
| PluginView.cpp, BlockingMenu, 160; PluginTest.cpp, AutoTestTick, 19 | UI singleton and GetMenuOpen; autotest menu messages | library-verified UI/UIMessageQueue definitions; menu strings assumed. |
| PluginTest.cpp, AutoTestQueueLoad, 11 | BGSSaveLoadManager singleton and queued latest-save load | library-verified manager/queue enum. Autotest defaults disabled; not an AE acceptance result. |
| Live.cpp, Name/Value/Permanent, 36–40 | TESFullName and actor-value virtual getters | library-verified TESFullName/ActorValueOwner; actual values require console comparison. |
| Live.cpp, ReadEquipped, 62 | currentProcess->middleHigh; equipment lock/list; item, equipIndex, weapon instance, ammo count/capacity/type | library-verified Actor, AIProcess, MiddleHighProcessData, EquippedItem, EquippedWeaponData, TESObjectWEAP. Header probe verifies currentProcess 0x300 and equippedItems 0x288. |
| Live.cpp, ReadInventory, 96 | Inventory rwLock/data/stack chain, counts, display names, extra-data mods | library-verified BGSInventoryList/BGSInventoryItem/extra-data definitions. Bad lists or pointer lifetimes can still fault/loop. |
| Live.cpp, Heading, 169; ReadFast, 200 | Player position/angles, PlayerCamera/currentState, Main::WorldRootCamera worldToCam | library-verified OBJ_REFR, TESCamera/PlayerCamera, Main, NiCamera; camera coordinate conventions assumed. |
| Live.cpp, ReadSlow, 211 | Player/AV singletons; HP/AP/rads/limbs/experience/perks/level; location, cell/worldspace/parent location; calendar; combat, gun state, power armor, UI | library-verified field definitions and getters. Parent-location traversal can loop on malformed cyclic data. Content names/menu names and value semantics assumed. |
| Live.cpp, ReadEffects, 338; ReadIllnesses, 737; ReadSurvival, 807 | ActiveEffectList entries, flags, spell/type/keywords/effect names, durations; difficulty/fatigue | library-verified ActiveEffect/List/MagicTarget/PlayerCharacter/ActorValue and spell/effect layouts. Need names/descriptions, localization and recovery messages are assumed. |
| Live.cpp, HeightThreshold, 397; LogCompassSettings, 662; LogXpSettings, 1272 | GMST collection/settings/type/value | library-verified collection; `fQuestMarkerZDiffThreshold`, perception/XP setting names assumed. Missing values use existing fallbacks. |
| Live.cpp, MapMarkerZ, 413; ResolveMarkerHeights, 433 | GetFormByID<REFR>; player cell, TESDataHandler cell arrays, attached state/worldspace, references under spinLock | library-verified TESForm global map, TESDataHandler, TESObjectCELL and REF layouts; header probe verifies references 0x70 and spinLock 0xC0. Lock/list faults can hang/crash. |
| Live.cpp, ReadQuests, 479 | Player coordinates/cell/worldspace; Pipboy marker coordinates/IDs; height resolvers | engine fields library-verified; Pip-Boy data semantics unknown on AE. Coordinate matching and 70 units/m assumed. |
| Live.cpp, ReadLoot, 590 | FavoritesManager quickContainerMode, dataLock, optional event data, container handle, items/buttons/mode/selection | library-verified FavoritesManager/QuickContainerStateData; whether stale state clears identically on AE is unverified. |
| Live.cpp, ReadEnemies, 685 | ProcessLists high/middle-high handles, combat/dead/disabled state, positions/currentCombatTarget, perception | library-verified ProcessLists/Actor. Probe verifies high 0x40/middle-high 0x70. Hostility and range semantics assumed. |
| Live.cpp, ReadParty, 763 | High-process actors, companion membership, display name/HP/life state/affinity | library-verified Actor/life enum; API enum naming differs. Optional-mod semantics assumed. |
| Live.cpp, ReadGear, 891; ReadWeight, 910 | Inventory equipped stacks, armor biped slots; carry actor values | library-verified layouts. New library uses bipedModelData.bipedObjectSlots instead of old GetFilledSlots helper. Slot interpretation assumed. |
| Live.cpp, TargetJson, 918; ReadTarget, 930 | autoAimActor event/lock/handle; last-hit form, hostility, combat, name/HP/level/mutations | library-verified PlayerCharacter autoAimActor (probe 0xC50), Actor and TESForm APIs. Live target lifetimes unverified. |
| Live.cpp, ReadStealth, 953 | IsSneaking and RequestHighestDetectionLevelAgainstActor | library-verified AE relocations; no sneak damage multiplier perk query. Actual detection must be checked in game. |
| Live.cpp, native HUD helpers, 967–1269 | UI task scheduling, strong menu/movie pointers and GFx object operations | library-verified native types/relocations; **Flash paths/getters unknown on AE**, detailed below. |
| Mines.cpp, AsMine, 30; AttachWatch, 36; RegisterSinks, 128 | Runtime cast Projectile/IsMine; cell attach event/form ID | library-verified Projectile, RTTI and TESCellAttachDetachEvent; callback payload/thread/lifetime unverified. |
| Mines.cpp, ScanAttachedCells, 70; DrainAttachEvents, 56 | Cell spinLock/reference lists; attached neighbor cells from TESDataHandler; form lookup | library-verified layouts; lock/iterator safety is high risk if wrong. |
| Mines.cpp, Project, 109; Read, 143 | Camera matrix/port; player/projectile positions, shooter, weaponSource, countdown sound, explosion timer/proximity, flags/age | library-verified Projectile/BGSProjectile/NiCamera fields; fuse and projection semantics assumed. WindowSize at 100 reads Win32 client size, not engine memory. |
| Mods.cpp, Lookup/Plugin/AvByEditorId/ListItems, 47–73; Resolve, 169 | TESDataHandler plugin/form arrays, TESForm/AV/global/list/quest records, editor IDs, faction/keyword/perk discovery | library-verified lookup/layout APIs; individual mod records assumed until checked against each AE mod release. |
| Mods.cpp, DeathWatch, 121; DrainDeaths, 133; RegisterSinks, 294 | TESDeathEvent actor handles, player, form IDs, optional death/corpse processing | library-verified death event and Actor; event correctness unverified. |
| Mods.cpp, IsCompanion/Affinity/LogFollowerState, 304–328 | Faction membership, commanded flag, affinity/follower AV | library-verified actor/faction/value APIs; record semantics assumed. |
| Mods.cpp, LegendaryEffects, 330; Mutations, 357 | ObjectInstanceExtra index data, attached mods/typed keywords, spell active effects | library-verified extra-data/attachment/keyword/effect definitions; optional mod names/slots assumed. |
| Mods.cpp, ReadKarma/RepStatus/LocalFaction/ReputationEvents, 391–470 | Player AVs; quest data flags; globals; parent location chain; reputation forms | library-verified native definitions; optional record semantics and quest flag bit assumed; malformed parent chains can loop. |
| Mods.cpp, ReadChallenges, 474; ReadAttack, 502 | Parsed Pipboy quests/objectives, player coordinates, marker bearings | Native position library-verified; protocol/objective wording unknown/assumed. |
| Mods.cpp, ReadAdrenaline, 565; LegendaryLootEvents, 586 | Player perk ranks, death/corpse actor inventory/stack/extra-data, item counts/names, dropped positions | library-verified APIs/layouts; optional behavior assumed; inventory traversal/lifetime high risk. |
| Mods.cpp, PlayerInScene, 659; PerkMenuKey, 669 | NAF/AAF keywords on player; MCM keybind JSON on disk | Native keyword API library-verified; specific mod records assumed; keybind file has no engine layout. |
| Pipboy.cpp, RunFeed, 229; Extract, 146; Run, 309 | localhost TCP 27000, message framing, binary value database, quests/locations/marker names and positions | **unknown** NG/AE wire compatibility; no engine memory access, no AE capture available. A connection alone cannot establish field/coordinate correctness. |

## Flash paths and failure behavior

The native library provides `Scaleform::GFx` (global namespace on libxse); this is an API spelling change. It does not validate HUDMenu.swf content.

| Baseline location | Path / property | AE status; failure behavior |
|---|---|---|
| Live.cpp HudHidden 1020 | `root`, `visible`, `alpha` | unknown; absent values fail soft, but native table lookup itself can fault/hang. |
| ReadHudText 1032 | `root.BottomCenterGroup_mc.SubtitleText_mc`, SubtitleText_tf, SpeakerName_tf | unknown; missing clip/text becomes empty. |
| ReadHudText 1032 | `root.HUDNotificationsGroup_mc.TutorialText_mc` / `Messages_mc` (1012–1015); TutorialText_tf, getChildAt, numChildren | unknown; display-list walks limited to 16 children/depth, but that does not bound engine calls. |
| ReadHudText 1032 | RolloverWidget_mc, RolloverName_tf, ButtonHintBar_mc, textField_tf; ButtonVisible, ButtonText, PCKey, ControllerButton/XenonButton; ActivateButton/PlayHolotapeButton | unknown. ButtonVisible must succeed and be true before unsafe data getters. Missing members fail soft; getters execute ActionScript and may fault. Prior OG hang was inside native lookup, not our bounded traversal. |
| ReadCrosshair 1142 | root.CenterGroup_mc.HUDCrosshair_mc.CrosshairBase_mc.CrosshairTicks_mc; Left/Right/Up/Down x/y; parent scaleX | unknown; missing ticks return null. Native object table lookup may still fault/hang. 1280×720 Flash coordinate assumption needs AE check. |
| ReadFlashlight 1177 | root.RightMeters_mc.FlashLightWidget_mc visible/alpha | unknown; absent clip returns false. |
| ShrinkVanillaText 1122, ShrinkVanillaFlashlight 1184, HideVanillaCrosshair 1195 | scaleX/scaleY/alpha writes | unknown; only hide owned categories and restore them on the UI queue; power armor remains excluded. |

HUDFramework or another HUDMenu.swf replacer can change these independently of game runtime. Null checks and C++ exception handlers do not make invalid native memory safe. No live AE validation is claimed.

## Optional records

Resolve by plugin plus local FormID. The lookup mechanism is version independent in principle; each native record layout still depends on the library, and mod authors can change records.

- Fallout4.esm: CurrentCompanionFaction `023C01`; AVs discovered by editor ID `CA_Affinity`, `FollowerState`; Adrenaline perks by name and globals by editor-ID substring.
- Liga.esp: team `0B80A1`, home `010317`.
- KARMA.esm: value AV `000F99`; title AVs `000015`, `000016`, `000014`, `000017`, `000018`.
- REPUTATION.esm: fame/infamy/title/shown lists `000056`, `000057`, `000001`, `000002`; location lists `000147`. Hit/gift quests: `359,35D,360,45B,469,472,478,482,48A,48B,48C,48D,495,49D,4A7,6B2,6B3,6B4,6B5,6B6,6B7,6B8,6B9,87E,883,886,889,88C,88F` (hex local IDs). Nested list contents are resolved dynamically.
- CHALLENGES.esm: tracked display quest `000067`.
- NAF.esp: scene keyword `001ECF`; AAF.esm/NAF Bridge keyword `00915A`.
- ImmersiveLegendaryMutations.esp: detect originating plugin of active spells; names classify mutation types, no fixed local form IDs.
- LEO/vanilla: dynamic legendary attachment keywords `ap_Legendary`, `ap_Legendary_2` through `_5`; no fixed local IDs.
- SKK/vanilla settlement attacks: Pipboy objective text beginning `Defend`, not a fixed SKK form.
- LevelUpMenuEx: MCM Keybinds.json; ConditionBoy character selection is an INI read in Core.cpp. Missing configuration/art must remain optional.
- Explosives Cycler: no direct engine integration or dedicated toggle in this source; prior display-conflict reproduction is separate from AE compatibility.

## Phase status

Inventory identifies the source adapters and unverified live boundaries. Flash content, protocol and optional records require tester evidence; they are not promoted to library-verified. Full AE behavior remains unverified.
