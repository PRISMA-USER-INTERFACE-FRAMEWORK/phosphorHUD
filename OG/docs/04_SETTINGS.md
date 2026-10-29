# 04 · Settings

Phosphor's MCM page is already built: `mod/Phosphor HUD/MCM/Config/PhosphorHUD/config.json`, defaults in `settings.ini` next to it.

## Reading them

1. Load defaults from `Data\MCM\Config\PhosphorHUD\settings.ini`.
2. Overlay the player's values from `Data\MCM\Settings\PhosphorHUD.ini` (MCM writes only changed values there; the file may not exist).
3. Re-read when `PauseMenu` closes (MCM lives inside it).
4. Send the result as one `settings` message after `OnDomReady`, and again after every re-read. Some settings are acted on by the plugin instead of (or as well as) the HUD; that's the last column.

## Map

OG 0.9.19 adds **Show crosshair** and **Show hit markers** under Look, plus a separate **Out of combat** section after Hide the HUD. All parts remain shown by default; existing users retain their prior appearance. The crosshair master switch overrides third-person aiming/context Show everything, without affecting interaction prompts. Hit markers stay independent and retain normal context rules. The native power-armor HUD is unchanged.

| New MCM id (`:Main`) | Values/default | `settings` field |
|---|---|---|
| `bShowCrosshair` | 0 Off, 1 On (default) | `showCrosshair` |
| `bShowHitMarkers` | 0 Off, 1 On (default) | `showHitMarkers` |
| `iCalmDelay` | 0–15 seconds, default 5 | `calmDelay` |
| `iCalmVitals` | 0 Show (default), 1 Dim, 2 Hide | `calmRules.vitals` |
| `iCalmCompass` | same | `calmRules.compass` |
| `iCalmWeapon` | same | `calmRules.kit` |
| `iCalmCompanions` | same | `calmRules.party` |
| `iCalmQuests` | same | `calmRules.tracker` |
| `iCalmEffects` | same | `calmRules.effects` |
| `iCalmCrosshair` | same; reticle only | `calmRules.cross` |

Calm starts after the chosen delay outside combat and ends immediately when combat resumes. All existing contexts, including ADS, scopes, VATS, menus and power armor, have higher priority. Existing awake states and relevant updates bring parts back for about five seconds; vitals remain shown below 35% HP. Firing/ammo changes wake weapon/crosshair and quest progress wakes the tracker. Show retains idle opacity; Dim uses the existing 0.3 context opacity. See the message contract for reset and update details. Event messages, hit markers, feed, banners, subtitles, settlement alerts, mines, enemy bar and stealth have no calm-hide setting.

