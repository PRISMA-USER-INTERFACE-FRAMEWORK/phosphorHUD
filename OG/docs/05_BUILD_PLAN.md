# 05 · Build plan

Seven stages. Each one ends playable, with a test checklist to pass before moving on. Stage 0 builds the debug replay first, so every later stage can be checked against `examples/messages.jsonl` before the real data reads are trusted.

Conventions for every stage:

- Every new message field gets one line in `examples/messages.jsonl` and passes `tools/replay.py`.
- Log every outgoing message at debug level (`Data\F4SE\Plugins\PhosphorHUD.log`). Copy lines from it straight into the replay tool when something looks wrong.
- A missing optional mod disables only its feature; it never stops the plugin from loading.

---

## Stage 0 · Skeleton and debug replay

Implementation update (2026-10-08): the Stage 0 DLL and automated tests are built. See `STAGE0_TEST.md` for the personal-use package and Ctrl+F10 replay instructions. All in-game checkboxes below remain unchecked; no in-game validation has been claimed. Stages 1–6 have not started.

Build:
- CommonLibF4 F4SE plugin for OG 1.10.163, logging, version check.
- Request PrismaUI `IVPrismaUI12` at `kPostLoad`; create the view at `kGameDataReady`; `SetViewRole(kWidget)`.
- MCM settings read (defaults + overrides) and `settings` sent on DOM ready.
- **Debug replay:** a hotkey (or console command) that reads `Data\F4SE\Plugins\PhosphorHUD\replay.jsonl` and sends one line per second through the same `InteropCall` path. Ship `examples/messages.jsonl` as that file during development.

Test:
- [ ] Game starts with PrismaUI installed; log shows the API version and "view created".
- [ ] Game starts without PrismaUI; log shows one warning; no crash.
- [ ] Main menu: nothing from Phosphor visible.
- [ ] Load a save, press the replay key: the HUD appears on the first snapshot line and every example plays without errors in DevTools (F12).
- [ ] Change a setting in MCM, close the pause menu: log shows a new `settings` message.

## Stage 1 · Core HUD and the vanilla HUD swap

Build:
- Snapshot on `kPostLoadGame` / `kNewGame`, preceded by `reset`.
- Vitals (`hp` `hpMax` `ap` `apMax` `rads`), `level` `xp` `xpGain` `levelUp` `perkPoints`.
- `location` `region`, `time`, `combat`.
- `weapon` `clip` `clipMax` `reserve`, `explosive`.
- `heading`, `markers` for the tracked quest target (with the `door` case), map markers in range, enemies.
- `context` from menu events, power armor, iron sights, HUD toggle; `paused` while a `menu` situation is open.
- Suppress the vanilla widgets Phosphor replaces (confirmed class names first); stop suppressing in power armor.

Test:
- [ ] Load: HUD fades in with correct HP/AP/RAD, level and XP, location, clock, weapon and ammo.
- [ ] Take damage, use a Stimpak, take rads, use RadAway: the cores match the vanilla values to within 1.
- [ ] Fire and reload: clip and reserve match the vanilla counter (check with the vanilla widget still on, then suppress it).
- [ ] Equip, throw the last grenade, unequip: the grenade row shows, counts down, shows 00, hides.
- [ ] Turn in place: the compass heading matches the vanilla compass (N/E/S/W line up).
- [ ] Track a quest whose target is inside a building: the compass shows the door marker outside, the quest marker inside.
- [ ] Open the Pip-Boy, a container, a terminal: Phosphor hides; effect timers and the clock pause.
- [ ] VATS, scope, dialogue, workshop, wheel menu, iron sights: each matches the "What shows where" table.
- [ ] Enter power armor: vanilla power armor HUD shows, Phosphor keeps only quests, companions, effects.
- [ ] Wait 3 hours, sleep, fast travel, load a save: the clock is correct after each.
- [ ] Load a different save mid-fight: no old feed lines, enemy bar or banners carry over.
- [ ] No vanilla HUD element doubles up with a Phosphor one (FallUI - HUD disabled).

## Stage 2 · Combat, world and quests

Build: `target` (+ `legendary`), `stealth`, `limbs`, `figure`/`gear`, `effect`/`effects`/`effectRemove`, `notify` (pickups, discoveries), `prompt`, `loot`, `subtitle`, `message`, `quest` banners, `tracker`, perk key.

Test:
- [ ] Aim at an enemy: bar shows name, level, health; legendaries get the star and gradient.
- [ ] Sneak: the eye appears; getting spotted walks it Hidden → Caution → Danger → Detected in step with vanilla's text; "Sneak attack ×2" only while hidden with a target.
- [ ] Get a leg crippled: the figure shows it red and blinking, "Left leg crippled" posts once; a Stimpak clears it and the figure hides.
- [ ] Vault Boy / Vault Girl / silhouette from MCM all render; armor pieces follow equip and unequip.
- [ ] Take Psycho, Med-X and a Nuka-Cola: three timers with the right durations; "wore off" posts when each ends; timers pause in the Pip-Boy.
- [ ] Pick up items: feed lines match the vanilla notifications (temporarily unsuppressed to compare).
- [ ] Look at a door, a container, an NPC: the prompt shows the right key and the Alternative Activation Prompts text.
- [ ] Container under the crosshair: loot panel lists the same items as vanilla's quick loot.
- [ ] Talk to an NPC with Extended Dialogue Interface: no doubled subtitles.
- [ ] Start, update and complete a quest: banners in order, never overlapping; tracker checks off objectives.
- [ ] Gain a level from a quest reward: quest banner first, then the level-up banner; perk reminder until spent; LevelUpMenuEx's key shows on the banner.

