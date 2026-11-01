# PrismaHUD_F4 — Full In-Game UI Editor Design Spec

**Date:** 2026-06-02
**Scope:** Bug fixes + editor power tools + Clean Modern Dark widget visual redesign
**Approach:** Hybrid — fix bugs minimally, add new features in clean isolated classes, redesign all HTML

---

## 1. C++ Bug Fixes

Seven targeted changes. No structural rewrites to working code.

### 1.1 `m_api` type mismatch
- `HUDLayoutManager::m_api` is declared `IVPrismaUI2*` but `main.cpp` passes `IVPrismaUI4*`.
- **Fix:** Change member type to `IVPrismaUI4*`, update `SetAPI(IVPrismaUI4*)`.

### 1.2 Threading — RegisterJSListener on Ultralight thread
- `moveWidget`, `saveLayout`, `closeEditor` all use `RegisterJSListener` in `OnEditorDomReady`.
- `RegisterJSListener` callbacks fire on the **Ultralight thread**. These callbacks touch `HUDLayoutManager` and `g_api`, which are game-thread state. Same threading bug pattern that caused data races in PrismaJournal.
- **Fix:** Replace all three with `BindUIEvent` (fires on game thread). `CloseEditor()` and `SaveFromJS()` are then safe to call from the callbacks.

### 1.3 BuildEditorPayload missing defaults
- `BuildEditorPayload()` emits `{id, name, x, y, w, h}` but no `defaultX/Y/W/H`.
- The editor JS stores `defaultX = item.x` on init, so "Reset to default" resets to the *current* position, not the actual default.
- **Fix:** Add `defaultX`, `defaultY`, `defaultW`, `defaultH` from `entry.desc` to the payload.

### 1.4 Duplicate listener registration on view reuse
- `CreateEditorView()` detects a surviving view and calls `OnEditorDomReady()` directly, which calls `RegisterJSListener` (now `BindUIEvent`) again — stacking duplicate callbacks.
- **Fix:** Add `bool m_editorListenersRegistered` to `HUDLayoutManager`. `OnEditorDomReady` sets it `true` after registering; skips registration if already `true`. Reset to `false` on `kPostLoadGame` when the view handle is invalidated and recreated.

### 1.5 File I/O under lock
- `SaveFromJS()` calls `SaveLayoutFile()` while holding `m_lock` (unique_lock). File I/O inside a mutex stalls any concurrent `RegisterWidget` call.
- **Fix:** In `SaveFromJS`, snapshot `m_savedPos` under lock, release lock, then write to disk.

### 1.6 `g_editorOpen` data race
- `g_editorOpen` is a plain `bool` written from both the game thread (key handler) and potentially the Ultralight thread (old `RegisterJSListener` path). After fix 1.2, the Ultralight-thread write is gone, but the bool is still shared state.
- **Fix:** Change to `std::atomic<bool>`.

### 1.7 Health bar rads overlay never shows
- `BindHealthUpdate` uses `BuildHealthJSON()` which only returns `{hp, hpMax}`.
- The health HTML expects `d.rads` and `d.radsMax` to render the red overlay.
- **Fix:** Merge rads fields into `BuildHealthJSON()` output: `{hp, hpMax, rads, radsMax}`. The separate rads widget still exists and reads from `BuildRadsJSON()` independently.

---

## 2. Architecture — New Class and Extensions

### 2.1 New class: `HUDEditorState` (`src/HUDEditorState.h/.cpp`)

Owns all editor-session state that `HUDLayoutManager` should not hold:

**Undo stack (JS-only)**
- Undo/redo state is maintained entirely in `hud_editor.html` JS — no C++ involvement.
- JS pushes `{id, x, y, w, h}` to `undoStack` on every `mousedown` that starts a drag or resize.
- `Ctrl+Z` pops from `undoStack`, pushes to `redoStack`, calls `window.moveWidget` with the restored coords to sync C++.
- `Ctrl+Y` pops from `redoStack`, pushes to `undoStack`, calls `window.moveWidget`.
- Max 50 entries. Stack cleared on `saveLayout`.

