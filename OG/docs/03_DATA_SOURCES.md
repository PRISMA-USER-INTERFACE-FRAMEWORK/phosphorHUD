# 03 · Where each value comes from

How to read this: **✔** means confirmed from the mod's own files or the Creation Kit docs. **?** means it's the expected source but needs checking in game or in CommonLibF4 before relying on it. Every **?** is also listed in `06_OPEN_QUESTIONS.md`.

Function names are described, not quoted, unless they are certain. Look up the exact CommonLibF4 call when implementing.

Target: Fallout 4 OG 1.10.163 and NG 1.10.984 (same DLL; this load order runs Backported Archive2 Support and several OG-only plugins). PrismaUI's `SuppressHUDWidget` is verified only on OG 1.10.163 and AE 1.11.137+; on NG it is unverified and is logged as refused if it fails.

## Vanilla game data

| Message field | Source | |
|---|---|---|
| `hp`, `hpMax` | Player actor value `Health`: current, and permanent (max) value. | ✔ AV name |
| `ap`, `apMax` | Actor value `ActionPoints`. | ✔ AV name |
| `rads` | Actor value `Rads`, rounded/clamped to 0–1000. 0.9.10 logs Value and Permanent at baseline and exposure for comparison with the Pip-Boy. | ✔ AV name · ? weapon/attack values versus Pip-Boy |
| `radsGain` | Positive changes in successive slow-tick `rads` readings, batched at most once per second with the current absolute total. Baseline resets for each snapshot. | ✔ offline batching tests · ? live exposure |
| `limbs.head/torso/larm/rarm/lleg/rleg` | `PerceptionCondition`, `EnduranceCondition`, `LeftAttackCondition`, `RightAttackCondition`, `LeftMobilityCondition`, `RightMobilityCondition`, each ÷ 100. Crippled at 0. | ✔ AV names |
| `level` | Player level. | ✔ |
| `xp` | Player XP toward the next level ÷ the next-level threshold. Same numbers the vanilla `HUDExperienceMeter` shows. | ? exact fields |
| `xpGain`, `levelUp` | XP-gained and level-increase events (the same ones that drive the vanilla XP meter and level-up sound). | ? event names |
| `perkPoints` | Player's unspent perk count. | ? field |
| `heading` | Camera world rotation first column: atan2(entry[0].x, entry[1].x), converted to clockwise degrees from north. Falls back to player Z rotation when the horizontal camera direction is unavailable. | ? camera axis/sign in both perspectives |
| `location`, `region` | Current `BGSLocation` full name, falling back to the cell name; region = worldspace full name. Send on location-change events. | ? event name |
| `time.hour`, `time.timescale` | Globals `GameHour` and `TimeScale`. | ✔ globals |
| `combat` | Player combat state. | ? call |
| `weapon`, `clip`, `clipMax`, `reserve` | Equipped weapon instance (name includes mods), its current ammo count and magazine capacity, and the inventory count of its ammo type minus the loaded rounds. | ? calls |
| `explosive` | Item in the grenade equip slot plus its inventory count. Explosives Cycler equips into this same slot (✔ from its translation file: "equip the next one from the same list"). | ? slot API |
| `gear` | Equipped biped slots: 33 Body → clothed; 41 [A] Torso, 42 [A] L Arm, 43 [A] R Arm, 44 [A] L Leg, 45 [A] R Leg → armor pieces; 30 Hair Top (helmets/hats) → head armor. | ✔ 0.9.0 (also 36–40 under-armor → clothed, 46 → head) · ? modded armor |
| `target` | The actor the vanilla enemy health meter is showing (`HUDEnemyHealthMeter`'s target), its health fraction, level, and whether it's legendary. | ? how to read the meter's target |
| `stealth.sneaking` | Player sneaking flag. | ? call |
| `stealth.level` | The detection value behind vanilla's `HUDStealthMeter` (0–100, highest detection on the player). | ? field |
| `context.menus` | `MenuOpenCloseEvent` sink: keep a set of open menu names. | ✔ event |
| `context.powerArmor` | Player is in power armor (power armor furniture / keyword check). | ? call |
| `context.ads` | Player aiming down sights (iron sights, not scoped). | ? call |
| `context.hudHidden` | HUDMenu's `root` clip invisible or transparent (read through Scaleform; transitions logged). | ✔ 0.9.0 · ? `tm` |
| `context.prismaFocus` | PrismaUI V10 `IsAnyPanelVisible(phosphorView)`. | ✔ API |
| `prompt` | PrismaUI V9 `GetButtonPrompt("Activate", …)` for the key; the activate label from V8 `GetActivateChoiceLabel(0, …)` or the crosshair ref's activate text. | ✔ API · ? label source |
| `loot` | The container under the crosshair: name, inventory list, total weight. Same data vanilla's quick-loot (rollover) panel shows. | ? |
| `markers` | Quest targets for active quests (tracked → `active:true`), discovered map markers in range, enemies (vanilla compass rules), companions. A quest target that resolves to a load door in the current cell → `t:"door"`. | ? the cleanest read is the data the vanilla compass gets |
| `tracker` | Tracked quest (`BGSQuest` displayed objectives, with alias text substituted), distance to its first open target, other active quests. Active Quest HUD's F4SE plugin already does this read. | ? |
| `quest` banners | Quest start / objective complete / quest complete events. | ? event names |
| `notify` | Vanilla HUD notifications (item added, location discovered, etc.). Either intercept the vanilla notification call or build them from container-changed and location events. | ? |
| `message` | Vanilla help/tutorial messages. | ? |
| `subtitle` | Subtitle manager / dialogue line events: speaker name and text. | ? |
| `effect`, `effects` | Player's active effects from consumables (alchemy items): name, duration, elapsed. `kind` from item keywords (chem, food/drink, med). Addictions → `kind:"bad"`, `duration:0`. | ? keyword set |
| `party` | Player teammates: name, health fraction, bleedout state (`downed`), waiting state, actor value `CA_Affinity`. Liga of my Companions keeps up to 16 teammates; read them the same way. | ✔ CA_Affinity · ? waiting flag |
| `companionReact` | Watch each teammate's `CA_Affinity`: a rise → `liked` (large rise `loved`), a drop → `disliked`/`hated`. Or hook the affinity event that drives "X liked that". | ? event |
| `weight` | Actor value `CarryWeight` (max) and the player's current inventory weight. | ✔ AV · ? weight call |
| `legendaryLoot`, `weaponLegendary` | Weapon/armor instance mods carrying the legendary keyword (LEO adds extra legendary slots; read every legendary mod on the instance, not just the first). `dropped` from death of a legendary actor; `picked` from container-changed into the player. | ? keyword name |

## Survival

| Message field | Source | |
|---|---|---|
| `survival.on` | `Game.GetDifficulty() == 6` (6 = Survival with hardcore; 5 is the old, unused Survival). Unlimited Survival Mode doesn't change the difficulty. | ✔ |
| `needs`, `survival.stages` | Vanilla survival (`HC_ManagerScript`) keeps hunger, thirst and sleep as stages and applies one stage effect per need. Read which stage effect is active; send the stage and a matching fraction. When no effect names a stage (seen with unnamed light-plugin effects), the game's stage messages set it; since 0.9.23 those stages are kept per save in the F4SE co-save (record `SURV` under `PHUD`) and restored on load, so the HUD is right before the next message. | ? effect names |
| `survival.fatigue` | `ActorValue::fatigue` ÷ permanent Action Points, capped at 0.9 (0.8.0). | ✔ read · ? scale in game |
| `survival.adrenaline` | Rank = player's rank in Fallout4.esm perks named "Adrenaline"; kills = player kills (TESDeathEvent) since the rank last changed. Globals with "Adrenaline" in their editor ID are logged; one with "Kill" sets `perRank` (0.8.0). | ✔ read · ? kills per rank |
| `illness` | `HC_Disease*` effects on the player: which one, time left. | ? exact effect names |

## Mod data

| Mod | Message | Source | |
|---|---|---|---|
| KARMA (80425) | `karma.value`, `karma.alignment` | Globals `KARM_KarmaValueGlobal` and `KARM_PlayerAlignmentValue` (−2…2, breakpoints ±250/±750). | ✔ `karmaLib.psc` |
| KARMA | `karma.title` | The mod's title AV for the current alignment (`KARM_PBT_Karma*AV`, KARMA.esm 014–018) indexes its tab StringArray (Kindhearted … Devil). Karma itself is the player AV `KARM_KarmaAV` (KARMA.esm F99), alignment from ±250/±750. | ✔ 0.8.0 |
| KARMA | `karma.change` + `reason` | Diff of the karma AV; KARMA's own gain/loss HUD message is dropped from the feed. `reason` not read. | ✔ change · ✘ reason |
| REPUTATION (79394) | `reputation.*` | FormLists in REPUTATION.esm: 056 fame AVs, 057 infamy AVs, 001 tab AVs (value indexes the 19 titles of its tab StringArray; the AV's name is the faction's), 002 discovered globals. Events on fame/infamy change and discovery; the mod's own fame/infamy HUD messages are dropped from the feed. Hit squads/gifts not read. | ✔ 0.8.0 · ✘ hit squads, gifts |
| CHALLENGES (76347) | `challenges`, `challengeComplete` | Objectives of `CHAL_ChallengeDisplayQuest` (CHALLENGES.esm 067, "Tracked Challenges") from the Pip-Boy feed, `Name: text (cur/max)`; an unresolved `<Global=…>` is read by editor ID. 0.9.1 rejects missing globals, partial numbers and non-finite progress instead of fabricating zero or emitting invalid JSON. That quest never takes the quest tracker. Completion from the mod's "Challenge completed: X." HUD message. | ✔ 0.8.0 |
| LevelUpMenuEx (28822) | `perkKey` | MCM keybind `levelupmenuex_open` in mod `PRKF` (MCM stores bound keys in `Data\MCM\Settings\Keybinds.json`). | ✔ id |
| LevelUpMenuEx | `skillPoints`, `skillsPerLevel` | Skill points live inside PRKF's DLL (native `GetSP`); no skills are configured in this load order, so not sent. `perkKey` is read from `Data/MCM/Settings/Keybinds.json` (currently unbound). | ✔ perkKey · ✘ skills |
| Explosives Cycler | `explosive` | Same slot as vanilla (above). Set its "Show the selection" to Nothing to avoid a second readout. | ✔ |
| Condition Boy and Girl (56602) | `figure.type` when MCM says Auto | `iCharacterType` in its MCM settings: 0 Vault Boy, 1 Vault Girl. User value in `Data\MCM\Settings\ConditionBoy.ini`, default in `MCM\Config\ConditionBoy\settings.ini`. | ✔ |
| Companion Status UI Remastered (42925) | portraits | Already converted to `portraits/*.svg`; nothing to read at runtime. | ✔ |
| Immersive Legendary Mutations (66165) | `target.mutations`, `targetMutated` | Active spells on the target that come from ImmersiveLegendaryMutations.esp (`LegendaryPowerUps`: DR/ER/DT buff, shields, speed, self destruct, Firey, healing, Disguised Synth), mapped to names and kinds. `targetMutated` when one appears on the same target. | ✔ 0.8.0 |
| LEO (66900) | `weaponLegendary`, `legendaryLoot.effects` | Object mods on the item stack (BGSObjectInstanceExtra) whose attach point is `ap_Legendary` or LEO's `ap_Legendary_2…5`; name = the mod's name. Drops: legendary deaths (TESDeathEvent, name contains "Legendary") are watched for 8 s for such an item in the body; `picked` when it leaves the body. | ✔ 0.8.0 |
| SKK Settlement Attack System | `settlementAttack` | Any Pip-Boy objective "Defend <name>" where <name> is a workshop location (SKK_SAS's objective is `Defend <Alias=AttackLocation>`). Pending until the player is within 200 m with hostiles in combat; then active, total = most hostiles seen, remaining = hostiles now; completed objective = won; gone = ended. | ✔ 0.8.0 · ? faction |
| Vanilla attacks / Workshop Framework | `settlementAttack` | Same "Defend <settlement>" rule. `hoursLeft` not read. | ✔ 0.8.0 · ✘ hoursLeft |
| Mine Detector HUD (96532) | `mines` | Mine projectiles (`Projectile::IsMine`) tracked from TESCellAttachDetachEvent plus a 1 s rebuild from the player cell and attached exterior cells in the same worldspace (0.9.1); not ones the player laid. Shown when counting down (countdown sound handle or `explosionTimer` below the base fuse) or within max(6 m, 2× trigger radius); within 30 m, nearest 8. Screen position from `Main::WorldRootCamera()->worldToCam` mapped onto the letterboxed 1920×1080 stage; off screen → `bearing`. Vanished while counting → `mineExploded`. Raw fuse values logged within 12 m. | ✔ 0.9.0 · ? fuse reading |
| NAF / AAF | `context.scene` | Player has keyword NAF_InScene (NAF.esp 1ECF) or Bridge_ActorBusy (NAF Bridge's AAF.esm 915A). Transitions logged. | ✔ 0.9.0 |
| Liga of my Companions (25699) | `party` | Up to 16 teammates. Affinity = vanilla `CA_Affinity` (sent when non-zero; reactions from its changes: ±5 liked/disliked, ±30 loved/hated). Waiting: the `FollowerState` AV is logged per companion to work out its values. | ✔ affinity · ? waiting |

## 0.9.1 verification boundary

The latest available game log predates 0.9.0. Source review and native tests confirm parser validation,
title bounds, bounded event capture and its reset behavior. Engine object reads from the death/cell event
sinks are deferred to the game thread. CommonLib headers confirm the attached-cell state and cell reference
collections used for recovery; runtime coverage and scan cost still need testing. No fuse, marker-type,
height, follower-state, adrenaline or scene assumption was promoted to in-game confirmed.

## Hiding the vanilla HUD

Phosphor replaces the vanilla HUD, so the plugin has to hide what it replaces, and nothing else.

- PrismaUI V6 `SuppressHUDWidget(className, true)`. Class names confirmed in PrismaUI's DLL: `HUDPlayerHealthMeter`, `HUDActionPointMeter`, `HUDRadiationMeter`, `HUDExperienceMeter`, `HUDEnemyHealthMeter`, `HUDStealthMeter`, `HUDCriticalMeter`, `HUDFlashLightWidget`. Names for the compass, ammo counter, quest tracker, notifications, subtitles, crosshair and rollover are **?**.
- Fallback: HUDMenu's AS3 groups (`TopCenterGroup_mc`, `TopRightGroup_mc`, `CenterGroup_mc`, `LeftMeters_mc`, `BottomCenterGroup_mc`, `HUDNotificationsGroup_mc`) can be hidden through Scaleform. ✔ names (Creation Kit wiki); **?** which groups hold which widgets.
- **Compass (2026-10-18):** not suppressed. AAF draws its wizard on a Sprite named `AAF_Surface` that it adds to `root.BottomCenterGroup_mc.CompassWidget_mc.CompassBar_mc` (decompiled `AAF.swf`: `GameScreen.switchSurface` / `createTargetSurface`; it starts on `CenterGroup_mc.HUDCrosshair_mc.CrosshairBase_mc.CrosshairClips_mc` and moves to the compass once its script gets `COMPASS_VISIBLE`). Suppressing `HUDCompass` or shrinking `CompassWidget_mc` hid AAF's menu, confirmed in game. `Live::ShrinkVanillaCompass` keeps the clip visible and fades each child of `CompassWidget_mc` and `CompassBar_mc` (`CompassBGHolder_mc`, `OtherMask_mc`, `OtherMarkerHolder_mc`, `QuestMask_mc`, `QuestMarkerHolder_mc`, the bar graphic) except names starting `AAF_`. ✔ in game: vanilla compass hidden, AAF wizard opens on Home.
- Troubleshooting switches in `F4SE/Plugins/PhosphorHUD.ini` (not shipped in the file, all default 1): `bHideVanillaWidgets`, `bHideVanillaText`, `bHideVanillaCrosshair`, `bHideVanillaFlashlight`, and `iKeepVanillaWidgetMask` (bit i leaves `kReplacedWidgets[i]` visible, quest widgets from bit 11). `bHideVanillaMeters` stays the master switch.
- **Power armor:** stop suppressing while `powerArmor` is true, so the vanilla power armor HUD shows. Replacing the power armor HUD is deliberately out of scope.
- **Keep:** VATS, the Pip-Boy, scope overlays, the workshop menu, and the critical meter during VATS.
- **Turn off once Phosphor is live:** FallUI - HUD and its presets, Active Effects on HUD, Condition Boy's widget (Alpha 0 keeps its Auto setting readable), Companion Status (+ UI Remastered), Active Quest HUD, HoloTime, Mine Detector HUD. Keep Visible Companion Affinity, Explosives Cycler, KARMA, REPUTATION, CHALLENGES, LevelUpMenuEx: their data is what the plugin reads.

## Optional mods

| Message field | Source | |
|---|---|---|
| `armor.current`, `armor.max`, `armor.perPlate` | Globals `WZ_PlatesPlayer_Health` (Warzone esp 0x0F99), `WZ_PlatesPlayer_MaxHealth` (0x0F9A) and `WZ_PlatesPlayer_HealthToAdd` (0x0011), read as `TESGlobal` values. Read from the esp's own records (EDIDs and values parsed from the .esp). | ✔ EDIDs and default values · ? which global the mod's HUD reads as "current" (confirm in game) |
| `armor.stock` | Inventory count of the plate item `WZ_PlatesSystem_Plates` (MISC 0x0005). | ✔ record · ? count updates on insert |

## Quest markers, objectives and map locations (0.9.22)

Read from the engine by `src/EngineQuests.cpp` (OG; AE port pending), in the same shape as the Pip-Boy feed snapshot, so every consumer (compass, quest tracker, quest banners, CHALLENGES, settlement attacks) is unchanged. Layouts are mirrored in `src/EngineLayout.h` from libxse/commonlibf4 and were checked on OG 1.10.163 against the feed with the engine probe (2026-10-20).

- Objectives and quests: `PlayerCharacter::objectives` (state 1 shown, 3 completed, 5 failed). Tracked = `TESQuest::data.flags & 0x0800`. Miscellaneous (`questType` 6) objectives are grouped under form id 0 like the Pip-Boy.
- Compass targets: `PlayerCharacter::questTargets` (quest -> targets -> refs with a `TeleportPath`). With doors on the path the marker sits on the first door, which is in the player's current space (so indoors it points at the exit, like vanilla). `worldMapOnly` paths use `path.end`; otherwise the target ref's position. Only targets of shown open objectives, named after that objective.
- Text: `<Alias=…>`/`<Alias.X=…>` are filled from the quest's alias list (`aliasedLocMap` for locations, `GetAliasedRef` for references), `<Global=…>` by editor ID. `BGSQuestInstanceText::ParseString` is not used: called with a hand-built string it corrupted game memory on OG.
- Map locations: the current worldspace's persistent cell, refs with `ExtraMapMarker` (scanned once per worldspace). Name, type (`MapMarkerData` byte 0x12), discovered (flags 0x02) and flags 0x01 ("known" on the map; the feed's `Visible`) match the feed. Since 0.9.23 every enabled named marker is a compass candidate within `iLocationMarkerMeters`: a user log showed nearby undiscovered places without flag 0x01, so 0.9.22's flag-based undiscovered markers never appeared. Flag counts are logged once per worldspace. Settlements: marker name equals a location with keyword `LocTypeWorkshopSettlement`. No virtual calls on references (`GetEditorLocation` crashed OG).
- Custom waypoint: `PlayerCharacter::playerMapMarker`.
- The Pip-Boy feed is the fallback (`bEngineQuestData=0`, or the engine read fails). While the feed is connected the plugin logs up to 12 engine-vs-feed comparisons per session (`Engine vs feed`). `bEngineProbe=1` enables the Ctrl+F11 diagnostic dump.
