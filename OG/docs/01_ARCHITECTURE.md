# 01 · Architecture

## The pieces

```
Fallout 4 ──reads──▶ PhosphorHUD.dll (F4SE plugin, to build) ──InteropCall("hudUpdate", json)──▶ hud.html (PrismaUI view, done)
                          │                                                                        ▲
                          ├── reads Phosphor's MCM settings ──────────── {"settings":{…}} ─────────┘
                          └── hides the vanilla HUD widgets it replaces (PrismaUI SuppressHUDWidget)
```

- **`hud.html`** is finished. It renders everything, animates locally (effect timers, clock, compass easing, banners), and applies the hide rules. It has no outgoing calls.
- **The plugin** reads game state, turns it into the messages in `02_MESSAGES.md`, and sends them. It never draws anything.
- **The contract** is the message spec. If the HUD needs a new value, add a field to `hudUpdate` and to the spec in the same change.

## Installed layout

```
Data\
  F4SE\Plugins\PhosphorHUD.dll                     (to build)
  PrismaUI_F4\views\PhosphorHUD\hud.html          (done)
  PrismaUI_F4\views\PhosphorHUD\hud.css           (done)
  PrismaUI_F4\views\PhosphorHUD\hud.js            (done)
  PrismaUI_F4\views\PhosphorHUD\portraits\*.svg   (11 companion faces, from Companion Status UI Remastered)
  PrismaUI_F4\views\PhosphorHUD\figures\*.png     (36 Vault Boy/Girl parts, from Condition Boy and Girl)
  MCM\Config\PhosphorHUD\config.json + settings.ini (done)
```

`mod/Phosphor HUD/` in this package is that layout minus the DLL, ready for Mod Organizer.

## PrismaUI setup

From `third_party/PrismaUI_F4_API.h` (PrismaUI F4 2.1.1, interfaces V1–V12):

```cpp
// F4SE kPostLoad (PrismaUI is loaded by then)
auto* prisma = PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI12>();
if (!prisma) { /* PrismaUI missing or too old: log once, stay disabled */ return; }

// kGameDataReady
g_view = prisma->CreateView("PhosphorHUD/hud.html", OnDomReady);   // relative to Data\PrismaUI_F4\views
prisma->SetViewRole(g_view, PRISMA_UI_API::ViewRole::kWidget);     // passive overlay: never "a panel in the way", never takes input

void OnDomReady(PrismaView view) {
    SendSettings();      // {"settings":{…}}
    g_domReady = true;   // the first snapshot goes out when a game is loaded
}
```

- `InteropCall(view, "hudUpdate", json)` calls the page's global function with one string. Prefer it over `Invoke` (no script parsing).
- Don't `Focus` the view. It's display only.
- `IsAnyPanelVisible(g_view)` (V10) gives `context.prismaFocus` (Gunsmith, Locker, MAP76, PrismaCrafting…).
- `GetButtonPrompt("Activate", buf, n)` (V9) gives the prompt key on the active device.
- `SuppressHUDWidget(className, bool)` (V6) hides vanilla widgets; see `03_DATA_SOURCES.md` → Hiding the vanilla HUD.
- V11 adds `DispatchToGameThread` / `IsGameThread`. Read game data on the game thread; send from there too unless PrismaUI documents `InteropCall` as thread-safe.
- DevTools are on in this install (`PrismaUI_F4.ini` `[DevTools] bEnabled=1`, F12 while a view is visible). The HUD exposes `window.hud.state`, so the live HUD state can be inspected in game.

## Lifecycle

| Moment | Send |
|---|---|
| DOM ready | `settings` |
| New game / save loaded (F4SE `kPostLoadGame`, `kNewGame`) | `{"reset":true}`, then a full snapshot with `"snapshot":true`: vitals, level/xp, location, time, weapon/ammo/explosive, needs/survival, limbs, figure/gear, effects (full list), party, tracker, markers, karma, challenges, weight, context, perk points. |
| Every tick | Only fields whose value changed since the last send (see below). |
| Game events | Immediately, merged into the current tick's message if one is pending. |
| Pause menu closes | Re-read MCM, send `settings`. |

The HUD stays invisible in game until the first `snapshot` (or `hp`) arrives, so nothing shows on the main menu.

## Update loop

One scheduler on the game thread, two rates, one message per tick:

| Tier | Rate | Fields |
|---|---|---|
| Fast | ~30 Hz | `heading`, `target.pct`, `clip`, `mines` (only while non-empty), `stealth.level` (only while sneaking), `context.ads` |
| Slow | ~4 Hz | vitals, `needs`, `survival`, `limbs`, `weight`, `party` hp/state, `markers` distances, `tracker` distance, `effects` resync (every ~5 s) |
| Events | on fire | `notify`, `quest`, `xpGain`, `levelUp`, `subtitle`, `message`, `prompt`, `loot`, `target` (new target), `companionReact`, `legendaryLoot`, `targetMutated`, `illness`/`cure`, `settlementAttack`, `karma.change`, `reputation`, `challengeComplete`, `explosive`, `weapon`, `gear`, `context` (menu open/close), `paused` |

Keep a last-sent copy of every field and only include changed ones; round floats before comparing (fractions to 0.005, degrees to 0.1) so noise doesn't produce traffic. Building the JSON with a small writer (or nlohmann/json) per tick is fine at these sizes.

## Event sinks to register

Exact type names to confirm in CommonLibF4:

- Menu open/close → `context.menus`, `paused`, re-read settings on `PauseMenu` close.
- Container changed (into/out of player) → pickup `notify`, `legendaryLoot.picked`, `weight`, `reserve`, `explosive.count`.
- Equip / unequip on the player → `weapon`, `clip`, `explosive`, `gear`, `weaponLegendary`.
- Death → kills for adrenaline, `legendaryLoot.dropped`, clearing `target`.
- Location change / cell change → `location`, `region`, `faction`, markers refresh.
- Quest stage / objective changes → `quest` banners, `tracker`.
- Level increase / XP gained → `xpGain`, `levelUp`, `perkPoints`.
- Combat state change → `combat`.
- Active effect applied/removed on the player → `effect`, `effectRemove`, `illness`, `cure`.

## Dependencies

- F4SE + Address Library, CommonLibF4 (OG 1.10.163 build). The dev machine has MSVC, CMake, Ninja and vcpkg.
- PrismaUI F4 2.1.1 at runtime (soft dependency: if `RequestPluginAPI` returns null, log once and do nothing).
- The integrations (KARMA, REPUTATION, CHALLENGES, SKK, NAF, LevelUpMenuEx, Condition Boy…) are all soft: look the forms/globals up by plugin name + local FormID at data-ready (most editor IDs aren't kept at runtime), and skip that feature if the plugin isn't loaded.
