# PrismaHUD v1 — Uniform Scale + Preset/Template System (Part B implementation prompt)

> Ready-to-run Claude Code prompt. Execute the tasks **in order**: scale (1) → presets (2) → icon
> picker (2B) → import/export (3) → profiles (4). Each task builds, deploys, and is committed locally
> before the next. Do NOT push.

## Fixed context (do not re-derive)
- PrismaHUD draws its **own CEF widgets** (11: health, ap, rads, survival, compass, ammo, grenade,
  quickloot, prompt builtin; `rad_toxic`/`rad_filter` registered externally by Rad Mod) and vtable-
  suppresses the vanilla ones. FallUI reskins vanilla Scaleform — we implement equivalents in our own
  HTML/CSS, we don't inherit its `.ini` params.
- Persistence: `Documents\My Games\Fallout4\PrismaHUD\` — `layout.json`, `slots.json` (4 slots),
  `settings.json`. Atomic temp-then-rename. **Zero savegame serialization** — keep it that way (mid-game
  safe). No `GetSerializationInterface`/co-save, ever.
- Data path: `HUDDataProvider.cpp` `BIND` macro (requestUpdate → build JSON → dirty-check → InteropCall),
  gated by `HUDLayoutManager::IsMasterVisible()`. Widget position comes from `HUDLayoutManager::ApplyToView`
  → `InteropCall(view,"setHUDPosition",{...})`; each widget HTML implements `window.setHUDPosition`.
- Editor: `assets/views/hud_editor.html` (drag via document-level hit-test, slots, HUD settings). C++
  editor events wired in `main.cpp` `OnEditorDomReady`.
- Part A shipped: `views/icons/game-icons/` (4180 SVG) + `manifest.json` (fetch via
  `prisma://views/icons/game-icons/manifest.json`; ids are `<author>/<name>`; paths prefix `prisma://views/`).
  SVG renders natively; no rasterization. Available to any icon-picker UI added later.
- Build: `cd Projects/Prisma/UI_Systems/PrismaHUD_F4 && xmake`. Deploy DLL →
  `E:\Modlists\Fallen World Alpha 2\mods\PrismaHUD\F4SE\Plugins\`, views →
  `mods\PrismaHUD\PrismaUI_F4\views\`. Commit locally only.

---

## TASK 1 — Uniform per-widget scale (prerequisite; do first)

Replace the current "box-resize stretches flex children" sizing with **true proportional `transform:scale`**.
The confirmed v1 schema is **position + uniform scale + visibility** — free `w/h` is retired from user
control (per-axis bar dimensions are v2 template territory).

### Schema
- `layout.json` per-widget entry becomes `{ "x": <frac>, "y": <frac>, "s": <scale> }` (drop `w`,`h`).
  `s` default `1.0`, sane range ~`[0.4, 2.5]`. On load, entries missing `s` default to `1.0` (positions
  preserved; a one-time size reset — log it, don't silently migrate `w/h`→`s` since w and h could differ).
- `IsSanePos` in `HUDLayoutManager.cpp`: validate `x,y ∈ [-0.1,1.001]`, `s ∈ [0.2,3.0]`.
- `WidgetEntry`: store `x,y,s`. Keep `desc.defaultW/defaultH` (the widget's **intrinsic** size at s=1,
  as viewport fractions) — the editor uses them to draw the box (`box = intrinsic × s`).
- `ApplyToView`: send `{x,y,s}`. `BuildEditorPayload`: send `x,y,s,defaultX,defaultY,defaultW,defaultH`
  (defaults still fractions; editor computes box size = default × VP × s).

### Each of the 11 widget HTMLs
1. Wrap all visible content of `#root` in `<div id="scaleWrap">…</div>`.
2. CSS: `#scaleWrap{transform:scale(var(--s,1));transform-origin:top left}`. Keep `#root` as the
   positioning anchor (position:fixed left/top).
3. Define an intrinsic size constant (the current fixed `#root` px, e.g. rads `200×44`).
4. `window.setHUDPosition(json)` now takes `{x,y,s}`: set `#root` left/top from `x*innerW`,`y*innerH`;
   set `#root` width/height = `INTRINSIC_W*s`,`INTRINSIC_H*s` (so the hit-box matches the scaled visual);
   set `scaleWrap.style.setProperty('--s', s)`. Keep the "retry until laid out" guard added for quick-loot.
5. `reportPosition` sends `{id,x,y,s}`.
6. `setEditorPreview` unchanged (still force-shows sample content).

### Editor (`hud_editor.html`)
- Remove free resize handles; add a **scale control** for the selected widget (a slider + numeric,
  range 0.4–2.5, step 0.05) and expose scale on the drag box (optional single corner "scale handle" that
  changes `s` uniformly). Drag still sets `x,y`.
- `initWidgets`: widget box px size = `defaultW*VP.w*s` × `defaultH*VP.h*s`. Properties panel shows X, Y
  (frac) + Scale. `moveWidget`/`emitMove` payload = `{id,x,y,s}`. `saveLayout` writes `{x,y,s}` per id.
