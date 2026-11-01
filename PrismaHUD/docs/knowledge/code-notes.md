# PrismaHUD code notes

> Imported into this repo on 2026-08-07 from the workspace knowledge base
> (`Code Notes/PrismaHUD.md`). Reasoning behind non-obvious code, anchored to file + symbol. Source files stay clean; the explanation lives here.

---

# PrismaHUD — Code Notes

## Controller button prompts — now a framework API (IVPrismaUI9)

As of 2026-07-14 the controller system lives in the **framework** (`PrismaUI_F4_OG`, `PrismaUI::
ControllerGlyphs` + `IVPrismaUI9`), so every Prisma plugin shares one implementation. PrismaHUD is a
consumer: `src/ControlLabels.*` is a thin shim that delegates device tracking, style, and ControlMap
button-prompt resolution to the framework via `GetButtonPrompt`/`IsUsingGamepad`/`GetControllerStyle`,
and keeps only PrismaHUD's own HUD-action→canonical table (take/take_all/transfer/bench_transfer/perk/
select — no other plugin has these). The button art + JS renderer (`window.PrismaCG`, `window.CG` alias)
are shipped by the framework at `views/prisma_controller_glyphs.js` + `views/icons/ControllerButtons/`.
The notes below describe the mechanism, which now lives framework-side.

## Controller/keyboard button labels (mechanism, framework-side)

Every on-screen prompt (interaction badge, quick-loot footer, message-box hints, message-box body
glyphs) resolves its button label so it matches the input device actually in the player's hands.

**Device tracking.** The engine exposes no "last used device" flag — `BSInputDeviceManager` only reports
whether a gamepad is *connected*, which is not the same thing (a pad can sit plugged in all session while
the player uses mouse+keyboard). So `NoteInputDevice` is fed from the `ButtonEvent`/`ThumbstickEvent`
sinks we already run: `QuickLootInput::ShouldHandleEvent` (PlayerControls, the gameplay chain — the one
that sees the pad while you roll over a container) and `KeyHandler::ShouldHandleEvent` (MenuControls, the
menu chain). Mouse counts as keyboard so a KB+M player's mouse motion doesn't flip the prompts.

**Why "Activate" showed "E" on a pad.** `ControlMap::GetMappedKey(..., kGamepad)` returns the raw XInput
mask (A = `0x1000`), but `RE::BS_BUTTON_CODE` tags the same button with the `0x10000` gamepad bit
(`kAButton = 0x11000`). `CanonicalFromCode` normalizes with `code | 0x10000` so either encoding resolves.

**PS vs Xbox — decompiled, not guessed.** `ReverseEngineering/Organized/1.10.163/README-Controllers.md`
decompiled the whole chain (`UIUtils::GetButtonHTMLFromUserEvent` → `ControlMap::GetButtonNameFromUserEvent`
→ `BSInputDeviceManager::GetButtonNameFromID`). The engine requests `kGamepad` unconditionally and never
branches on physical controller brand, so it always emits `Xenon_*` (Xbox) glyphs even for a PS pad. The
`PSN_*` glyphs exist in the font but are never natively selected. So PlayStation labels are *our* re-style
of the same canonical button (`kLabels` xbox/psn columns), gated by `controllerStyle` in settings.json.
PS face symbols are BMP (Cross U+2715, Circle U+25CB, Square U+25A1, Triangle U+25B3) — safe in CEF; the
astral-emoji freeze only affects codepoints > U+FFFF.

## `assets/views/hud_messagebox.js` — `scaleformToHtml` / `GLYPH_CHAR`

The message body is Scaleform HTML: the engine substitutes button-glyph markup into it before we (or the
vanilla SWF) ever see it, e.g. `<font face='$Controller_Buttons_Inverted' size='26'>t</font>`. The char
inside is a **glyph slot, not a letter** — that `t` is the D-pad-left icon, not the T key. `GLYPH_CHAR` is
the verified char→canonical table straight from the decompile doc above (33 entries: `Xenon_*` uppercase,
`PSN_*` lowercase, `DPad_*` shared). We decode char→canonical→styled label and render a real button chip.
An unrecognised char yields an empty (hidden) chip rather than a wrong prompt.

