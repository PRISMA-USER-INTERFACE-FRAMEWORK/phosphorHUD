# PrismaHUD_F4 — Overview

In-game HUD replacement + layout editor for Fallout 4, built on the PrismaUI_F4 (CEF) framework.
Replaces the vanilla Scaleform HUD widgets (health/AP/rads/compass/ammo/grenade/survival/interaction
prompt/quick-loot) with its own HTML/CSS/JS views, plus a live drag-and-drop editor for repositioning
and scaling them.

**AI agent instructions (auto-loaded when working in this folder):** [CLAUDE.md](../CLAUDE.md)
**Current feature/bug status:** [STATUS.md](STATUS.md)
**Live backlog:** [GitHub issues](https://github.com/PRISMA-USER-INTERFACE-FRAMEWORK/PrismaHUD/issues)

## Where things live

| | |
|---|---|
| [knowledge/](knowledge/) | Bug history, root causes, engine facts, code reasoning. **Read the findings file before touching source.** |
| [STATUS.md](STATUS.md) | Rollup of what's shipped and what's genuinely open |
| [specs/](specs/) | Design specs |
| [archive/](archive/) | Superseded documents, kept for history |
| [THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md) | Icon and asset attribution |

The knowledge base was imported into this repo on 2026-08-07; before that it lived only in the
workspace outside version control. See [knowledge/README.md](knowledge/README.md) for what was
imported whole vs. extracted, and the caveats.

---

## Build & Deploy

```powershell
cd "E:\F4SE OG\Projects\Prisma\UI_Systems\PrismaHUD_F4"
xmake
```

**Output:** `build\windows\x64\release\PrismaHUD_F4.dll`
**Deploy target:** standalone MO2 mod `E:\Modlists\Fallen World Alpha 2\mods\PrismaHUD\` (targeted
copies only — never delete/replace the whole mod folder):
- `build\windows\x64\release\PrismaHUD_F4.dll` → `F4SE\Plugins\PrismaHUD_F4.dll`
- `assets\views\*.html` / `*.css` / `*.js` → `PrismaUI_F4\views\`
- `assets\icons\*` → `PrismaUI_F4\icons\` (deployed alongside the views)

CommonLibF4 is the single workspace-root copy (`E:\F4SE OG\CommonLibF4`, xmake); the plugin links it
via `includes("../../../../CommonLibF4")` (4 levels up from this plugin's folder).

---

## Source Layout

```
src/
├── main.cpp                   — F4SE plugin entry, messaging hooks, menu-suppress/sinks, key handling
├── PCH.h                      — precompiled header
├── HUDDataProvider.cpp/h      — reads game state (health/AP/rads/ammo/grenade/survival/compass/prompt),
│                                 feeds the passive HUD views via BindUIEvent
├── HUDLayoutManager.cpp/h     — widget position/scale/visibility persistence + editor plumbing,
│                                 SetMenuSuppressed gate (hides HUD under Crafting/Terminal/Workshop/etc.)
├── QuickLootController.h/.cpp — quick-loot panel: crosshair-pick tracking, lock-state gating, workshop-
│                                 bench transfer-only mode, perk-driven activate-choice surfacing, input
├── KeyHandler.cpp/h           — hotkey handling for editor toggle
├── BindingRegistry.cpp/h      — shared rebindable-keybind system (Feature 2), keybinds.json persistence
├── PrismaHUD_F4_API.h         — this plugin's own public API header (for third-party widget registration)
└── PrismaUI_F4_API.h          — PrismaUI framework API header (synced from the framework repo)
assets/views/                  — one .html + .css + .js per widget (split 2026-07-13; see STATUS.md)
assets/icons/                  — icon assets deployed alongside views
```

## PrismaUI_F4 Integration Timing

| What | When | NOT |
|------|------|-----|
| `RequestPluginAPI<IVPrismaUI...>()` | `kGameDataReady` | `F4SEPlugin_Load` |
| `CreateView()` | `kPostLoadGame` or `kNewGame` | `kGameDataReady` |
| `RegisterJSListener()` / `BindUIEvent()` | Inside `OnDomReadyCallback` | Before DOM ready |
| `Invoke()` / `InteropCall()` | After DOM ready | During load |

Always call `Hide(view)` immediately after `CreateView` unless the widget should be visible at load.
Always call `IsValid(view)` before using a stored handle after a game reload (views can survive or be
recreated across `kPostLoadGame`).

## Widget contract

Every widget HTML implements:
- `window.setHUDPosition(json)` — `{x, y, s}` (viewport-fraction position + uniform scale). Intrinsic
  size lives in the widget's own CSS; `#scaleWrap`/`transform:scale(var(--s,1))` (or `#root` directly
  for widgets predating the split) applies the scale.
- `window.reportPosition(json)` — echoes `{id, x, y, s}` back so the editor stays in sync.
- `window.reportVisibility(json)` — `{id, visible}`, called on any auto-hide state change (health/AP
  are never hidden; ammo/grenade/rads/survival/interaction/quick-loot all conditionally hide).
- `window.setEditorPreview(v)` — force-shows sample content while the layout editor is open, so the
  widget is visible to drag even when its real trigger condition (rads>0, weapon equipped, etc.) isn't met.

Persistence: `Documents\My Games\Fallout4\PrismaHUD\` — `layout.json` (position/scale/visibility per
widget id), `slots.json` (4 save slots), `settings.json`, `keybinds.json` (BindingRegistry). All writes
are atomic (temp-file + rename) and exception-safe. **Zero savegame serialization** — no
`GetSerializationInterface`/co-save, by design (keeps HUD changes mid-game-safe).
