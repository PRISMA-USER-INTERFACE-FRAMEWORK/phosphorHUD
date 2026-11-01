# PrismaHUD

A quick-loot panel, in-game HUD layout editor, widget API and menu replacements for Fallout 4, built on the
[PrismaUI](https://github.com/NomadsReach/framework-F4-Conversion) framework.

The core HUD (health, AP, ammo, compass, notifications, prompts) is provided by Phosphor HUD. PrismaHUD
draws the panels Phosphor does not, in HTML/CSS/JS, so they can be repositioned and restyled without touching a SWF.

## Features

- **Quick-loot widget** - the built-in HUD widget, plus external widgets registered by other plugins
  through the public API.
- **In-game layout editor** - drag to reposition, per-widget uniform scale, snap grid, undo/redo,
  alignment and distribution tools, per-widget visibility toggles, and 4 save slots.
- **Quick-loot panel** - keyboard-driven loot list with per-row 3D model previews, carry-weight bar
  with a projected-weight indicator, value/weight read straight from the engine, lock-state gating
  that matches vanilla, and a Transfer-only view at settlement workbenches.
- **Perk-driven activate choices** - any mod that adds an activate choice through the vanilla perk
  entry point (for example Wasteland Hunter's "Field Dress", or Workshop Framework's "Manage") is
  picked up automatically and rendered in the quick-loot panel. No per-mod patching.
- **Third-party widget API** - other F4SE plugins can register their own HUD widgets and have them
  appear in the layout editor alongside the built-in ones (see `src/PrismaHUD_F4_API.h`).
- **No savegame writes.** All state lives in external JSON under
  `Documents\My Games\Fallout4\PrismaHUD\`, so installing or removing PrismaHUD is save-safe.

## Requirements

- Fallout 4 **1.10.163** (Old-Gen). The vanilla-HUD suppression relies on raw addresses that are
  verified against this build only.
- [F4SE](https://f4se.silverlock.org/)
- The PrismaUI framework (`PrismaUI_F4.dll` + `PrismaUI_F4_Host.dll`)

## Building

Requires [xmake](https://xmake.io) and
[CommonLibF4](https://github.com/Dear-Modding/CommonLibF4).

```
xmake
```

Output: `build\windows\x64\release\PrismaHUD_F4.dll`

## Installing

- `PrismaHUD_F4.dll` -> `Data\F4SE\Plugins\`
- `assets\views\*` (the `.html`, `.css`, and `.js` files) -> `Data\PrismaUI_F4\views\`
- `assets\icons\*` -> `Data\PrismaUI_F4\icons\`

Each view is a separate `.html` + `.css` + `.js` triple; deploy all three files for a view together.

## Licence

PrismaHUD is released under the **Prisma UI License** - see [LICENSE.md](LICENSE.md). It is not MIT;
read it before redistributing.

Bundled icons from [game-icons.net](https://game-icons.net) are used under CC BY 3.0 / CC0, with full
per-author attribution in [THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md).