**Documented fallback (not a bug):** if a button name has no glyph-table entry the engine emits plain
`[EventName]` bracketed text. Seeing that in a prompt is expected engine behavior, not a PrismaHUD defect.

## `src/QuickLootController.cpp` — `EnginePerkButtonLabel()` and the dead ActivateChoiceListener path

The rollover's perk button (WH "Field Dress", Workshop Framework "Manage", any mod's Activate-entry-point
perk) is read from `RE::FavoritesManager::quickContainerMode.eventDataStruct.optionalValue->perkButtonText`
— the engine's single quick-container perk slot, populated generically for whatever quick-container target
is active (corpse in `kLoot`, bench in `kWorkshop`). This one `static` read is the whole generic mechanism;
`HUDDataProvider::BuildPromptJSON` (bench prompt) and `BuildSnapshot` (corpse/container panel) both call it,
so it is surfaced on every quick-container target, not just the bench.

The framework's `ActivateChoiceListener`/`GetActivateChoiceLabel` enumeration it replaced is **dead code in
1.10.163**: the class's constructor has zero callers and its `PopulateData` logged zero calls across a full
session (both vtable slots verified at 0x832870/0x8329a0). Do not re-wire anything to it. The engine's own
`HasPerkButton(QuickContainerStateData*)` @ RVA 0xa8e020 gates the same `perkButtonText`/`perkButtonEnabled`
fields, corroborating the read. The press is handled by vanilla (Space / gamepad Y); PrismaHUD renders only
the hint. **Constraint:** one engine slot means one perk button per target — the multi-slot case needs the
opt-in registry below, not this read.

## `src/ActivateChoiceRegistry.{h,cpp}` — `IVPrismaHUD3` RegisterActivateChoice

Mod-agnostic opt-in (mirrors `RegisterWidget`) for the two things the single engine slot can't surface: a
2nd perk button on one target, and custom-UI mods that never populate `QuickContainerStateData`. Holds
`{id,label,icon,cb,ctx}` per choice; `BuildChoicesJSON` feeds the rollover (panel + prompt),
`TriggerByIndex` fires the callback **outside the lock** (a callback may register/unregister). The old
`QuickLootController::TriggerActivateChoice` was repurposed from the dead framework capture to route here;
the JS `activateChoice` listener and QuickLootInput's number keys both call it. Callbacks are plain C
function pointers (ABI-safe). No mod-specific identifiers anywhere — PrismaHUD never inspects who registered.

## `src/QuickLootController.cpp` — `QuickLootInput::OnButtonEvent` gamepad mapping

Quick-loot actions were keyboard-idCode only, so a pad could take an item (via the `Activate` user event)
but not take-all/transfer/scroll. Gamepad handling maps via `BS_BUTTON_CODE`: X = take all, D-pad up/down
= scroll, Y = open full container. At a workshop bench Y is the vanilla `HUDQuickContainer` perk button
(Manage) and must NOT be consumed, so the bench's Transfer is X there instead (`bench_transfer`). The
label side (`ControlLabels`) uses the same `bench_transfer` mapping so hint and handler can't disagree.

## `src/HUDLayoutManager.cpp` — `ClampExtent` / `RecoverOffscreen`

`IsSanePos` only ever bounded the widget ORIGIN (`x`/`y` into roughly `[-0.1, 1.001]`, `s` into
`[0.2, 3.0]`). It never considered the widget's projected extent, so a widget at a legal edge origin with
a large scale had its whole body pushed off-screen, and there was no automatic recovery: the only way back
was deleting `layout.json` wholesale. `ClampExtent` closes that by bounding `origin + intrinsic * s` so at
least `kMinVisible` (25%) of the widget stays on screen on each axis, using `WidgetDesc::defaultW/H` as the
intrinsic size (a viewport fraction; `0` means "unknown", and the axis is left alone).

Applied at two different depths on purpose:

