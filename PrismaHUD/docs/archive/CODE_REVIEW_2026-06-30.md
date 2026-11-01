# PrismaHUD_F4 — Full Code Review (2026-06-30)

Consolidated from three parallel reviewers (C++ correctness/design, silent-failure/error-handling,
HTML/JS views), cross-checked against the CommonLibF4 fork headers. Line numbers are as of this review;
`[FIXED]` markers are appended as issues are resolved.

Legend: 🔴 critical · 🟠 important · 🟡 medium · ⚪ minor · 🧩 incomplete feature

---

## Fix status (2026-06-30, all built + deployed)

**FIXED:**
- **C1** astral emoji purged from `hud_quickloot.html` + `hud_editor.html` (BMP glyphs `⚔◈✚⌖◆⚙▣↔☠¤` + `●/○`); verified 0 astral chars remain in any view.
- **C2** `equippedItemsLock` (`RE::BSAutoLock`) now held around both the ammo and grenade `equippedItems` loops.
- **C3** stack double-count fixed at all 4 sites: ammo reserve + grenade sum one `stack.count` per callback; QuickLoot `BuildSnapshot` now processes once per item in the `a_filter` (returns false to skip the per-stack callback) → one row per object, correct total.
- **C4** ammo cast now distinguishes null-data (not drawn, silent) from a vtable mismatch (throttled `logger::warn` with actual vs expected vtable → catches offset/runtime drift).
- **I2** `SaveLayoutFile` is exception-safe (no throw escapes the detached thread), serialized via a static mutex, and atomic (temp-file + rename).
- **I3** `MakeView` checks `CreateView`'s return and logs + skips `RegisterWidget` on a 0 handle.
- **I4** corrupt `layout.json` is renamed to `.corrupt` and logged loudly instead of silently reset.
- **I5** `RegisterWidget` evicts any prior entry sharing the same `id` (honors the reload/replace contract; no more leaked stale entries).
- **I6** `GetAV`/`GetPermAV` catch blocks now throttled-log instead of silent 0.
- **I7** QuickLoot `TakeSelected`/`TakeAll` log (throttled) when a take no-ops on a stale container handle.
- **Dead code** removed: the `#if 0 BuildQuickLootJSON` block (114 lines).

**DEFERRED (by design / needs input):**
- **I1** debug scaffolding (`DumpHUDMenuGfx` probe + `s_diag` logger) kept **on purpose** until the probe captures the vanilla-ammo GFx path once; remove both immediately after.
- **I8** `hud_container.html` orphan — left in place (harmless, not created); delete or wire up later.
- Mediums: rads `radsMax=1000`/`hpLost` math (needs the intended formula); QuickLoot rarity still `"common"` (needs a rarity source); `IsSanePos` off-screen bound; editor `GetAsyncKeyState`; quest-item filter. Tracked below.

---

## 🔴 CRITICAL

### C1 — Astral-plane emoji in views → hard-freezes the whole game  `[FIXED 2026-06-30]`
Ultralight hard-freezes FO4 (no crash log) on any 4-byte/astral (≥U+10000) char.
- `assets/views/hud_quickloot.html`: 🔫🛡💉🎯📦🔧🔥📏💀💰 as category/stat/corpse/value icons
  (lines ~152, 161, 163-165, 191, 213; HTML entities `&#x1F4E6;`/`&#x1F4B0;` at ~112, 133). Renders on
  **every container open** — live freeze risk in normal play.
- `assets/views/hud_editor.html`: 🔒/🔓 on the Lock button (~627, 593) — freezes on widget-lock click.
Fix: replace with BMP glyphs or masked PNG icons.

### C2 — `equippedItems` iterated without `equippedItemsLock` (data race / UAF)  `[FIXED 2026-06-30]`
`HUDDataProvider.cpp` ammo (~264) and grenade (~345). Engine guards the array with a `BSSpinLock`
(`MiddleHighProcessData.h:81`, offset 0x280) because equip/unequip mutates it from async/AI/anim
contexts. A reallocation mid-iteration is a use-after-free. Fix: `RE::BSAutoLock` on `equippedItemsLock`.

### C3 — Stack double/over-counting (inflated ammo/grenade counts + duplicate loot rows)  `[FIXED 2026-06-30]`
`HUDDataProvider.cpp:311-319` (ammo reserve), `:361-369` (grenade), `QuickLootController.cpp:71-78` and
`:118-133`. `BGSInventoryList::ForEachStack` already invokes the callback **once per stack**; each
callback *also* walks the whole `nextStack` chain → an item with *k* stacks is summed ~*k(k+1)/2* times,
and quick-loot emits a duplicate row per stack. Inflates reserve counts and duplicates loot entries.
Fix: sum only the single `stack.count` per callback; aggregate quick-loot per object.

### C4 — `catch(...)` cannot catch the memory faults it guards; vtable-mismatch silently dropped
`HUDDataProvider.cpp` raw-offset reads (`QuestTargetLite`, `EquippedWeaponData`). An access violation is
an SEH exception that MSVC `/EHsc` `catch(...)` does not reliably catch → the "safe default" is false
comfort. When the vtable guard mismatches (`:282`) the branch is silently dropped (`dataNull=true`) even
though it means offset/runtime drift (a real bug). Fix: throttled `logger::warn` on the mismatch branch
with the actual vs expected vtable pointer; treat "not drawn" and "guard failed" as distinct messages.

---

## 🟠 IMPORTANT

### I1 — Debug scaffolding ships enabled
`DumpHUDMenuGfx` GFx probe (`HUDDataProvider.cpp:196-234`, trigger `:242-250`; its own comment says
"Remove once the ammo path is known") and the `s_diag` ammo logger (`:291-295`, permanent ~10s spam).
Both in the live ammo path. Keep the probe until it captures the vanilla-ammo GFx path once, then delete
both (or gate behind an INI/`#ifdef` that ships off).

