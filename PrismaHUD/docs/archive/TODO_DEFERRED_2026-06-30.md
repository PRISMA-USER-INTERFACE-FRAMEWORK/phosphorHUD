# PrismaHUD_F4 — Deferred Work & Needs-Input

Pick-up list after the 2026-06-30 full code review. Everything in
[CODE_REVIEW_2026-06-30.md](./CODE_REVIEW_2026-06-30.md) marked critical/important is already fixed + deployed.
This file is what's left: things that need a decision from you, and work I deferred on purpose.

---

## 🔵 NEEDS YOUR INPUT (I can't decide these)

### 1. Vanilla ammo SWF hide — ✅ DONE (2026-06-30)
Probe captured the path (`_root.RightMeters_mc.AmmoCount_mc` + `.ExplosiveAmmoCount_mc`).
`HideVanillaAmmo()` now `SetVariable`s their `._visible=false` on the main thread, re-asserted each ammo
poll (the engine re-shows them on weapon draw). Built + deployed. **Retest:** confirm the vanilla ammo
counter is gone (and no visible flicker on weapon draw — if there's a brief flash, I can hook the show
instead of polling). Probe removed; only the `s_diag` ammo logger remains (see A).

### 2. Rads widget math (`radsMax` / `hpLost`)
`HUDDataProvider.cpp` hardcodes `radsMax = 1000` in two places and reads `radHealthMax` into a field named
`hpLost`. **Question:** what should the rads bar actually represent?
- (a) Rads out of a fixed 1000 (current), or
- (b) Rads as a fraction of the current max-HP reduction (`radHealthMax`), i.e. how much HP rads have eaten?
Tell me which and I'll wire the correct formula + rename the field.

### 3. Quick-loot rarity tiers
The quickloot UI has full rarity coloring (uncommon/rare/veryrare/legendary) but C++ always sends
`"common"` (`QuickLootController.cpp`). **Question:** do you want real rarity? FO4 has no vanilla item
"rarity" — options: derive from value thresholds, from a keyword (e.g. legendary keyword), or drop the
rarity UI. Your call on the source.

### 4. Editor: aspect-locked scale + per-widget 90° rotate  (features, not bugs)
Both are **completely absent** (confirmed in `hud_editor.html` — only free 8-handle resize exists, no
rotation field anywhere). These are real feature builds. **Questions before I implement:**
- Scale: uniform aspect-lock always, or hold-Shift to lock aspect (free otherwise)?
- Rotate: a 90°-step button (0/90/180/270) is simplest and matches "rotate a single widget 90°" — confirm
  that's what you want vs. free rotation. Rotation needs a new `rot` field plumbed through
  WidgetEntry → editor payload → each view's `applyPos` (`transform:rotate`).

### 5. `hud_container.html` — delete or wire up?
Orphaned dead view (never created in `main.cpp`, no `BuildContainerJSON`, diverges from the widget
contract). Looks superseded by the quickloot panel. **Delete it, or do you still want a "nearby
containers" widget?**

---

## 🟢 DEFERRED — ready to do, no input needed (just say go)

### A. Remove remaining debug scaffolding
- ✅ `DumpHUDMenuGfx` probe removed (became `HideVanillaAmmo`).
- ⏳ `s_diag` ammo logger (`HUDDataProvider.cpp`, ~10s "PrismaHUD ammo:" spam) still present. Remove it +
  the now-diagnostic-only `eqCount`/`dataNull` locals it feeds. Kept one more session in case the ammo
  values need another look after retest.

### B. Remaining medium hardening (from the review, non-blocking)
- `IsSanePos` allows a widget fully off-screen (no `x+w ≤ ~1.05` bound) — add a bound so a bad layout
  can't hide a widget with no recovery.
- Editor toggle uses `GetAsyncKeyState(VK_LSHIFT)` (global keyboard state, not game input) — switch to the
  ButtonEvent modifier state; also verify the `0x7A` VK vs BS-button-code path for the editor key.
- Quest-item filtering in quickloot: the old `As<TESBoundObject>` "filter" never actually filtered quest
  items (they can appear in the loot list). Add a real quest-item/non-playable check if you want them hidden.
- Protect `SaveSlotsFile` + the inline write in `SaveFromJS` the same way `SaveLayoutFile` now is
  (try/catch + atomic), or route both through `SaveLayoutFile`.
- Convert the empty `catch(...){}` editor handlers in `main.cpp` to `catch(const std::exception&)` + log.

### C. Incomplete features (bigger, design-dependent — see #4 and the review's 🧩 list)
- Grenade widget can't show a **real equipped grenade** (grenades aren't in `equippedItems`; needs the
  player's equipped-throwable source). Right now it correctly hides for a gun — fine until you use grenades.
- Loaded-ammo accuracy — confirm `8/101`-style values match vanilla once you're back in-game.

---

## 🎮 IN-GAME RETESTS PENDING (next session)
- [ ] Grenade widget no longer shows the equipped pistol (should hide with a gun out).
- [ ] Ammo reserve count is correct now (stack double-count fixed).
- [ ] Weapon **name** shows in the ammo widget.
- [ ] Quick-loot: no duplicate rows; looting doesn't freeze (emoji purged).
- [ ] Compass: smooth turning, tick-mark look, correct size, quest blip points right.
- [ ] Paste the `HUDMenu GFx:` log lines (for #1).
- [ ] Load **from the Main Menu** (cold) to dodge the engine string-pool load freeze.

---
_Full findings: [CODE_REVIEW_2026-06-30.md](./CODE_REVIEW_2026-06-30.md). Session changelog:
`docs/Prisma/CHANGELOG.md` (2026-06-30 pt7)._