- `ApplyToView` clamps into LOCALS only. It runs under a *shared* lock from `ApplyLayout`, so it must not
  mutate the entry, and it is also the live-drag path (`LiveMove`) where rewriting the stored value would
  fight the editor's own drag box.
- `RecoverOffscreen` is the load-time path (`RegisterWidget`, `LoadAndApply`), where the exclusive lock is
  held. It mutates `WidgetEntry::x/y` *and* writes back into the widget's `m_savedPos` JSON entry, so the
  recovery survives the next `SaveLayoutFile`, and it logs a warning naming the widget and both positions.

An oversized widget (extent wider than the viewport) pins to origin `0` rather than solving an empty range.
Fixes issue #19.

## `src/HUDDataProvider.cpp` — `BuildAmmoJSON` diagnostics

The `s_diag` throttled ammo dump (`eqItems/weapon/ammoForm/loaded/dataNull`, ~10s cadence) is gone: it was
added 2026-06-30 to confirm the equipped-weapon read and outlived its question by weeks. Its removal made
`eqCount` and `dataNull` write-only, so both went with it, and the `if (!base)` branch (whose only body was
`dataNull = true`) folded into the `else if (base)` guard on the vtable-mismatch arm.

The `s_vt` vtable-mismatch warning is deliberately KEPT. It is not a diagnostic but a regression detector:
it only fires when `EquippedItemData`'s runtime vtable stops matching `EquippedWeaponData::VTABLE[0]`, which
means CommonLibF4 offset drift, and without it the ammo read would silently return garbage from
`ammo@0x10` / `ammoCount@0x18`. Fixes issue #12.

## `src/QuickLootController.cpp` — `QLRarity` / `QLIsLegendary`

Rarity and legendary are deliberately TWO fields, not one. `QLRarity` is a caps-value band and nothing
else (FO4 has no per-item rarity field); it lost its old `legendary` parameter, which was hardcoded
`false` at the single call site and, when true, would have returned `"legendary"` and thrown the value
band away. Collapsing them hides a cheap legendary and lets an expensive plain item impersonate one, so
the JSON now carries `rarity` and `legendary` separately and the view renders the band as the name
colour with a star plus gold inset edge on top.

`QLIsLegendary` uses the engine's own `ExtraDataList::GetLegendaryMod` (VariantID 178075/2190180,
declared in CommonLibF4's `RE/E/ExtraDataList.h`) rather than guessing at a keyword. **Constraint:** the
legendary mod lives on the STACK's `extra` (`BGSInventoryItem::Stack::extra` @ 0x18), never on the base
form, which is why the existing base-form reads never saw it — hence the stack walk. A quick-loot row
aggregates every stack of one base object, so any legendary stack marks the whole row; a mixed pile of
plain and legendary of the same base object reads as legendary.

## `src/HUDLayoutManager.cpp` — `RenameSlot` vs `SaveSlot`

`SaveSlot` unconditionally does `m_slots[i].layout = m_savedPos`, so it can never be reused to relabel a
slot — doing so overwrites the layout stored in it. That is exactly what the editor used to do: the only
route to the rename input was the Save button. `RenameSlot` sets the name and persists, touching no
layout, and is bound as the `renameSlot` UI event.

Editor-side, two keyboard bugs went with it (`hud_editor.js`, `attachSlotHandlers`): Escape called
`blur()` and the blur handler *was* the commit path, so Escape saved instead of cancelling; and the
keydown listener carried `{once:true}`, which removed it after the first keystroke, meaning Enter and
Escape only ever worked as the very first key pressed. Both replaced with an explicit `done` latch and
persistent listeners. Fixes issue #17.

## `assets/views/hud_quickloot.js` — `valuePerWeight`

Per-UNIT ratio on purpose: `value` and `weight` in the row JSON are both per-unit, so the number is
count-independent and does not change as a stack is looted. Returns null (row omitted) when weight or
value is 0 — weightless items are infinitely efficient, which is noise, and it keeps `inf` off screen.
No C++ change was needed; the snapshot already carried both fields. Fixes issue #6.