**Save slots**
- `struct SaveSlot { std::string name; std::unordered_map<std::string, nlohmann::json> layout; };`
- `std::array<SaveSlot, 4> m_slots`. Slot 0 name = "Default".
- Persisted to `Documents/My Games/Fallout4/PrismaHUD/slots.json`.
- `LoadSlot(int index)` returns the slot's layout map. `SaveSlot(int index, std::string name)` snapshots current `m_savedPos` into the slot.

**Widget visibility**
- `std::unordered_map<std::string, bool> m_visibility`. Missing key = visible.
- `SetVisible(id, bool)` / `IsVisible(id)`.
- Persisted under `"visibility"` key in `layout.json`.

### 2.2 `HUDLayoutManager` additions

Three new public methods delegating to `HUDEditorState`:
- `SetWidgetVisible(const char* id, bool visible)` — updates visibility map, calls `m_api->Show/Hide` on matching view, saves to file.
- `GetSlotsPayload() → std::string` — JSON array of `{index, name}` for the editor JS.
- `LoadSlot(int index)` — swaps `m_savedPos` with slot contents, calls `ApplyToView` for all widgets.
- `SaveSlot(int index, const char* name)` — snapshots to slot, writes `slots.json`.

### 2.3 New JS listeners (added in `OnEditorDomReady` via `BindUIEvent`)

| JS event | C++ handler |
|---|---|
| `moveWidget` | `HUDLayoutManager::LiveMove` (push undo first) |
| `saveLayout` | `HUDLayoutManager::SaveFromJS` |
| `closeEditor` | `CloseEditor()` |
| `setWidgetVisible` | `HUDLayoutManager::SetWidgetVisible` |
| `loadSlot` | `HUDLayoutManager::LoadSlot` |
| `saveSlot` | `HUDLayoutManager::SaveSlot` |

On editor open (`OpenEditor`), send two payloads:
1. `initWidgets` — widget array with `defaultX/Y/W/H` and current positions
2. `initSlots` — slot names array

### 2.4 Fractional coordinate storage

All positions stored and saved as **viewport fractions** (0.0–1.0):
- `WidgetEntry::x/y/w/h` are fractions, not pixels.
- `ApplyToView` sends fractions via `setHUDPosition(json)`.
- Each widget HTML multiplies by `window.innerWidth / innerHeight` to get pixel positions.
- `hud_editor.html` works in pixels internally (fraction × viewport) for drag accuracy, converts back to fractions before calling `moveWidget` and `saveLayout`.
- Default values in `WidgetDesc` updated to fractions (e.g. `defaultX = 30.f/1920.f`).
- Old `layout.json` files in absolute pixels are detected by values > 1.0 and auto-migrated on load.

### 2.5 Compass data enrichment

`BuildCompassJSON()` gains two new fields:
- `"location"` — `player->GetParentCell()` → `TESFullName::GetFullName`. Empty string when no name available.
- `"questDir"` — bearing in degrees (0–360) to the active quest marker. Derived from: active quest → first incomplete objective → `BGSQuestMarker` → reference position → bearing from player. Returns `-1.0` when no active quest marker exists (JS hides the blip).

---

## 3. Editor HTML/JS — `hud_editor.html`

### 3.1 Layout

- **Top toolbar** (34px): PRISMAHUD logo, hint text, Snap toggle with size display, Reset Sel, Reset All, Save, Close. No panel toggle in toolbar.
- **Alignment bar** (26px, below toolbar): Align Left/Right/Top/Bottom, H-Center, V-Center, Distribute-H, Distribute-V, Screen Center. Disabled (dimmed) unless 2+ widgets selected.
- **Canvas** (full screen, z-index 10): Fully transparent. Widget drag zones are outline-only, no fill.
- **Properties panel** (210px wide, right edge, top=60px): Glassy background `rgba(8,10,14,0.62)` + `backdrop-filter:blur(12px)`. Collapsible via arrow tab on the **left edge of the panel, vertically centered**. Arrow shows `◀` when open, `▶` when collapsed. Panel slides off-screen with CSS `transform:translateX(210px)` transition.

### 3.2 Drag zones