| MCM id (`:Main`) | Values | `settings` field | Plugin also does |
|---|---|---|---|
| `bShowLevelBar` | 0 Hide, 1 Show (default) | `showLevelBar` (bool) | Hides the whole level/XP row, including its perk reminder and karma readout; does not stop XP tracking or level-up banners. |
| `bHideFullVitals` | 0 Off (default), 1 On | `hideFullVitals` (bool) | Independently hides recovered HP/AP and zero radiation. HP uses its radiation-reduced maximum; AP uses its fatigue-reduced maximum in survival. Comparisons use whole points to match native readings. Separate from the survival-needs toggle. |
| `bLocationOnArrival` | 0 Off (default), 1 On | `locationOnArrival` (bool) | Shows the large location name for 8 seconds on arrival or save load, then fades it out. Repeated names/settings do not restart the countdown. Pausing or context-hiding the location group suspends it; clock and region are unaffected. |
| `iTextReadability` | 0 Off (default), 1 Subtle, 2 Strong | `textReadability` ("off", "subtle", "strong") | Optional dark text shadows for brighter backgrounds. Off retains existing text styling; no changes to layout or hide rules. |
| `iClockFormat` | 0 12-hour AM/PM (default), 1 24-hour | `clock24` (bool) | Updates the clock format immediately when settings are applied. |
| `iLimbFigure` | 0 Auto, 1 Silhouette, 2 Vault Boy, 3 Vault Girl | `limbFigure`: `"silhouette"` `"vaultboy"` `"vaultgirl"` | Resolve Auto: Condition Boy's `iCharacterType` (0 → `vaultboy`, 1 → `vaultgirl`); without Condition Boy → `silhouette`. |
| `bHideHealthyFigure` | 0/1 | `hideHealthyFigure` (bool) | |
| `bShowArmorPieces` | 0/1 | `armorPieces` (bool) | |
| `iCtxAds` `iCtxScope` `iCtxVats` `iCtxDialogue` `iCtxWheel` `iCtxWorkshop` `iCtxPowerArmor` `iCtxScene` | 0 Recommended, 1 Show everything, 2 Hide everything | `contextRules`: `{ads, scope, vats, dialogue, wheel, workshop, powerarmor, scene}` each `"recommended"` `"show"` `"hide"` | |
| `iShowWeight` | 0 Near the limit, 1 Always, 2 Never | `showWeight`: `"near"` `"always"` `"never"` | |
| `bShowAdrenaline` | 0/1 | `showAdrenaline` | |
| `bNeedStageNames` | 0/1 | `needStageNames` | |
| `bHideSatisfiedNeeds` | 0 Off (default), 1 On | `hideSatisfiedNeeds` (bool) | Each need circle hides at stage 0 and appears from stage 1 (Peckish / Parched / Tired). Existing survival and context visibility rules still apply. |
| `bSurvivalEnemyMarkers` | 0/1 | `survivalEnemyMarkers` | |
| `bMineWarnings` | 0/1 | `mineWarnings` | When 0, skip the mine scan entirely (it's the most expensive read). |
| `bMineCompass` | 0/1 | `mineCompass` | |
| `bKarmaReadout` | 0/1 | `karmaReadout` | |
| `bLocalReputation` | 0/1 | `localReputation` | |
| `bChallengeTracker` | 0/1 | `challengeTracker` | |
| `iMutationNotices` | 0 Enemy bar only, 1 Enemy bar and feed, 2 Off | `mutationNotices`: `"bar"` `"feed"` `"off"` | When Off, don't send `target.mutations` or `targetMutated`. |
| `bLegendaryDropMarkers` | 0/1 | `legendaryDropMarkers` | |
| `bShowWeaponLegendary` | 0/1, default 1 | `showWeaponLegendary` | Off hides the equipped weapon's legendary-effects line above the ammo counter (0.9.19). |
| `iTextScale` | 80–150 (step 10), default 100 | `textScale` (÷100) | Look > Text size (%). All HUD text except the large numerals; ring captions cap at 130% (0.9.23). |
| `iIconScale` | 80–200 (step 10), default 100 | `iconScale` (÷100) | Look > Compass icon size (%). Compass markers grow upward from their baseline (0.9.23). |
| `iRingScale` | 80–150 (step 10), default 100 | `ringScale` (÷100) | Look > Ring size (%). Vital rings, armor wheel, effect rings and companion portraits; needs move to their own row when the vitals row would reach the compass (0.9.23). |
| `bShowUndiscovered` | 0/1, default 1 | `showUndiscovered` | Look > Show undiscovered locations on compass. On: nearby undiscovered places (the game's visible flag) show hollow and grey. Off: only discovered places (0.9.22). |
| `bQuickLootFirst` | 0/1, default 1 | (plugin only) | Quick loot > Quick loot before perk choices. With a perk-choices key bound, perk Activate choices on containers and dead bodies wait for the key, so the game shows the quick-loot box. Nothing changes while no key is bound (0.9.24). |
| `PerkChoicesKey` (MCM hotkey) | unbound by default | (plugin only) | Quick loot > Perk choices key (hold). Stored by MCM in `Data/MCM/Settings/Keybinds.json` (`modName` PhosphorHUD); the plugin reads it (`FindKeybind`) and polls the key while the game has focus. Dynamic Activation Key's global (`Dynamic Activation Key.esp` 0x10) also opens the gate (0.9.24). |
| `bShowArmorWheel` | 0/1, default 1 | `showArmorWheel` | Look > Show armor wheel. Off hides the Warzone Armor Plates wheel; the armor value keeps tracking (0.9.20). |
| `bLevelUpBanner` | 0/1 | `levelUpBanner` | |
| `bPerkReminder` | 0/1 | `perkReminder` | |

Full message with every default:

```json
{"settings":{"showLevelBar":true,"hideFullVitals":false,"locationOnArrival":false,"clock24":false,"limbFigure":"vaultboy","hideHealthyFigure":true,"armorPieces":true,
  "contextRules":{"ads":"recommended","scope":"recommended","vats":"recommended","dialogue":"recommended","wheel":"recommended","workshop":"recommended","powerarmor":"recommended","scene":"recommended"},
  "showWeight":"near","showAdrenaline":true,"needStageNames":true,"hideSatisfiedNeeds":false,"survivalEnemyMarkers":false,
  "mineWarnings":true,"mineCompass":true,"karmaReadout":true,"localReputation":true,"challengeTracker":true,
  "mutationNotices":"bar","legendaryDropMarkers":true,"levelUpBanner":true,"perkReminder":true}}
```

## Settings worth adding to the plugin side later

Not on the MCM page yet; add them when the plugin needs them:

- **Update rate** for the fast tick (heading, mines, enemy bar). 30 Hz is a reasonable default.
- **Debug logging** of every message sent, to replay with `tools/replay.py`.

## Look and effects (added 0.3.1)

| MCM id | Setting / plugin field | Values |
|---|---|---|
| `iIdleOpacity:Main` | `settings.idleOpacity` | 20–100 (%), default 85. Opacity of HUD parts that have been idle a few seconds; they light to 100% when something changes or in combat. |
| `iMaxEffectHours:Main` | plugin only | 0–48, default 2. Consumable effects longer than this stay off the effects tray (mods that use alchemy items for "permanent" statuses). 0 shows everything. |