- Reset default sets `s=1` + default x/y.

### ⚠ RUNTIME-VERIFY (do NOT mark done on code inspection — in-game screenshot required)
- **Compass**: `#strip` (tape) is `transform:translate3d`-animated and `.blip`/`.caret` are placed by JS
  pixel math. Put tape+caret+blip **inside** `#scaleWrap`; that math MUST use the **intrinsic** width
  constant, NOT `root.offsetWidth` (root is now scaled). Verify the quest blip still lines up with the
  correct bearing at s=1 AND at s=0.7 in-game.
- **Quick loot**: `#root` uses `backdrop-filter:blur()` and is click-interactive. Applying `transform` to
  a backdrop-filter element is a known Chromium quirk (new stacking context / mis-sampled blur), and
  hit-testing runs through the transform. Verify in-game that (a) the blur still renders and (b) clicking
  loot rows still selects/takes the right item, at a non-1.0 scale.

### Acceptance
Build clean; all 11 widgets scale proportionally (icon + text + bars together, not stretched); editor
scale control persists to `layout.json`; compass blip + quickloot verified in-game.

---

## TASK 2 — Preset library (5 shipped, one-click)

Reuse the apply-all path; presets ship as served JSON and are applied through existing editor events —
no new C++ file I/O needed for presets.

- Ship `views/presets/<name>.json` for each preset + `views/presets/index.json` listing
  `[{file,name,desc}]`. Deploy under `mods\PrismaHUD\PrismaUI_F4\views\presets\`.
- Each preset JSON = full map `{ "<widgetId>": {"x":..,"y":..,"s":..,"visible":bool}, ... }` for all 11.
- Editor gets a **Presets** section: fetch `prisma://views/presets/index.json`, list; on click fetch the
  preset JSON and apply each widget (set x/y/s via the moveWidget path + visibility via setWidgetVisible),
  then `saveLayout`. One click, no manual dragging.
- The 5 presets (author concrete coords; these are design intents):
  1. **Standard** — current defaults, s=1.0.
  2. **Minimal** — s≈0.75, tight bottom-left cluster; non-essential widgets lean on auto-hide.
  3. **Cinematic** — s≈0.7, pushed to screen edges, sparse.
  4. **Ultrawide/21:9** — widgets pulled in from the far horizontal edges (x roughly 0.12–0.88) so nothing
     strands at the sides on 21:9/32:9.
  5. **Tactical/Left-stack** — dense left-column vitals, compass repositioned.
- Presets differ by **position + scale + visibility only** (bar style = v2 templates — out of scope).

### Acceptance
Each preset one-click applies to all 11 widgets and persists; visually distinct in-game (density differs,
not just position). Screenshot-verify at least Standard + Minimal + Cinematic.

---

## TASK 2B — User-selectable HUD icons (icon picker)

The Part A library (`views/icons/game-icons/`, 4180 SVG, `manifest.json`) becomes user-pickable per widget.
Icon choice is a field in the **same** layout entry as x/y/s, so it rides presets/profiles/import-export
automatically (no parallel config).

### Widgets (exactly 7 — the ones with a single replaceable icon element)
`ammo, ap, grenade, health, rads` (builtin) + `rad_toxic, rad_filter` (Rad Mod). **NOT** survival (text
labels), compass/interaction/quick-loot (no single fixed icon). Each renders its icon as a CSS
`mask-image` tinted by `background-color` — swapping the icon changes only the **shape**; the widget keeps
its dynamic color logic (health color, rads reddening, filter green→red). Do NOT touch that color code.

### Schema
- Layout entry becomes `{ x, y, s, icon }`. `icon` = manifest id `"<author>/<name>"`, or absent/null =
  built-in default. Rides `layout.json`, slots, presets, export/import unchanged. `icon` is an optional
  string, validated against the manifest at apply time (see fallback).

### Each of the 7 widget HTMLs
1. Give the icon element a stable id (some already have `.vicon`/`#filterIcon`).
2. On load, capture the built-in default mask string (`_defaultIcon`) — ap/health/rads use inline base64
   data-URI masks, so store the original literal for restore.
3. `window.setIcon(pathOrNull)`: non-null → set `maskImage`/`webkitMaskImage` to
   `url("prisma://views/<path>")` (path from the manifest), leaving `background-color` untouched;
   null/empty → restore `_defaultIcon`.

### C++ (HUDLayoutManager)
- Load `manifest.json` once (parse from `<GameDir>/Data/PrismaUI_F4/views/icons/game-icons/manifest.json`,
  same resolution the scheme handler uses) into an `id → path` map — for validation + the startup sweep.
- Store `icon` per widget entry; include in serialize/load, LoadSlot, SaveFromJS, preset apply, import.
- New editor event `setWidgetIcon` (`{id, iconId|null}`): store on the entry, apply, save.
- On any apply (ApplyToView / preset / import): if `icon` present, validate against the manifest map; valid
  → `InteropCall(view,"setIcon", path)`; invalid → skip (keep default) + log (see fallback logging).

