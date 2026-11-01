# PrismaHUD_F4 — Current Status

_Last verified against source: 2026-07-13. Supersedes the plugin folder's old `CODE_REVIEW_2026-06-30.md`,
`SESSION_HANDOFF.md` (2026-07-07), and `TODO_DEFERRED.md` — those tracked a moving target session-to-
session and had drifted from the live code; every item below was re-checked with a fresh grep against
current source before being marked done/open, per the workspace's "never guess a verifiable fact" rule.
Day-to-day fix history with full root-cause writeups lives in
[knowledge/findings-prismahud.md](knowledge/findings-prismahud.md) — this
file is the rollup, not a replacement for it._

---

## Shipped and working

**Core widgets (11):** health, AP, rads, compass, ammo, grenade, survival, interaction prompt,
quick-loot, plus `rad_toxic`/`rad_filter` registered externally by Rad Mod via the third-party API.
`hud_container.html` (the old orphaned "nearby containers" stub) was deleted — quick-loot supersedes it.

**Layout editor** (`hud_editor.html`) — drag-to-position, uniform per-widget **scale** (slider +
numeric, 0.4–2.5, replaced the old free 8-handle resize entirely), snap grid, undo/redo (50-entry
stack), 4 save slots, alignment tools (align/distribute/center), widget visibility eyes, multi-select.
Position schema is `{x, y, s, r}` (viewport fractions, uniform scale, quarter-turn rotation) plus an
optional `icon` override — `w`/`h` free-resize was retired. Presets, layout export/import and slot
rename all shipped 2026-08-07; see Resolved.

