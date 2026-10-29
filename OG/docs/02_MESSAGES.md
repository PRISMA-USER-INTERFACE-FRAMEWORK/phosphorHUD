# 02 · Message spec

Everything the plugin tells the HUD goes through one JavaScript function on the page:

```cpp
prisma->InteropCall(view, "hudUpdate", jsonText);   // jsonText is a JSON object, as a string
```

`hudUpdate` accepts any combination of the fields below in one object. Every field is optional, so send only what changed, and batch everything for one tick into a single call. Unknown fields are ignored.

Conventions used throughout:

| Term | Meaning |
|---|---|
| fraction | a number from 0 to 1 |
| bearing (`b`) | compass degrees, 0 = north, 90 = east, measured from the player to the target. Any value works; the HUD wraps it. |
| `dist` | metres. Game units ÷ 70 is close enough (1 unit ≈ 1.43 cm). |
| `dz` | height of the target minus the player, metres. Above ±8 m shows ▲ or ▼. |
| `id` | any stable string. FormIDs as hex (`"ff012a3b"`) are the obvious choice. |
| icon names | see [Icon names](#icon-names) at the end |

Every example below is also in `examples/messages.jsonl`, and `tools/replay.py` checks them against the HUD.

---

## Lifecycle

| Field | Type | What it does |
|---|---|---|
| `snapshot` | `true` | Marks a full state message. In game the HUD stays invisible until the first message with `snapshot:true` (or any message with `hp`), so preview sample values never show. |
| `reset` | `true` | Clears everything that belonged to the previous moment: feed, banners and their queue, enemy bar, loot panel, mines, settlement attack, legendary drop markers, stealth. Send it after a save loads, before the new snapshot. |
| `settings` | object | Phosphor's MCM values. See `04_SETTINGS.md`. Send once after `OnDomReady` and again whenever the pause menu (which hosts MCM) closes. |
| `paused` | bool | Freezes every effect timer and the clock. Send `true` while any `menu` situation is open (see `context`), `false` after. |

```json
{"reset":true}
{"snapshot":true,"hp":212,"hpMax":310,"ap":64,"apMax":90,"rads":140,"level":23,"xp":0.72,"heading":27,"location":"Sanctuary Hills","region":"Commonwealth"}
```

## Text readability

`settings.textReadability` accepts `"off"` (default), `"subtle"` or `"strong"`. Subtle and Strong add dark text shadows for brighter backgrounds, including interaction keys and subtitles. Off preserves the original styling. This changes text rendering only; existing layout, visibility rules and animations continue. Partial settings updates and save resets retain the selection until a new value is sent.

```json
{"settings":{"textReadability":"subtle"}}
{"settings":{"textReadability":"strong"}}
{"settings":{"textReadability":"off"}}
```

## Vitals and leveling

`settings.hideFullVitals` (boolean, default `false`) independently hides the HP wheel when healed to its radiation-reduced maximum, the AP wheel when recovered to its fatigue-reduced maximum (survival only), and the radiation wheel when rads are zero. HP/AP comparisons use rounded whole points, matching native readings; zero HP/AP remain visible. The native fatigue fraction retains its precision so rounding it to a percentage does not shift the recoverable AP maximum. This setting is independent of `hideSatisfiedNeeds`, takes effect immediately, and respects existing context hiding. When all three vitals and carry weight are hidden, the needs row has no leading separator.

```json
{"settings":{"hideFullVitals":true},"hp":100,"hpMax":100,"ap":100,"apMax":100,"rads":0,"survival":{"on":false}}
{"hp":75,"ap":50,"rads":100}
{"hp":90,"ap":100}
{"settings":{"hideFullVitals":false}}
```

`settings.showLevelBar` (boolean, default `true`) controls the player level/XP row, including its perk reminder and karma readout. XP tracking and level-up banners continue when it is hidden. `settings.clock24` (boolean, default `false`) switches the clock immediately between 12-hour AM/PM and 24-hour time; subsequent hour updates retain the choice unless `time.h24` explicitly overrides it.

```json
{"settings":{"showLevelBar":false,"clock24":true}}
{"settings":{"showLevelBar":true,"clock24":false}}
```

| Field | Type | Notes |
|---|---|---|
| `hp`, `hpMax` | number | Current and maximum health. |
| `ap`, `apMax` | number | Current and maximum action points. |
| `rads` | number | Radiation, 0–1000. The HUD bites `rads/1000` off the end of the HP ring, like the game. Only the RAD ring has a minimum visible 3% arc when radiation is positive; its number stays exact. |
| `radsGain` | number | Floats "+N RADS" over the RAD core. Adds to `rads` only if the message has no absolute `rads` value. The live plugin batches positive changes at most once per second and always includes the current absolute `rads`, even if already sent. Loading a save establishes a baseline without a popup. |
| `level` | integer | Player level. |
| `xp` | fraction | Progress to the next level. |
| `xpGain` | number | Floats "+N XP". If `xp`/`level` come in the same message they are taken as the exact values after the gain; a level change then plays the bar rollover. |
| `levelUp` | `{from, to, perks?, skills?}` | Plays the level-up ring and banner. `perks` defaults to `to - from`. |
| `perkPoints` | integer | Unspent perk points (reminder chip next to the level). |
| `skillPoints` | integer | Unspent LevelUpMenuEx skill points. |
| `skillsPerLevel` | integer | LevelUpMenuEx skill points per level, 0 when its skills are off. |
| `perkKey` | string | Key shown on the banner for opening the perk menu (LevelUpMenuEx's hotkey). Empty hides the hint. |

```json
{"xpGain":150,"xp":0.31,"level":24}
{"levelUp":{"from":23,"to":24,"perks":1,"skills":15},"perkPoints":1,"skillPoints":15}
{"perkKey":"K","skillsPerLevel":15}
{"rads":9,"radsGain":5}
{"radsGain":1}
```

## Location, clock, combat

`settings.locationOnArrival` (boolean, default `false`) shows the large location name for 8 seconds of unpaused time while its HUD group is not hidden, then fades it out. A different name or the first location after `reset` starts a fresh countdown; repeated names or unchanged settings do not restart it. An empty location clears the name and allows re-entry into the same named place to count as an arrival. Enabling this setting briefly shows the current known location; disabling it restores the persistent name. The region, clock and reputation display retain their existing behavior. Edge flapping is ignored: moving from a named place into the bare region (a `location` equal to the current `region`, e.g. "Military Checkpoint" -> "Commonwealth") is not an arrival, and returning from the region to the place just left within 60 seconds is not one either. Only arrivals wake the vitals group. The first location after a load or `reset` is always an arrival, even when it is the region.

```json
{"settings":{"locationOnArrival":true},"location":"Sanctuary Hills","region":"Commonwealth"}
{"location":"Red Rocket"}
{"settings":{"locationOnArrival":false}}
```

| Field | Type | Notes |
|---|---|---|
| `location` | string | Big location name. |
| `region` | string | Small line above it (worldspace or region). |
| `faction` | string \| null | REPUTATION faction that owns this place; shows your standing next to the region. Send with `location`. |
| `time` | `{hour, timescale?, h24?}` | `hour` is GameHour (0–24, fractional). The HUD advances the clock itself at `timescale`; resend after waits, sleeps, fast travel and loads. |
| `combat` | bool | Player in combat. Keeps the HUD fully lit and dims the quest tracker. |

```json
{"location":"Diamond City","region":"Commonwealth","faction":"Diamond City"}
{"time":{"hour":21.7,"timescale":20,"h24":false}}
{"combat":true}
```

## Survival needs and weight

`settings.hideSatisfiedNeeds` (boolean, default `false`) hides each satisfied food, water and rest circle independently. When enabled, a circle appears from stage 1 (Peckish, Parched or Tired), remains visible at worse stages, and hides again at stage 0. The HUD uses the reported stages, or its existing fraction-to-stage fallback when stages are absent. Settings changes take effect immediately; survival mode and context hide rules still apply. The separator disappears when all three circles are hidden.

```json
{"settings":{"hideSatisfiedNeeds":true},"survival":{"on":true,"stages":{"food":0,"water":0,"rest":0}}}
{"survival":{"stages":{"food":1,"water":0,"rest":1}}}
{"survival":{"stages":{"food":0,"water":0,"rest":0}}}
{"settings":{"hideSatisfiedNeeds":false}}
```

| Field | Type | Notes |
|---|---|---|
| `needs` | `{food?, water?, rest?}` fractions | 1 = fully satisfied. Drives the small food, water and rest cores. |
| `survival` | object | `on` (bool, survival difficulty), `fatigue` (fraction of max AP lost, drawn as a red bite on the AP ring), `stages` (`{food,water,rest}` each 0–5, optional, overrides the stage picked from `needs`), `adrenaline` (`{rank 0–10, kills, perRank, bonus?}`), plus the display flags `stageNames`, `showAdrenaline`, `enemyMarkers` (normally sent through `settings`). |
| `weight` | `{cur, max, show?}` | Carried weight and carry limit. `show`: `"near"` (from 85 %), `"always"`, `"never"`. Over the limit turns it red and posts "Overencumbered" once. |
| `illness` | `{id, hoursLeft?, quiet?}` | Adds a survival illness as a red effect counting down in game time. `quiet` skips the "You caught" line (illnesses already present at load). `id`: `fatigue` `infection` `insomnia` `lethargy` `parasites` `weakness`. |
| `cure` | `true` \| illness id | Removes all illnesses, or one. |

Stage names (index 0–5):

| | 0 | 1 | 2 | 3 | 4 | 5 |
|---|---|---|---|---|---|---|
| food | Well-fed | Peckish | Hungry | Famished | Ravenous | Starving |
| water | Hydrated | Parched | Thirsty | Mildly dehydrated | Dehydrated | Severely dehydrated |
| rest | Well-rested | Tired | Overtired | Weary | Exhausted | Incapacitated |

```json
{"survival":{"on":true,"fatigue":0.18,"stages":{"food":1,"water":2,"rest":3},"adrenaline":{"rank":4,"kills":2,"perRank":5,"bonus":"+20% dmg"}}}
{"needs":{"food":0.7,"water":0.45,"rest":0.25}}
{"weight":{"cur":286,"max":300}}
{"illness":{"id":"parasites","hoursLeft":120}}
{"illness":{"id":"infection","hoursLeft":36,"quiet":true}}
{"cure":true}
```

## Weapon, ammo, grenades

| Field | Type | Notes |
|---|---|---|
| `weapon` | string | Equipped weapon's display name. |
| `clip`, `clipMax` | integer | Rounds loaded, magazine size. |
| `reserve` | integer | Matching ammo in inventory, not counting the loaded rounds. |
| `weaponLegendary` | string[] | Legendary effect names on the equipped weapon (LEO allows up to 5). `[]` hides the line, and so does `settings.showWeaponLegendary: false` (MCM "Legendary effects on weapon", default on); the effects are kept and reappear when it is turned back on. |
| `explosive` | `{name, short?, count, announce?}` \| `null` | Whatever is in the throwable slot (vanilla favorites and Explosives Cycler both equip there). A new name flashes in full, a lower count plays the throw tick, `count:0` shows red "00" then folds the row. `null` hides it. `short` defaults to the first word, uppercased, 5 letters. `announce:false` skips the flash. |
| `grenades` | integer | Count only, without changing the item. |
| `flashlight` | boolean | The Pip-Boy light (or a mod light on its toggle) is on: a small lamp row shows under the ammo. Mirrors vanilla's own flashlight icon, which Phosphor hides outside power armor. |

```json
{"weapon":"Combat Shotgun","clip":3,"clipMax":8,"reserve":216,"weaponLegendary":["Bloodied","Explosive"]}
{"explosive":{"name":"Plasma Grenade","short":"PLSM","count":2}}
{"explosive":null}
{"flashlight":true}
```

## Limbs and figure

| Field | Type | Notes |
|---|---|---|
| `limbs` | `{head?, torso?, larm?, rarm?, lleg?, rleg?}` fractions | Condition ÷ 100. 0 = crippled (blinks red, posts "Left leg crippled", keeps the figure lit). |
| `figure` | `{type?, hideHealthy?}` | `type`: `silhouette`, `vaultboy`, `vaultgirl` (needs the `figures/` folder; falls back to the silhouette when files are missing). Resolve the MCM "Auto" choice in the plugin. |
| `gear` | `{clothed?, armor?:{head,torso,larm,rarm,lleg,rleg}}` | Which layer each Vault Boy/Girl part draws: clothed or naked base, plus armor pieces on top. Send on equip and unequip. |

```json
{"limbs":{"head":1,"torso":0.6,"larm":0.2,"rarm":1,"lleg":0,"rleg":0.9}}
{"figure":{"type":"vaultgirl","hideHealthy":true}}
{"gear":{"clothed":true,"armor":{"torso":true,"larm":true,"rarm":false}}}
```

## Active effects

| Field | Type | Notes |
|---|---|---|
| `effect` | object | Add or refresh one effect by `id`. Fields: `id`, `name`, `kind` (`chem` amber, `med` green, `drink` cyan, `food` wheat, `bad` red), `icon`, and either real-time `duration`/`remaining` in seconds or game-time `hours`/`hoursLeft`. `duration:0` = no timer (addictions). The HUD counts down locally. |
| `effects` | array of the above | Replaces the whole tray. Use after a load and every few seconds as a resync. |
| `effectRemove` | string | Remove by id. Add `"announce":true` in the same message to post "X wore off". |

```json
{"effect":{"id":"psycho","name":"Psycho","kind":"chem","icon":"bolt","duration":480,"remaining":480}}
{"effect":{"id":"wellrested","name":"Well Rested","kind":"med","icon":"bed","hours":8}}
{"effect":{"id":"0003f2c1","name":"Grilled Radstag","kind":"food","icon":"food","duration":3600,"remaining":3600}}
{"effectRemove":"psycho","announce":true}
{"effects":[{"id":"radx","name":"Rad-X","kind":"med","icon":"capsule","duration":60,"remaining":41}]}
```

## Compass

| Field | Type | Notes |
|---|---|---|
| `heading` | degrees | Camera facing, 0 = north, clockwise; falls back to the player's facing if the camera direction is unavailable. Send whenever it changes (fast tick). |
| `markers` | array | Replaces every marker. |
| `marker` | object | Add, or update by `id`. |
| `markerRemove` | string | Remove by id. |

Marker fields: `id`, `t` (type), `b`, `dist`, `dz`, `label` (shown in the focus readout when you face it), `active` (quests: `false` = not tracked, smaller and dimmer; tracked quests stay pinned to the compass edge when behind you), `disc` (locations: `false` = undiscovered, hollow and grey; hidden when `settings.showUndiscovered` is `false`, MCM "Show undiscovered locations on compass", default `true`, 0.9.22).

Enemy markers: the plugin adds one `enemy` per actor in combat and hostile to the player within 150 m.

Types: `quest`, `door` (quest objective is behind this load door), `settlement`, `city`, `cave`, `vault`, `factory`, `landmark`, `pin` (custom map marker), `enemy`, `ally`. The HUD adds its own `defend`, `mine` and `loot` markers from the settlement, mine and legendary messages, so don't send those. The plugin sends one `ally` marker per companion (`id` `ally:<name>`) at the companion's real bearing and distance. Quest markers that share a door are sent as one `door` marker labelled `Door · N objectives` (a single one keeps its objective text).

In survival the HUD drops `enemy` markers unless the "Enemies on compass in survival" setting is on.

```json
{"heading":27,"markers":[{"id":"q1","t":"door","b":42,"dist":142,"dz":12,"label":"Relay Tower","active":true},{"id":"v111","t":"vault","b":-68,"dist":780,"label":"Vault 111","disc":true},{"id":"e1","t":"enemy","b":118}]}
{"marker":{"id":"q1","t":"quest","dist":38,"dz":-9,"label":"Broadcast source"}}
{"markerRemove":"q1"}
```

## Quests and challenges

| Field | Type | Notes |
|---|---|---|
| `tracker` | `{title, objectives:[{t, done?, dist?}], others?:string[]}` \| `null` | The tracked quest. Resend the whole thing when anything changes; objectives are matched by text, so a newly completed one gets a check and a new one slides in. `dist` shows on the first open objective. `others` lists the other active quests in one line. |
| `quest` | `{eyebrow?, title, objectives?:[{t, done?}], tone?, ms?}` | Centre banner. `eyebrow` defaults to "Quest completed". `tone`: none (cyan), `"alert"` (red), `"good"` (green). Banners queue so they never overlap. |
| `challenges` | `[{id, name, cur, max}]` | CHALLENGES the player tracks. `[]` hides the block. |
| `challengeComplete` | `{id, name, reward?, xp?}` | Green banner, feed item, optional XP, then drops it from the list. Only fires once per id. |

```json
{"tracker":{"title":"Signal in the Static","objectives":[{"t":"Reach the relay tower","done":true},{"t":"Get inside the relay tower","dist":142}],"others":["Salt and Rust"]}}
{"quest":{"eyebrow":"Quest started","title":"Signal in the Static","objectives":[{"t":"Find the source of the broadcast"}]}}
{"challenges":[{"id":"hitdeck","name":"Hit the Deck","cur":31,"max":50}]}
{"challengeComplete":{"id":"hitdeck","name":"Hit the Deck","reward":"Perk: Hit the Deck","xp":0}}
```

## Feed, messages, subtitles, prompt

| Field | Type | Notes |
|---|---|---|
| `notify` | `{text, sub?, kind?, icon?, ms?}` | One feed line, top right. `kind` picks the colour and default icon: `item` (default), `weapon`, `radio`, `level`, `quest`, `caps`, `chem`, `bad`, `legendary`. `icon` overrides it with any effect icon. `ms` defaults to 7000. |
| `message` | `{title?, text, ms?}` \| `null` | Tutorial/help box, top left. `null` hides it (the plugin mirrors vanilla's tutorial box). |
| `subtitle` | `{who?, line, ms?}` \| `null` | Bottom-centre subtitle. `ms` defaults to the line length × 70, at least 3.5 s. `null` hides it (the plugin follows vanilla's own timing). |
| `crosshair` | `{spread, reticle?, dot?}` \| `null` | The game's crosshair radius in HUD stage pixels (vanilla CrosshairBase `requestedRadius` × clip scale × 1.5). Sets the gap between the four ticks; `null` restores the default 8 px. `reticle`: vanilla shows its ticks (a drawn gun); `false` hides the ring. `dot`: vanilla shows its ticks or its own dot; `false` hides the dot. With no weapon, a holstered weapon or melee both are `false` unless vanilla's dot is up, so the crosshair follows vanilla's. Missing fields keep both. |
| `prompt` | `{key, text, hold?}` \| `{name?, keys:[{key?, text}], crime?}` \| `null` | Activation prompt under the crosshair. `key` is GetButtonPrompt's token (keyboard text or `"gp:A"`). The second form mirrors vanilla's rollover: the target's name over its button hints; `crime` turns it red. |

```json
{"notify":{"text":"Purified Water","sub":"+1 · WT 1.0","kind":"item","icon":"bottle"}}
{"message":{"title":"Tutorial","text":"Hold the workshop key to build."}}
{"subtitle":{"who":"Settler","line":"Water pump is running again."}}
{"prompt":{"key":"E","text":"Talk to Piper"}}
{"prompt":null}
{"prompt":{"name":"Piper","keys":[{"key":"E","text":"Talk"},{"key":"R","text":"Pickpocket"}]}}
{"prompt":{"name":"Toolbox","keys":[{"key":"E","text":"Steal"}],"crime":true}}
{"subtitle":null,"message":null}
```

## Target, loot, legendaries

| Field | Type | Notes |
|---|---|---|
| `target` | `{name, level, pct, legendary?, mutations?}` \| `null` | Enemy bar, top centre. `pct` is health fraction. `mutations`: `[{name, kind}]`, kind `offense` (red), `defense` (cyan), `heal` (green). |
| `targetMutated` | `{name, kind, heal?}` | The current target just mutated: flashes the bar, adds a chip, `heal` adds that fraction back. Posts to the feed only when `mutationNotices` is `"feed"`. |
| `legendaryLoot` | `{id, item, effects?, dropped?, picked?, b?, dist?}` | `dropped:true` posts "Legendary item dropped" and (with `b`) puts a gold star on the compass; `picked:true` posts the gold pickup line and removes the star. |
| `loot` | `{name, items:[{n, q?, w?, leg?}], sel?, keys?, locked?, steal?}` \| `null` | Quick-loot panel for the container under the crosshair (the plugin reads vanilla's quick-container event). `w` per-item weight, `leg` legendary star. `keys`: `[{key?, text}]` button hints, `key` as GetButtonPrompt returns it. `locked` shows Locked; `steal` turns the panel red. Long lists scroll in a 7-row window that follows `sel`. |
| `mutationFeed` | bool | Direct switch for mutation feed lines (normally via `settings`). |

```json
{"target":{"name":"Legendary Raider Veteran","level":28,"pct":0.6,"legendary":true,"mutations":[{"name":"Ballistic hide","kind":"defense"}]}}
{"targetMutated":{"name":"Healing factor","kind":"heal","heal":0.25}}
{"legendaryLoot":{"id":"ff0a12","dropped":true,"item":"Combat Rifle","effects":["Two Shot"],"b":40,"dist":12}}
{"legendaryLoot":{"id":"ff0a12","picked":true,"item":"Combat Rifle","effects":["Two Shot"]}}
{"loot":{"name":"Wooden Crate","items":[{"n":"Duct Tape","q":2,"w":0.5}],"sel":0}}
{"loot":{"name":"First Aid","items":[{"n":"AI-2 Medkit [II]"},{"n":"Herbal Restorant","q":2},{"n":"Dirty Bandage","leg":true}],"sel":1,"keys":[{"key":"F","text":"Take"},{"key":"R","text":"Transfer"},{"key":"Space","text":"Equip"}]}}
{"target":null,"loot":null}
```

## Stealth

The sneak-freeze hotfix disables the live multiplier calculation and sends `mult:null`. The HUD still accepts
numeric multipliers for compatibility, but live play shows "Sneak attack" without a number. Detection and
actual game damage are unchanged.

| Field | Type | Notes |
|---|---|---|
| `stealth` | `{sneaking, level?, state?, mult?}` | `level` 0–100 is the highest detection on the player (the value vanilla uses for its HIDDEN/CAUTION/DANGER/DETECTED text). `state` (`hidden` `caution` `danger` `detected`) overrides the thresholds (<20, <50, <85). While sneaking the HUD dims and the crosshair tightens; hidden + a target shows "Sneak attack ×N", where `mult` is the game's sneak attack multiplier for the equipped weapon and target (perks included); without `mult` it reads "Sneak attack". |

```json
{"stealth":{"sneaking":true,"level":35}}
{"stealth":{"sneaking":false}}
```

## Companions

| Field | Type | Notes |
|---|---|---|
| `party` | array | The whole team, in display order. Members missing from the array are removed. |
| `companion` | object \| `null` | Add or update one member by `name`. `null` clears the team. Going to `state:"downed"` posts "X is down". |
| `companionRemove` | string | Remove by name. |
| `companionReact` | `{name, kind}` | "▲ Liked" / "▼ Disliked" floats off that portrait. `kind`: `loved` `liked` `disliked` `hated`. Crossing an affinity rank posts to the feed. |

Member fields: `name`, `type` (`human` `dog` `robot` `mutant`, picks the fallback icon), `hp` (fraction), `state` (`following` `waiting` `downed`), `affinity` (−1000…1000, or `null` for companions without affinity such as Dogmeat), `rank` (overrides the HUD's rank label), `portrait` (path to an image; by default the HUD tries, in order: a base companion matched by word (piper, nick, preston, maccready, danse, cait, codsworth, deacon, dogmeat, hancock, x6-88), `portraits/<whole-name>.svg` with the name lower-cased and joined by hyphens (`blood-rose`, `f4-111`), then `portraits/<word>.svg` for each word of three or more letters (`gage` for Porter Gage); a missing file falls through to the next, ending at the type icon).

Up to 12 portraits show; with 13 or more the HUD shows 11 and folds the rest into "+N".

```json
{"party":[{"name":"Piper","affinity":780},{"name":"Dogmeat","type":"dog","affinity":null},{"name":"Nick Valentine","hp":0.6,"affinity":910}]}
{"companion":{"name":"Piper","hp":0,"state":"downed"}}
{"companionReact":{"name":"Piper","kind":"liked"}}
{"companionRemove":"Dogmeat"}
```

## Settlements and mines

| Field | Type | Notes |
|---|---|---|
| `settlementAttack` | object \| `null` | `id`, `name`, `faction?`, `total?` (attackers), `remaining?`, `defense?`, `state` (`pending` on the way, `active` fighting, then `won`, `lost` (didn't make it), `ended` (called off)), `b`/`dist` (pulsing defend marker on the compass), `hoursLeft?` (when vanilla will resolve it without you), `vertibird?`. Updates merge by `id`; an end state clears itself after 6 s. |
| `mines` | array | Every armed mine currently counting down at the player. Send 10–20 times a second while the list is non-empty, then `[]` once. Fields: `id`, `name`, `dist`, `left`/`total` (fuse seconds), `disarm` (in reach), and either `x`/`y` (stage pixels on a 1920×1080 canvas, from projecting the mine) or `onScreen:false` + `bearing` (degrees relative to the camera, 0 = ahead, 90 = right). Optional `b` adds a compass marker. |
| `mineExploded` | string \| string[] | Plays the blast at those mines' last positions. Send with the new `mines` list. |

```json
{"settlementAttack":{"id":"abernathy","name":"Abernathy Farm","faction":"Raiders","total":12,"defense":85,"state":"pending","b":118,"dist":1320}}
{"settlementAttack":{"id":"abernathy","state":"active","remaining":7}}
{"settlementAttack":null}
{"mines":[{"id":"ff012a","name":"Frag Mine","x":905,"y":792,"dist":5,"left":2.1,"total":4.5,"disarm":true,"b":31},{"id":"ff012b","name":"Frag Mine","onScreen":false,"bearing":150,"dist":8,"left":3,"total":4}]}
{"mines":[],"mineExploded":"ff012a"}
```

## Karma and reputation

| Field | Type | Notes |
|---|---|---|
| `karma` | `{value?, alignment?, title?, change?, reason?}` | `value` from KARMA's global; `alignment` (−2…2) is worked out from it at ±250 and ±750 unless sent. `title` is KARMA's level-based title. `change` + `reason` post "+15 Karma · Healed a settler". Changing alignment posts once and flashes the readout. |
| `reputation` | `{faction, title?, fame?, infamy?, event?, detail?}` | `title` is one of REPUTATION's titles (Idolized, Liked, Accepted, Neutral, Shunned, Hated, Vilified, Wild Child, Merciful Thug, Smiling Troublemaker, Sneering Punk, Good-Natured Rascal, Dark Hero, Soft-Hearted Devil, Unpredictable). `event`: `discovered`, `fameGained`, `fameLost`, `infamyGained`, `infamyLost`, `hitSquad`, `gift`. A title change posts once. |

```json
{"karma":{"value":380,"title":"Defender","change":15,"reason":"Healed a settler"}}
{"reputation":{"faction":"Diamond City","title":"Liked","event":"fameGained"}}
{"reputation":{"faction":"the Gunners","event":"hitSquad"}}
```

## Armor plates (optional mod)

`settings.textScale` (number 0.8–1.5, default 1, MCM "Text size (%)") multiplies every HUD font size except the large numerals (ammo count, quest title, level-up number); the captions under the vital rings stop at 1.3 so the row stays clear of the compass. Subtitles narrow and the loot panel widens slightly as text grows. `settings.iconScale` (number 0.8–2, default 1, MCM "Compass icon size (%)") sizes every compass marker; markers grow upward from the same baseline and stay centred on their bearing. `settings.ringScale` (number 0.8–1.5, default 1, MCM "Ring size (%)") sizes the vital rings (including weight, needs and the armor wheel), the active-effect rings and the companion portraits; when the single vitals row would come within 16 px of the compass, food/water/rest move to a row above the main rings (`.cores.two-rows`), and the effects tray wraps beyond 480 px (0.9.23).

`settings.showArmorWheel` (boolean, default `true`, MCM "Show armor wheel") hides the armor ring without stopping the data. Turning it back on shows the latest armor value.

Warzone - Armor Plates System (Nexus, LuBuCake; `Warzone - Armor Plates System.esp`). The plugin sends `armor` only while that esp is loaded, and the HUD builds its armor core from the first message that carries it. Without the mod nothing is built, nothing is drawn and no log line beyond the one at data-ready is written.

| Field | Type | Notes |
|---|---|---|
| `armor` | `{current, max, perPlate, stock}` or `null` | `current` is armor now (`WZ_PlatesPlayer_Health`), `max` is the armor pool (`WZ_PlatesPlayer_MaxHealth`), `perPlate` is armor per plate (`WZ_PlatesPlayer_HealthToAdd`), and `stock` is the plates carried (the `WZ_PlatesSystem_Plates` item). The plates shown are `max / perPlate`, rounded, from 1 to 8. A drop in `current` pops a red "-N" on the ring. `current` at 0 shows the broken state. `null` hides the core again. |

The core sits in the vitals cluster before weight, with one arc per plate and `armor · ×stock` underneath.

```json
{"armor":{"current":110,"max":150,"perPlate":50,"stock":3}}
{"armor":{"current":0,"max":150,"perPlate":50,"stock":0}}
```

## Context: what shows where

### OG 0.9.19 crosshair and out-of-combat settings

`settings.showCrosshair` and `settings.showHitMarkers` are independent booleans, both defaulting to `true`. Crosshair off hides Phosphor's ring/dot even with third-person aiming or a context Show everything override. It does not hide interaction prompts, quick loot or sneak-attack text. Hit markers have a separate wrapper and keep their existing context visibility; disabling hit markers clears an active marker and prevents new ones. Native vanilla-crosshair suppression is unchanged, including its power-armor exemption.

`settings.calmDelay` is an integer from 0 to 15 seconds, default 5. `settings.calmRules` maps `vitals`, `compass`, `kit`, `party`, `tracker`, `effects`, and `cross` to `"show"`, `"dim"`, or `"hide"`; every default is `"show"`. Partial rule updates merge with the current rules. `cross` affects only the ring/dot, never hit markers or interaction widgets. Unlisted event groups retain their normal rules.

The `calm` situation follows ADS and precedes explore. It begins after uninterrupted time with the existing `combat` signal false; repeated false messages do not restart it, a true message leaves it immediately, and a save reset starts fresh. All existing higher-priority contexts win. Delay changes are measured from the current noncombat interval. Show keeps existing idle opacity; Dim uses `ctx-dim` (0.3, composed with normal parent opacity).

Calm hiding/dimming yields while that part or its parent/child has the existing `.awake` state, or for about five seconds after a relevant update. HP below 35% of hpMax keeps vitals visible. Ammo/weapon updates and the existing firing kick wake weapon and crosshair; quest banners or changed quest progress wake the tracker. Compass marker-list membership/details and effect-list membership/details wake their parts; continuously changing bearings/distances and effect countdowns do not continually reawaken them. Illness and cure messages wake effects. A changed party list (members or their following/waiting/downed state), a single companion update, a removal or an affinity reaction wakes the companion row; HP changes alone do not. These rules use only messages already supplied by the plugin. The location-arrival countdown follows the resulting visible state, including temporary calm wakes.

| Field | Type | Notes |
|---|---|---|
| `context` | object | `menus` (string[]: every game menu currently open), `powerArmor`, `ads` (aiming down sights, not scoped), `thirdPerson` (standard third-person camera active; false if unknown), `scene` (NAF/AAF scene running), `photo`, `hudHidden` (player toggled the HUD off), `prismaFocus` (another PrismaUI page has focus), `rules` (overrides, normally via `settings`). Send the whole object on every change. |

The HUD turns this into one situation, highest first: `hidden`, `photo`, `menu`, `scene`, `powerarmor`, `vats`, `dialogue`, `workshop`, `scope`, `wheel`, `ads`, `calm`, `explore`. The "What shows where" table on the preview page shows the result for every situation.

`settings.hideThirdPersonAimCrosshair` (boolean, default `false`) controls only the aiming reticle in third person. With `context.ads` and `context.thirdPerson` both true, the recommended ADS rule keeps the reticle visible unless this option is enabled. The prompt, quick loot and other interaction widgets retain their prior ADS rules. Explicit ADS "show everything"/"hide everything" overrides take precedence; higher-priority scopes, menus, VATS and power armor keep their existing rules. The native plugin reports camera mode on the normal slow update; it never forces visibility on a native game crosshair.

The player's custom map waypoint uses the existing marker contract: `{"id":"player-waypoint","t":"pin","b":90,"dist":100,"label":"Custom waypoint","active":true}`. It comes from the Pip-Boy feed's `Map.World.Custom` outdoors or `Map.Local.Custom` indoors. `Visible=false`, absent/incomplete coordinates, or feed disconnection removes it from the next full marker list. Moving it retains its ID and updates its bearing and distance. This pin stays at the compass edge when behind the player, and is independent of the nearby-location range limit. No height arrow is inferred from the feed's frequently zero `Height` field.

Menu names it maps (anything else is ignored):

| Situation | Menus |
|---|---|
| `menu` | PipboyMenu, ContainerMenu, BarterMenu, ExamineMenu, TerminalMenu, LockpickingMenu, LevelUpMenu, SleepWaitMenu, PauseMenu, Console, LooksMenu, BookMenu, LoadingMenu, MainMenu, PowerArmorModMenu, SPECIALMenu, CreditsMenu, CookingMenu, NAFMenu, NAFStudioMenu, FalloutEmotions2Menu, OCM_outfitChangeMenu, AddItemMenu, TextInputMenu |
| `vats` | VATSMenu |
| `dialogue` | DialogueMenu |
| `scope` | ScopeMenu |
| `workshop` | WorkshopMenu |
| `wheel` | FavoritesMenu, WheelMenu |
| `photo` | ScreenArcherMenu, PhotoMenu |

The mod menu names come from their files and need confirming in game (see `06_OPEN_QUESTIONS.md`). To add one, add it to `MENU_CTX` in `hud.html`.

```json
{"context":{"menus":["ScopeMenu"],"powerArmor":false,"ads":false,"scene":false,"photo":false,"hudHidden":false}}
{"context":{"menus":[],"prismaFocus":true}}
```

---

## Icon names

**Effect and feed icons** (`effect.icon`, `notify.icon`): `bolt` `shield` `hourglass` `dumbbell` `capsule` `syringe` `ivbag` `bottle` `warn` `infection` `parasites` `insomnia` `lethargy` `weakness` `karma` `star` `medal` `hitsquad` `gift` `sleepy` `bed` `food`. Unknown names fall back to `bolt` (effects) or the kind's icon (feed).

Suggested mapping for vanilla consumables: Psycho/Psychobuff/Psychotats `bolt`, Med-X `shield`, Jet/Jet Fuel/Ultra Jet `hourglass`, Buffout/Buffjet `dumbbell`, Rad-X/Mentats/Daddy-O/Day Tripper `capsule`, Stimpak/Med-X injection `syringe`, RadAway `ivbag`, Nuka-Cola and other drinks `bottle`, food `food`, addictions `warn`.

**Compass types**: `quest` `door` `settlement` `city` `cave` `vault` `factory` `landmark` `pin` `enemy` `ally` (plus the HUD's own `defend`, `mine`, `loot`).