### Picker UI (`hud_editor.html`)
- Selecting an icon-bearing widget shows an **Icon** control (current icon name + Change + Default).
- Change → picker panel: fetch `prisma://views/icons/game-icons/manifest.json`; a search box filters by
  name/author substring; render only the **filtered subset** (windowed/capped, ~120 visible — never 4180
  DOM nodes) as preview `<img src="prisma://views/<path>">` (or mask-tinted swatches) + author label. Pick
  → `setWidgetIcon` + close. Default → `setWidgetIcon({id, iconId:null})`. Author shown per result and on
  the current selection (point-of-use CC BY credit).

### Three validation layers (all required — different failure modes)
1. **Startup existence sweep (C++, packaging check):** after loading the manifest, stat every `path`; log a
   summary + each missing file.
2. **Picker `onerror` logging (JS, serving/mime check):** any preview `<img>` that fails to load logs its
   path via the console callback.
3. **Dev "validate all" (opt-in, exhaustive proof):** an editor dev button / console command that preloads
   every manifest path in **throttled batches** (e.g. 50 at a time, await each batch — respect the
   daily-driver machine; do NOT fire 4180 at once) and logs failures. Run once during the build/test pass.

### Fallback + logging detail (confirmed requirement)
- No user choice → built-in default. Unresolvable saved `icon` → skip override, keep built-in (never an
  empty mask), and **log with triage detail distinguishing the two failure modes**:
  `PrismaHUD: widget '<widgetId>' icon '<iconId>' unresolved — <MISSING_FROM_MANIFEST | MISSING_ON_DISK>; using built-in default`.
  - **MISSING_FROM_MANIFEST** = id not in the parsed manifest → a shared/hand-edited layout referencing an
    icon this install doesn't list → **user's bad layout**.
  - **MISSING_ON_DISK** = id IS in the manifest but failed the existence sweep → **our deploy/packaging is
    broken**.
  That manifest-vs-disk split is the triage signal (user data vs. our build) — do not collapse it to a
  single generic "icon not found".

### Attribution
Existing `THIRD_PARTY_LICENSES.md` (all 36 authors, CC BY 3.0 / CC0) already satisfies the requirement
regardless of which icons are chosen — no new obligation. Add author display in the picker as point-of-use
credit.

### Acceptance
All 7 widgets swap icons from the library and revert to default; the choice persists across reload and
travels in presets/profiles/export-import; the three validation layers run (clean, or every failure
logged); unresolvable ids fall back gracefully with the manifest-vs-disk log split; author shown in
picker. **RUNTIME-VERIFY** the mask swap in-game (shape changes, widget color preserved) — not diff-only.

---

## TASK 3 — Import / export (FallUI folder-drop equivalent)

- **Export**: editor "Export" button → prompt for a name (like slot naming) → C++ writes the current
  layout to `Documents\My Games\Fallout4\PrismaHUD\export\<name>.json` (reuse the `SaveFromJS` serialize
  path; atomic temp+rename). Report the path to the user (JS log/toast).
- **Import**: new `HUDLayoutManager::ListImportFiles()` (enumerate `…\PrismaHUD\import\*.json` names) and
  `ImportLayout(name)` (read + apply via the same map-apply used by presets — which now includes `icon`,
  so imported/exported layouts carry icon choices too, with the Task 2B unresolved-id fallback applied).
  Editor "Import" section lists shipped presets + files found in the import folder; one click applies. This
  is the folder-drop workflow: the user drops a shared `.json` into the import folder, it appears in the list.
- Create the `import\` and `export\` folders on startup so they're discoverable.

### Acceptance
Export produces a file that Import (or another install) applies to the same result. Round-trip verified.

---

## TASK 4 — Profiles relabel (UX only)

- The existing 4 slots are already full save/load layouts. Relabel the editor "SAVE SLOTS" section to
  **Profiles** and fold Presets/Import/Export into one coherent "Layouts" panel (Presets = read-only
  shipped, Profiles = user slots, Import/Export = files). No C++ plumbing change.

### Acceptance
UI reads as Presets / Profiles / Import-Export; existing slot save/load still works.

---

## Global constraints & close-out
- No savegame writes; all state stays in the external JSON files. Atomic writes.
- After each task: `xmake`, deploy DLL + changed views, commit locally (specific files, never `git add -A`,
  never push).
- Update `docs\Prisma\findings\prismahud.md` + `docs\Prisma\CHANGELOG.md` per task.
- **Runtime-verify items (do NOT close on code inspection — pre-registered in `docs\Prisma\findings\open-items.md`):**
  Task 1 compass blip/caret alignment at non-1.0 scale; Task 1 quick-loot backdrop-filter + hit-testing
  under the scaled ancestor; Task 2B icon mask-swap renders in-game (shape changes, widget color preserved)
  and the 3 validation layers run clean or log every failure.
- v2 (per-widget style templates) and v3 (immersive/auto-fade) remain out of scope. The icon picker (2B) is
  IN v1 (it composes with the scale schema + preset/import plumbing, so it ships with them, not later).