- One `.wdz` div per widget, positioned using `left/top` in pixels (fraction × viewport).
- On `mousedown` on `.wdz`: push undo state, begin drag. On `mouseup`: emit `moveWidget`.
- On `mousedown` on `.rh` (resize handle): push undo state, begin resize. 8 handles (N/NE/E/SE/S/SW/W/NW).
- Viewport dimensions cached at init and on `resize` event — never read `window.innerWidth/Height` inside `mousemove`.
- Snap grid: snap value (default 10) applied on move/resize. Snap size configurable via toolbar dropdown (off / 5 / 10 / 20).

### 3.3 Properties panel

- **Coord inputs**: X, Y, W, H displayed as fractions (4 decimal places). Editable — typing updates the drag zone and calls `moveWidget` live.
- **Reset default** button: restores `defaultX/Y/W/H` from the widget's stored defaults.
- **Lock button**: sets a `locked` flag on the widget; locked widgets cannot be dragged or resized (drag is ignored).
- **Widget list**: all 9 widgets listed. Eye toggle (filled dot = visible, hollow = hidden) per widget. Click name to select. Selected widget highlighted blue.
- **Save slots**: 4 slots. Active slot shown with blue border. Each slot has Load + Save buttons. Clicking Load calls `loadSlot(index)`. Clicking Save prompts for a name (inline text field replaces the slot name briefly) then calls `saveSlot(index, name)`.

### 3.4 Undo / redo (JS-only)

- `undoStack` and `redoStack` are plain JS arrays of `{id, x, y, w, h}` entries. Max 50.
- Every `mousedown` that starts a drag or resize snapshots the current position onto `undoStack` and clears `redoStack`.
- `Ctrl+Z`: pop from `undoStack`, push current to `redoStack`, call `window.moveWidget(restored)` to sync C++ and update the drag zone.
- `Ctrl+Y`: pop from `redoStack`, push current to `undoStack`, call `window.moveWidget(restored)`.
- Stack cleared when `saveLayout` is called.

### 3.5 Keyboard shortcuts

| Key | Action |
|---|---|
| `Ctrl+Z` | Undo |
| `Ctrl+Y` | Redo |
| `Ctrl+S` | Save layout |
| `Escape` | Close editor |
| `Arrow keys` | Nudge selected widget 1px |
| `Shift+Arrow` | Nudge 10px |
| `Tab` | Cycle selection to next widget |

### 3.6 Alignment tools

All alignment operations act on the currently **selected** widget relative to the **screen** (single selection) or on **all selected widgets** relative to each other (multi-select via `Shift+click`).

| Button | Single-select behaviour | Multi-select behaviour |
|---|---|---|
| Align Left | Snap to left edge (x=0) | Align all to leftmost widget's left edge |
| Align Right | Snap right edge to screen right | Align all to rightmost widget's right edge |
| Align Top | Snap to top (y=0) | Align all to topmost widget's top |
| Align Bottom | Snap bottom to screen bottom | Align all to bottommost bottom |
| H-Center | Center horizontally on screen | Center all on their shared horizontal midpoint |
| V-Center | Center vertically on screen | Center all on shared vertical midpoint |
| Screen Center | Center on screen both axes | — |
| Distribute H | — | Equal horizontal spacing between widgets |
| Distribute V | — | Equal vertical spacing between widgets |

Each alignment emits one `moveWidget` call per affected widget and pushes one undo entry per widget.

---

## 4. Widget HTML Redesign — Clean Modern Dark

All 9 widget files redesigned. Shared design language:

- **Background**: `rgba(12,14,18,0.72)` + `backdrop-filter:blur(10px)`
- **Border**: `1px solid rgba(255,255,255,0.07)`, `border-radius:4px`
- **Accent left bar**: 2px wide `::before` pseudo-element, color varies per widget
- **Typography**: `-apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif`
- **Label**: 8px, 2px letter-spacing, uppercase, `rgba(255,255,255,0.3)`
- **Bars**: 10px tall, `border-radius:5px`, dark track `rgba(0,0,0,0.45)`
- **Numbers**: thin-weight (200–300), `rgba(220,225,240,0.9)`

### Per-widget specs