## Stage 3 · Survival

Build: `survival` (on, fatigue, stages, adrenaline), `needs`, `illness`/`cure`, `weight`.

Test:
- [ ] Survival difficulty: food, water and rest cores appear; other difficulties: they don't.
- [ ] Let hunger drop a stage: the stage name changes in step with vanilla's message; Hungry or worse posts a reminder.
- [ ] Fatigue: the red bite on the AP ring matches vanilla's red AP section.
- [ ] Kill five enemies: adrenaline fills a rank; sleep: it drops like vanilla.
- [ ] Catch an illness (raw meat, dirty water): it appears with the right name and days left; Antibiotics remove it.
- [ ] Load up past the carry limit: weight core appears near 85 %, turns red over 100 %, "Overencumbered" posts once.
- [ ] Enemies are off the compass in survival unless the MCM switch is on.

## Stage 4 · Companions

Build: `party` (Liga's full team), `companion` updates, `companionRemove`, `companionReact`.

Test:
- [ ] One companion, then four through Liga: portraits in recruit order, compass dots for each.
- [ ] A companion gets downed: red pulse over the portrait, "X is down" once; back up after the fight.
- [ ] Team wait / follow from Liga: pause badges appear and clear.
- [ ] Do something a companion likes and dislikes: the reaction floats off the right portrait; an affinity rank change posts once.
- [ ] 13+ companions: 11 portraits and "+N".
- [ ] Dismiss one: portrait and compass dot go.

## Stage 5 · Mod integrations

One at a time, each behind a "plugin present" check:

- [ ] **KARMA**: readout matches the Pip-Boy tab; a karma change posts with its reason; alignment change posts once. Turn off KARMA's own notifications and pop-ups.
- [ ] **REPUTATION**: entering a town shows your standing; fame/infamy, discovery, hit squad and gift events post.
- [ ] **CHALLENGES**: tracked challenges show with progress; completion banner once.
- [ ] **SKK Settlement Attack System**: declared → pending card and compass marker; arrive → defend counter; win → green banner; leave → "didn't make it".
- [ ] **Immersive Legendary Mutations / LEO**: mutation chips on the bar (feed only with the MCM option); legendary drop star on the compass; pickup line with effects; weapon shows its legendary effects.
- [ ] **Mines**: markers over armed mines counting down at you, chevrons for unseen ones, compass marks, blast at detonation; nothing when `bMineWarnings` is off. Turn off Mine Detector HUD.
- [ ] **NAF / AAF**: HUD hides during scenes.

## Stage 6 · Polish

- [ ] Only changed fields go out each tick (check the log during a quiet minute: near-silent).
- [ ] Profile a busy fight (PrismaUI `[Perf] Profiler=1` plus the plugin's own timings): no measurable frame-time cost from the plugin.
- [ ] Every mod in the "turn off once Phosphor is live" list is disabled, and nothing from them shows.
- [ ] Every MCM setting works without reloading the save.
- [ ] Remove each optional mod in turn: the plugin loads, logs one line, and the rest of the HUD still works.

## Backlog

### User stylesheet (`custom.css`), requested 2026-10-16
Context: Prisma Designer (Nexus 107857) only reopens its own `.prisma` projects and exports, so it can't edit Phosphor's
hand-written `hud.html`. A user stylesheet gives modders a supported way to restyle or move things without editing our
file, and their changes survive Phosphor updates.

- **Loading.** `hud.html` loads `custom.css` from its own folder (`PrismaUI_F4/views/PhosphorHUD/custom.css`) after
  the built-in `<style>`, so its rules win at equal specificity.
- **Missing file.** Phosphor does **not** ship a `custom.css`. If it shipped one, updates would overwrite the user's
  copy. A missing file must be harmless in PrismaUI/Ultralight: no error loop, no layout change. Check this in game
  and in the browser replay. If a plain `<link>` logs errors every frame, load it once from script instead.
- **Survives updates.** Tell users to put `custom.css` in **their own small MO2 mod** with the same folder path, below
  Phosphor in the left pane. It then overrides nothing of ours and is never touched by updates.
- **Stable hooks.** Document the IDs, classes and CSS variables we commit to keeping stable:
  - colours: `--ink`, `--pip`, `--pip-soft`, `--amber`, and the rest
  - fonts: `--f-label`, `--f-mono`, `--f-display`
  - the main groups: vitals, compass, weapon block, prompts, subtitles, enemy bar, companions, effects, tracker,
    mines
  Renaming a documented hook later is a breaking change; note it in the changelog.
- **Coordinates.** The stage is a fixed 1920×1080 layout scaled to the screen, so positions in `custom.css` use that
  space. The ultrawide edge offsets (`--wide-gutter`) still apply.
- **Docs.** Add `docs/07_CUSTOMIZING.md` and a short "Customizing" section on the Nexus page, with three examples:
  recolour the theme, move the compass, hide one element.
- **Tests.** Run the replay with `custom.css` absent and with a sample present. With the sample, check that its
  rules apply and that nothing else changes. Confirm the power-armor and context-hiding rules still win, since
  `custom.css` must not be able to un-hide the HUD in menus or power armor. Prefer state classes on `.stage`
  that a user rule can't easily override, or document the limit.
- Not part of the AE work. Don't mix it into the AE experimental build.