## `src/HUDLayoutManager.cpp` — `SanitizeStem` / `ExportLayout` / `ImportLayout`

Layout export/import (issue #16) writes to `Documents\My Games\Fallout4\PrismaHUD\exports\<stem>.json`,
outside the 4 fixed slots. The stem comes from a **text box in the editor**, so it is untrusted input
that reaches the filesystem, and `SanitizeStem` is a whitelist (`[A-Za-z0-9 _-]`, everything else to
`_`) rather than a blacklist. `../` is only the obvious attack; alternate data streams (`name:stream`),
trailing dots/spaces, and length all matter on Windows too.

**Constraint that is easy to miss:** reserved DOS device names still resolve to the device even with an
extension — `exports\CON.json` is the device, not a file — so character filtering alone is insufficient
and a stem matching CON/PRN/AUX/NUL/COM1-9/LPT1-9 gets an `_` appended. Both export and import run the
same sanitizer, so a crafted name cannot read outside the folder either.

`ImportLayout` drops individual corrupt entries via `IsSanePos` instead of rejecting the whole file (one
bad widget in a shared layout should not lose the other ten), runs each surviving entry through
`RecoverOffscreen`, then persists. `MigrateToFractions` runs first so old absolute-pixel exports import
correctly. Import re-pushes widget state through the new `requestWidgets` event because the editor's
drag boxes are built from `initWidgets`, not read back from the live views.

## `src/QuickLootController.cpp` — `BuildSnapshot` extras: ammoType, components

`TESObjectMISC::componentData` (@0x158) is the scrap list, `BSTTuple<TESForm*, SharedVal>` where
`.second.i` is the count; `BGSComponent` inherits `TESFullName` so names read with the same static call
used elsewhere. Capped at 4 per row and rendered on its own `ql-row3` line. Issue #10.

Weapon ammo type is `TESObjectWEAP::InstanceData::ammo` (@0x068). **Constraint:** like every other read
in `BuildSnapshot` this passes a `nullptr` instance, so it is the BASE form's ammo — a receiver mod that
changes a weapon's caliber will not show. Issue #8 stays open for two fields that are NOT unambiguous
and were deliberately not guessed: "accuracy" (only `accuracyBonus`, an `int8` @0x136, exists, and that
is a bonus, not the Pip-Boy's aim-model-derived accuracy) and "fire rate" (`rangedData->fireSeconds`,
`attackSeconds` @0x0C0 and `speed` @0x0C8 are all plausible; vanilla's display is a derived stat).

## Issue #7 (quest/skill-book/read icons) — why it is NOT header-solvable

Recorded so nobody re-derives it. All three signals need runtime or Ghidra verification:

- **Quest item:** `BGSInventoryItem::IsQuestObject(std::uint32_t a_stackID)` exists (VariantID
  823447/2194076) but takes a stackID, and `BGSInventoryItem::Stack` in this fork has **no id member**
  (`nextStack` 0x10, `extra` 0x18, `count` 0x20, `flags` 0x24). Nothing here produces a stack id.
- **Skill book:** `OBJ_BOOK::teaches` is a **union** of `ActorValueInfo*` / `SpellItem*` / `BGSPerk*`,
  selected by `OBJ_BOOK::flags` — and CommonLibF4 has **no BOOK flag enum for FO4**. The only enum in
  the workspace is CommonLib**SSE**'s (Skyrim). Reusing those bit values is the same cross-game
  assumption that forced Ghidra verification on the compass. A misread dereferences the wrong pointer type.
- **Already read:** no getter. `TESForm` has `AddChange`/`RemoveChange` (setters) and
  `CHANGE_TYPES::kBookRead = 1u << 6`, so the state is in the save's change-form data with no documented
  read path here.

## `src/KeyHandler.h` — `InputMods::ShiftDown`

Every Shift check in the plugin goes through this one function; there is deliberately no second way to
ask. `GetAsyncKeyState` is the **ratified** mechanism (issue #20), not an accident left in place.

The routed `ButtonEvent` stream was the obvious alternative and is worse: `KeyHandler`'s sink lives on
ONE input chain, so a chain that stops delivering (menu open, focus change) leaves a *latched* modifier
bool stale. A stuck "Shift is held" is far worse than a poll that is a frame late, and a poll has no
state to desync. These callbacks only run while the game owns input, so global OS key state cannot pick
up keystrokes meant for another window.

The real bug the issue had buried: of six ad-hoc call sites, four accepted either Shift and two demanded
the **LEFT** one — so right-Shift users could not open the HUD editor (Shift+F11) or the keybind menu
(Shift+F10) at all. `ShiftDown()` accepts either side.

## `src/HUDLayoutManager.cpp` — `kPresets` / `ApplyPreset`

Presets are DERIVED from each widget's registered `WidgetDesc` defaults, never a coordinate table: a
third-party widget registered through the API has defaults this plugin cannot know at compile time, so
a table would misplace or silently skip it. Each preset is "every widget at its own default, at scale
N". These are therefore *arrangement* presets, not curated layouts — a curated layout is better built
as a shipped export file now that #16 exists. Every entry goes through `RecoverOffscreen` so a 1.25x
preset cannot strand a widget off-screen. Issue #15.

## Widget views — rotation, and the three positioning shapes

`{x,y,s}` became `{x,y,s,r}` (issue #18). `IsSanePos` **rejects** a non-right-angle `r` rather than
snapping it, so corruption is dropped like any other bad field. **Constraint:** a quarter turn swaps the
on-screen footprint, so `ClampExtent`, `RecoverOffscreen` and the editor's `bw`/`bh` all swap
`defaultW`/`defaultH` for 90/270 — miss that and a rotated-tall widget is bounded as if still wide.
CSS is `transform:rotate(var(--r,0deg)) scale(var(--s,1))` across all 10 widget views.

**Trap for any future sweep across the widget views:** they look like one shared template and are not.
Three shapes exist — direct `applyPos(xf,yf,s)` (ap, compass, health), `_want` + `flushPos` (ammo,
grenade, interaction, rads, survival, notify, with notify differing in brace spacing), and quickloot's
`_last` + `applyFrac(p)`. A blind find-and-replace matches 5 of 10 and silently skips the rest; patch
per shape with per-file assertions.

The icon-picker half of #18 is unbuilt on purpose: there is no shared "the icon" concept for a picker to
target (each view sets its own icons inline, some have several, some none), and the scope and icon
source are both undecided. Details on the issue.

## `src/HUDLayoutManager.cpp` — `kIcons` / `SetWidgetIcon` / `PushIconToView`

Per-widget primary-icon override (issue #18). The choice list is a **curated 7-icon table in C++**, not
a scan of `assets/icons/game-icons/` — that pack is 4180 SVGs, which is unusable as a picker and would
mean shipping a directory-listing path for something nobody wants to scroll.

`kIcons` doubles as the **security boundary**: `SetWidgetIcon` accepts nothing outside the table, so the
filename that ends up inside a view's CSS `url()` can never be typo- or attacker-supplied, and needs no
sanitising at all (contrast `SanitizeStem` for export names, where a whitelist of characters was needed
because the value is free text). `""` means "the widget's own built-in icon", never "no icon".

**Widgets opt in; the editor does not assume.** A view calls `reportIconSupport` at DOM ready (bound in
main.cpp's shared `DOM_READY` macro), which populates `m_iconCapable`; only then does
`BuildEditorPayload` report `canSetIcon` and the editor draw the picker. Widgets with no single primary
icon — and any third-party widget — simply never show a control that would silently do nothing. Health,
AP and Rads opt in: they were the only three sharing the `<div class="vicon" id="icon">` + `maskImage`
pattern, which is what "a single primary icon" actually means in these views.

`PushIconToView` is deliberately NOT folded into `ApplyToView`: that one runs on every `LiveMove` drag
frame, while the icon only changes on load or an explicit pick. It is called from `ApplyLayout` (each
view's DOM ready, so a saved override survives a reload) and from `SetWidgetIcon`.
