# PrismaHUD_F4 — Session Handoff (2026-07-07)

Paste this into a new session to resume. Everything below is **built + deployed** unless marked otherwise.

---

## 🎯 IMMEDIATE NEXT STEP — waiting on an in-game log line

The compass was rewritten and instrumented but **not yet tested in-game**. To resume, the user must:

1. Load the modlist **from the Main Menu** (cold load — avoids the BSStringPool load-race, see below).
2. **Track a quest**, run around a bit.
3. Send the `PrismaHUD compass:` line from
   `Documents\My Games\Fallout4\F4SE\PrismaHUD_F4.log`.

The diagnostic line looks like:
```
PrismaHUD compass: objectives=N displayed=N targets=N resolved=N alias0=N heading=X questDir=Y
```
Interpretation (this pinpoints the quest-detection failure):
- `displayed=0` → active-objective filter wrong (nothing matches `kDisplayed`).
- `targets>0` but `resolved=0` → alias→ref resolve broken (`GetAliasedRef`).
- `alias0` = huge/garbage → `TESQuestTarget::alias@0x10` offset wrong.
- `resolved>0`, real `questDir` → working; problem was elsewhere.

Also just ask: **is the turning smooth now, or still jittery?**

---

## What changed this session (compass)

**File:** `assets\views\hud_compass.html` — full rewrite of the smoothing model.
- Removed JS rAF hand-easing (that WAS the jitter — Ultralight's rAF frame rate is low/uneven).
- Now: C++ pushes heading → JS sets target transform → **CSS `transition` glides the tape at display
  refresh** (compositor-driven, independent of JS frame rate).
- Continuous `_disp` heading + shortest-signed delta; seamless ±360 re-base when `_disp<-90||_disp>450`
  with `void strip.offsetWidth` force-commit; poll 80ms; blip uses `left 0.13s linear`.
- Consts: `PX_PER_DEG=3.4`, `D_MIN=-180`, `D_MAX=540`, `DEF={xf:750/1920,yf:14/1080,wf:420/1920,hf:24/1080}`.

**File:** `src\HUDDataProvider.cpp` — `BuildCompassJSON` quest diagnostics.
- Added throttled log (every 6th heavy call, ~3s): counters `dbgTotal/dbgDisplayed/dbgTargets/dbgResolved/dbgAlias0`.
- Quest scan: iterate `player->objectives` (kDisplayed), read alias via
  `reinterpret_cast<const QuestTargetLite*>(tgt)->alias`, resolve via `ownerQuest->GetAliasedRef`,
  pick nearest via `atan2(dx,dy)`.
- `QuestTargetLite { uint8 flags@0x00; uint8 _pad01[7]; void* conditions@0x08; int32 alias@0x10; uint32 _pad14 }`
  with `static_assert(offsetof(alias)==0x10)` — verified via Ghidra (see 03-FINDINGS.md).

**Deployed:** DLL (834,048 bytes) + `hud_compass.html` → `E:\Modlists\Fallen World Alpha 2\mods\PrismaHUD\`.

### HONEST FALLBACK (user is skeptical of the custom compass — 2 failed passes already)
If it STILL jitters after the CSS-transition rewrite, the bottleneck is Ultralight's repaint rate and no
JS trick beats it. Robust answer: **show the vanilla FO4 compass** (native Scaleform, smooth + correct
quests) and hide the custom one — one line via GFx, same mechanism as the ammo hide already working.

---

## Also done & deployed earlier this session (verify still good in-game)

- **Vanilla ammo hide** — `HideVanillaAmmo()` in HUDDataProvider.cpp: `SetVariable(
  "_root.RightMeters_mc.AmmoCount_mc._visible", false)` + `ExplosiveAmmoCount_mc`, re-asserted on MAIN
  thread each ammo poll (engine re-shows on weapon draw). GFx paths verified via live probe (03-FINDINGS.md).
- **Ammo values** — BuildAmmoJSON: weapon name output, `IsGunWeapon()` guard, vtable-guarded downcast,
  C3 stack fix (sum stack.count once), C2 spinlock on `equippedItemsLock`.
- **Grenade widget** — BuildGrenadeJSON: `IsThrownWeapon()` (fixed the "pistol shows as grenade" magic-8/9 bug).
- **QuickLoot** — per-item processing in `a_filter` (fixes stack double-count + dup rows).
- **Layout manager** — id-dedupe eviction, exception-safe + atomic + mutexed SaveLayoutFile, corrupt-file quarantine.
- **main.cpp** — MakeView checks CreateView return, skips RegisterWidget on 0.
- **Astral emoji purged** from hud_quickloot.html + hud_editor.html (astral = HARD game freeze in Ultralight).

---

## Still TODO (deferred — see docs\TODO_DEFERRED.md + CODE_REVIEW_2026-06-30.md)

- Remove `s_diag` ammo logger + `eqCount`/`dataNull` diagnostics (deferred one session — safe to strip once ammo confirmed).
- Rads math formula (needs decision).
- QuickLoot rarity source (needs source).
- Off-screen widget bound; editor `GetAsyncKeyState`.
- Feature: editor aspect-locked scale + per-widget 90° rotate (needs design input).

---

## Build & deploy (reminder)

```powershell
cd "E:\F4SE OG\Projects\Prisma\UI_Systems\PrismaHUD_F4"
xmake
# then copy build\windows\x64\release\PrismaHUD_F4.dll + changed views\*.html
#   → E:\Modlists\Fallen World Alpha 2\mods\PrismaHUD\
```
Note xmake.lua CommonLibF4 path is `../../../../CommonLibF4` (reorg — 4 levels up).

## Gotchas that bit us (don't rediscover)
- **Infinite load freeze = engine BSStringPool/Papyrus-VM race during save-load, NOT the plugin.** Zero
  plugin frames in the thread dump; the probe log never prints (HUD isn't up yet). Mitigation: cold-load
  from Main Menu. Try `bHeapAnalysis=false` in `AddictolCrashLogger.toml`. See memory
  `project_fw_load_freeze_stringpool`.
- **Astral-plane chars (≥U+10000) in any Ultralight HTML hard-freeze the whole game, no crash log.** BMP only.
- **GFx must be touched on the MAIN thread** (`F4SE::GetTaskInterface()->AddTask`).
- **Compass HUD classes aren't in CommonLibF4** and quest-target fns aren't named in the Ghidra DB (no vtable) —
  read `PlayerCharacter` quest members directly, compute bearing yourself.

## RE pipeline (if you need engine facts)
Corrected project: `E:\F4SE OG\ReverseEngineering\Projects\fwgh_proj_1.10.163_unpacked` (`Fallout4_163_unpacked`).
Query: `E:\F4SE OG\ReverseEngineering\Scripts\run_query.ps1 "Name"` or `"decompile:Class::Method"`.
⚠ Old packed `fwgh_proj` gives garbage `.text` decompile — use the unpacked one.