### I2 — Detached save thread is unsafe
`HUDLayoutManager.cpp:335, 419` — `std::thread(...).detach()` writes `layout.json`/`slots.json` with zero
error handling: a throw calls `std::terminate` (CTD, empty log); rapid toggles spawn concurrent writers
that can truncate/corrupt; a write in flight at shutdown is lost. Fix: serialize writes through one
worker, wrap in try/catch, write-temp-then-rename for atomicity.

### I3 — `CreateView` failure never checked
`main.cpp:121` (`MakeView`) registers whatever handle returns, including `0` (invalid). Missing/misnamed
HTML → that widget silently never renders, no log, phantom view-0 pollutes the registry/editor. Fix:
`if (!view) { logger::error(...); return 0; }` and skip `RegisterWidget` on 0.

### I4 — Corrupt `layout.json` silently resets the whole layout
`HUDLayoutManager.cpp:168` abandons the entire file on any parse error → all widgets revert to defaults
with no recovery. Combined with I2, a crash mid-save produces exactly that corrupt file. Fix: rename bad
file to `layout.json.corrupt-<ts>` and log at error.

### I5 — `RegisterWidget` keyed by view handle, not `id` (contract violation, leak)
`HUDLayoutManager.cpp:63`, `m_widgets` keyed by `PrismaView`. API doc (`PrismaHUD_F4_API.h:48-50`)
promises "re-register by id replaces old entry," but reloads with a new handle leak the old entry →
duplicate editor rows for third-party widgets. Fix: erase any existing entry with the same `id` before
insert (or key by id).

### I6 — Safe-default JSON masks real bugs; the change-only push freezes on it
Every `Build*JSON` returns a default (`hp:0/hpMax:1`, `loaded:0`) identical whether the read failed or the
value is genuinely 0. The `BIND` dirty-check only pushes on change, so a builder stuck on its default
never re-signals. `GetAV` catch (`:42,48`) and null `av`/`inventoryList` swallowed silently. Fix:
throttled logs on the *unexpected* branches (keep `!player` silent).

### I7 — QuickLoot take gives no feedback on stale-handle no-op
`QuickLootController.cpp:223-263` — if the container handle went stale between poll and keypress, Activate
silently does nothing (no sound/log). Fix: log the stale-handle case + a UI cue.

### I8 — `hud_container.html` is orphaned dead code
Never created in `main.cpp`, no `BuildContainerJSON`; diverges from the shared contract (reports pixels
not fractions, no `_posSet`/viewport guards). Delete or wire up.

---

## 🟡 MEDIUM / ⚪ MINOR
- Quest-scan `catch(...)` collapses an offset bug into "no marker" and caches it (`HUDDataProvider.cpp:163`) — log it.
- `radsMax` hardcoded `1000` in two places + `hpLost` field misnamed (`:68,184,188`) — rads math unfinalized.
- Quickloot rarity dead: C++ always sends `"common"` (`QuickLootController.cpp:125`); all rarity CSS unreachable.
- Quest-item filter comment overstates: `As<TESBoundObject>` doesn't filter quest items (`QuickLootController.cpp:81`).
- `IsSanePos` allows a widget fully off-screen (no `x+w` bound) — recoverable only by deleting `layout.json`.
- Empty `catch(...){}` editor handlers (`main.cpp:90-111`) → convert to `catch(const std::exception&)` + log.
- Editor toggle uses `GetAsyncKeyState(VK_LSHIFT)` (global state, not game input) — `main.cpp:259`; verify VK `0x7A` vs BS-button-code.
- Compass `DEF.wf=420` vs C++ `600` (cosmetic flash); compass rAF loop never stops when idle; interaction keycap hardcoded `"E"`.
- Dead code: `#if 0` `BuildQuickLootJSON` (~110 lines, `HUDDataProvider.cpp:379-492`) — superseded by QuickLootController.
- `QuickLootInput` `new`'d, never freed (intentional one-time leak — document it); stale header comment claims front-insertion (`QuickLootController.h:94`); `RequestPrismaHUDAPI` ignores version param; PCH `#pragma warning push/pop` wraps nothing.

---

## 🧩 INCOMPLETE / UNFINISHED FEATURES
1. Editor **aspect-locked uniform scale** — NOT implemented (only free 8-handle resize).
2. Editor **per-widget 90° rotate** — NOT implemented (no rotation field/button/transform).
3. **Vanilla ammo SWF hide** — in progress (GFx probe deployed, awaiting the tree-dump log).
4. **Quickloot rarity tiers** — stubbed (UI built, C++ hardcodes `common`).
5. **Grenade widget** can't show a real equipped grenade (not in `equippedItems`; needs another source).
6. **Rads max / `hpLost`** math not finalized.
7. **Quest-item filtering** — promised in a comment, not implemented.
8. **Loaded-ammo accuracy** — still needs in-game confirmation.

---

## ✅ Done well
Handle revalidation on reload; the `EquippedWeaponData` vtable guard; `escH()` HTML-escaping of item/widget
names; the compass `translate3d` single-layer render; survival AV log-once pattern; `RequestPluginAPI` null handled.

## Fix order
1. C1 emoji purge → 2. C3 stack double-count (fixes inflated ammo) → 3. C2 spinlock → 4. I3 CreateView guard
→ 5. I2/I4 layout write safety + corrupt recovery → C4/I6 throttled logs → I5, I7, I8, mediums.
Debug scaffolding (I1) removed after the ammo-path probe fires once.