| Widget | Accent color | Content | Conditional hide |
|---|---|---|---|
| Health | green `rgba(28,180,80,0.6)` | Label + bar (72% fill) + rads red overlay from right + numeric HP | Never hidden |
| Action Points | blue `rgba(33,150,243,0.55)` | Label + bar + numeric AP | Never hidden |
| Radiation | red `rgba(200,40,40,0.5)` | Label + red bar + numeric rads | Hide when rads == 0 |
| Compass | white `rgba(255,255,255,0.18)` | Scrolling tape (major cardinals bold, current centered + white), location name below, yellow quest blip dot+line on tape | Never hidden |
| Ammo | amber `rgba(255,180,50,0.5)` | Large loaded count / reserve, ammo name below in small caps | Hide when no weapon equipped |
| Grenade | orange `rgba(230,100,30,0.55)` | Count + type label + grenade name | Hide when no grenade/mine equipped |
| Quick Loot | — | Full panel (see Section 4a) | Hide when no lootable target in crosshair |
| Survival | purple `rgba(150,80,200,0.5)` | 3 rows: hunger (orange bar), thirst (blue bar), sleep (purple bar), each with icon + bar + value | Hide when survival AVs not found (non-survival game) |
| Interaction Prompt | white `rgba(255,255,255,0.25)` | Key badge + action text + target name | Hide when no crosshair target |

### 4a. Quick Loot Panel (`hud_quickloot.html`)

Replaces the vanilla Scaleform quick loot SWF. Appears when the player's crosshair targets a lootable container or corpse. Fully keyboard-driven — no mouse interaction.

**Visual structure:**
- **Header**: 32×32 icon block (amber box-open for containers, dark red skull with inner glow for corpses) + container/corpse name + "X lbs inside" + item count badge
- **Item list**: keyboard-navigable, `overflow-y:hidden`, programmatic scroll. Bottom fade gradient hints overflow. Each row:
  - Row 1: rarity dot (colored) · category icon (FA Solid: gun/shield/syringe/bullseye/coins) · item name (rarity-tinted) · count · cap value
  - Row 2 (stat line, indented to align under name): icon+value pairs — fire=DMG, shield-halved=DR/DT, gauge=ROF, arrows=RNG, weight-hanging=WT, heart=HP restore. Only stats relevant to the item type shown.
- **Carry weight bar**: icon + "CARRY WEIGHT" label + current/max values + 3px bar (green <80%, amber 80–99%, red ≥100% with current value turning red)
- **Footer**: `[E] Take · [R] Take All` keycaps + total cap value right-aligned. 14px bottom padding, `border-radius:0 0 5px 5px`.

**Rarity system** (derived from item flags/form type, not a Fallout 4 native field — mapped in C++):
- Common = white `rgba(200,205,225,…)` — standard unmodified items
- Uncommon = green — modified items (has ObjectModifications)
- Rare = blue — named/unique items
- Very Rare = purple — named unique with special properties
- Legendary = gold — has legendary effect (`TESObjectWEAP::weaponData.flags` legendary bit, or `ExtraDataList` legendary mod entry)

**Item stat data** (all read game-thread via `BindUIEvent`):
- Weapons: `TESObjectWEAP::attackDamage` (DMG), `weaponData.rangeMax` (RNG), `weaponData.animationAttackMult` (ROF approximation), `GetWeight()`
- Armor: `TESObjectARMO::armorRating` (DR), `TESObjectARMO::data.armorValue` (DT), `GetWeight()`
- Aid: `AlchemyItem` effect list → first `MagicEffect` description string (FX), `GetWeight()`
- Ammo/Misc/Junk: weight only

**Keyboard scheme** (handled via `BindUIEvent` `"navItem"` event from C++ key hooks):
- Up/Down arrows → move selection, `scrollIntoView` on selected element
- E → `window.takeItem(JSON.stringify({id}))` → C++ removes item from container, updates list
- R → `window.takeAll('')` → C++ empties container, closes panel
- Escape / crosshair clears → panel hides