**Quick-loot panel** (`QuickLootController` + `hud_quickloot.*`) — keyboard-driven loot list, mesh
thumbnails (per-row 3D preview keyed by stable `formId`, not row index), carry-weight bar with
projected-add ghost segment, value-banded row tints with a separate real legendary
flag (engine `ExtraDataList::GetLegendaryMod`, read off stack extra data), real item value/weight via `TESValueForm`/`TESWeightForm` (works for AlchemyItem too,
unlike the old per-type raw-InstanceData reads), lock-state gating (matches vanilla: shows nothing on
a still-locked container, appears once unlocked), settlement workshop-bench detection (`Workshop::
ExtraData`) with a minimal Transfer-only view, and perk-driven activate-choice surfacing (any mod using
the vanilla `BGSEntryPointFunctionDataActivateChoice` entry point — e.g. WH Butchering's "Field Dress",
Workshop Framework's "Manage Workbench" — is picked up dynamically with zero PrismaHUD code changes;
see the `ActivateChoiceListener` capture in the framework's `VanillaUISuppressor.h` part D/E).

**Menu/context suppression** — HUD hides under Crafting/Terminal/Workshop/Chemistry/Cooking menus
(`HUDLayoutManager::SetMenuSuppressed` + `MenuSuppressSink` in `main.cpp`) and while lockpicking; quick-
loot specifically suppresses the vanilla `ContainerMenu`. The interaction prompt no longer sticks on
screen after the crosshair leaves a target (switched from the unreliable `RE::Console::GetPickRef()` to
`QuickLootController`'s own event-driven pick cache).

**Rebindable input (Feature 2)** — `BindingRegistry` + `keybinds.json`, exclusive-input-mode gating
shared with PrismaWheelMenu so the two don't fight over input focus.

**Compass** — heading/location/quest-blip all working as of 2026-07-07 pt7/pt8: quest blip now only
tracks Pip-Boy-**tracked** quests (gates on `TESQuest::data.flags & 0x0800`, matching the vanilla
compass's own `CalculateCompassMarkersFunctor`, decompile-verified against 1.10.163) instead of the
nearest displayed objective across every active quest; turn smoothing is CSS-`transition`-driven
(compositor, not JS rAF) so it isn't bottlenecked by Ultralight's/CEF's paint rate.

**HTML/CSS/JS split (2026-07-13)** — all 11 widget views (+ editor) split from single-file HTML into
separate `.html`/`.css`/`.js` per view (33 files), served via the `prisma://` custom CEF scheme with
correct MIME types. JS modernized conservatively (`let`/`const`, template literals, arrow-function
callbacks, native `NodeList.forEach`, `e.currentTarget` instead of ambient `this`) — no behavior change
intended, this was a maintainability pass only.

---

## Genuinely still open (re-verified, not carried over blindly)

- **Rads widget math unfinalized.** `HUDDataProvider.cpp` still hardcodes `radsMax = 1000.f` in two
  places (`BuildHealthJSON`, `BuildRadsJSON`) and reads `radHealthMax` into a field named `hpLost`.
  Needs a decision: rads out of a fixed 1000, or as a fraction of the current max-HP reduction.
- **Task 1's own two runtime-verify items are still unconfirmed in-game** (per
  `docs\Prisma\findings\open-items.md`): compass blip/caret alignment at non-1.0 scale, and
  quick-loot's `backdrop-filter` + click hit-testing under the scaled `#root` transform. Both were
  built+deployed 2026-07-07; neither has a recorded in-game confirmation since.

## Resolved since the last written status (confirmed via source, not assumed)

- **`IsSanePos` off-screen bound** — fixed 2026-08-07 (commit 5936755, GitHub #19). `ClampExtent` now
  bounds `origin + intrinsic * s`; `RecoverOffscreen` persists the correction at load. **Unbuilt** —
  written without an MSVC toolchain; needs a Windows `xmake` build and an in-game check.
- **`s_diag` ammo diagnostic logger** — removed 2026-08-07 (same commit, GitHub #12). Also unbuilt.
- **Quick-loot rarity source** — decided and implemented 2026-08-07 (f2a6e01, GitHub #13). Value bands
  and legendary are now two independent fields; legendary comes from the engine's
  `ExtraDataList::GetLegendaryMod`, walked over the item's stacks (the legendary mod lives on stack
  extra data, never the base form, which is why base-form reads never saw it). **Unbuilt.**
- **Slot rename (v1 spec Task 3)** — the UI existed but renaming ran through `SaveSlot`, which
  overwrites the slot's layout, so relabelling destroyed it; Escape committed instead of cancelling;
  and `{once:true}` on keydown killed Enter/Escape after the first keystroke. All three fixed
  2026-08-07 (79aaa87, GitHub #17) with a new `RenameSlot`. **Unbuilt.**
- **Quick-loot value/weight** — shipped 2026-08-07 (bbdb5fa, GitHub #6). View-only, no rebuild needed.
- **v1 spec Tasks 2, 2B, 3, 4 — all shipped 2026-08-07.** Preset library (47b83a4, #15), layout
  export/import (3e5c37f, #16), slot rename (79aaa87, #17), per-widget rotation + primary-icon picker
  (1ff5169 / 511ca32, #18). Position schema is now `{x, y, s, r}` plus an optional `icon`. **All
  unbuilt** — they need a Windows `xmake` run.
- **`GetAsyncKeyState` convention** — resolved 2026-08-07 (8325b60, #20): ratified and centralised into
  `InputMods::ShiftDown`. Fixed the real defect, that two of six sites demanded LEFT Shift only.
- `hud_container.html` orphan — deleted.
- `DumpHUDMenuGfx` GFx probe — removed (superseded by the working `HideVanillaAmmo`).
- Grenade widget "can't show a real equipped grenade" — resolved; now reads `equippedItems` and
  compares the exact `WEAPON_TYPE` value (`kGrenade`/`kMine`) instead of the broken bitwise
  `IsThrownWeapon()` (see `reference_tenumset_any_is_bitwise_weapon_type` memory — that overlap bug
  used to misclassify pistols as grenades).
- Quest-item filtering comment/TODO in `QuickLootController.cpp` — the whole filter area was rewritten
  since (container/dead-NPC type filter + lock-state gating); no dangling `As<TESBoundObject>` "filter
  that doesn't filter" remains.
- Editor aspect-locked scale — shipped as the uniform-scale control (Task 1 above).
- Compass jitter/lag — resolved through the 2026-07-07 pt7/pt8 CSS-transition rewrite + tracked-quest
  gating (see Shipped section).

---

## Architecture references

- Full editor design spec (bug fixes 1-7 + editor power tools + widget redesign):
  [specs/2026-06-02-full-editor-design.md](specs/2026-06-02-full-editor-design.md)
- v1 preset/template/scale spec (Task 1 done, Tasks 2-4 not started):
  [specs/2026-07-07-v1-preset-template-scale.md](specs/2026-07-07-v1-preset-template-scale.md)
- Third-party icon licensing: [THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md)