**C++ data feed** (`BuildQuickLootJSON`):
```json
{
  "containerName": "Steamer Trunk",
  "containerType": "container",
  "totalWeight": 22.6,
  "playerCarryWeight": 116.4,
  "playerCarryMax": 200.0,
  "items": [
    { "formId": "0x12345", "name": "Kellogg's Pistol", "count": 1, "value": 312,
      "weight": 4.5, "category": "weap", "rarity": "legendary",
      "dmg": 48, "rng": 35 }
  ]
}
```

Called on `kCrosshairRefChanged`-equivalent: C++ polls `RE::Console::GetPickRef()` on a `BindUIEvent` timer; when the ref changes and is a `CONT` or dead `NPC_`, rebuild and push the JSON. When ref is null or not lootable, call `window.clearLoot('')`.

**File:** `assets/views/hud_quickloot.html` (replaces `hud_container.html`)
**Widget ID:** `prisma_quickloot`
**Default position:** right side, mid-screen (mirroring vanilla quick loot position)

---

### Conditional visibility implementation

Each widget HTML checks its own data in `updateStats` / `updatePrompt` and calls `window.reportVisibility(JSON.stringify({id, visible}))` when the visible state changes. C++ `BindUIEvent` handler for `reportVisibility` calls `m_api->Show/Hide` on the view. This avoids polling and keeps show/hide on the game thread.

### `setHUDPosition` (all widgets)

```js
window.setHUDPosition = function(json) {
  var p = JSON.parse(json);
  // p.x, p.y, p.w, p.h are fractions
  var r = document.getElementById('root');
  r.style.left   = (p.x * window.innerWidth)  + 'px';
  r.style.top    = (p.y * window.innerHeight) + 'px';
  r.style.width  = (p.w * window.innerWidth)  + 'px';
  r.style.height = (p.h * window.innerHeight) + 'px';
};
```

Default positions updated from absolute pixels to fractions in each widget's `DEF` block.

---

## 5. Files Changed

| File | Change |
|---|---|
| `src/HUDLayoutManager.h` | Change `m_api` type; add `m_editorListenersRegistered`; new public methods |
| `src/HUDLayoutManager.cpp` | Fix file I/O under lock; fix payload defaults; fix duplicate registration; new slot/visibility/undo methods |
| `src/HUDDataProvider.cpp` | Merge rads into `BuildHealthJSON`; enrich `BuildCompassJSON` with location + questDir |
| `src/HUDEditorState.h` | New file — undo stack, save slots, visibility map |
| `src/HUDEditorState.cpp` | New file — implementation |
| `src/main.cpp` | Fix `SetAPI` type; fix `g_editorOpen` atomic; send `initSlots` on open; add new BindUIEvent handlers; add `reportVisibility` BindUIEvent in each widget's DomReady callback |
| `src/CMakeLists.txt` | Add `HUDEditorState.cpp` to sources |
| `assets/views/hud_editor.html` | Full rewrite — glassy UI, alignment bar, panel collapse arrow, undo/redo, save slots, visibility toggles, fractional coords, viewport cache |
| `assets/views/hud_health.html` | Redesign — Clean Modern Dark, rads overlay, fractional positions |
| `assets/views/hud_ap.html` | Redesign |
| `assets/views/hud_compass.html` | Redesign — scrolling tape, location, quest blip |
| `assets/views/hud_rads.html` | Redesign — conditional auto-hide |
| `assets/views/hud_ammo.html` | Redesign — conditional auto-hide |
| `assets/views/hud_grenade.html` | Redesign — conditional auto-hide |
| `assets/views/hud_quickloot.html` | New file — replaces hud_container.html; full quick loot panel |
| `src/HUDDataProvider.cpp` | Add `BuildQuickLootJSON`, `BindQuickLootUpdate`, `BindQuickLootNav` (take/takeAll/nav events) |
| `assets/views/hud_survival.html` | Redesign — conditional auto-hide |
| `assets/views/hud_interaction.html` | Redesign — conditional auto-hide |

---

## 6. Out of Scope

- Third-party plugin registration API changes (existing `PrismaHUD_F4_API.h` is unchanged)
- New HUD widget types beyond the existing 9
- In-editor widget creation / deletion
- Animated transitions between save slots
