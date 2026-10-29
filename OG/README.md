# Phosphor HUD 0.9.24 — OG source

Source for the Phosphor HUD F4SE plugin (Fallout 4 **1.10.163**, OG) and its PrismaUI HUD page. The playable mod is
the main download; this archive is optional and only needed if you want to read or build the code.

## What's in here

| Path | What |
|---|---|
| `src/` | The F4SE plugin (C++23). `Plugin.cpp` lifecycle and message loop, `Live.cpp` game/HUD reads, `Mods.cpp` optional-mod support, `Pipboy.cpp`, `Mines.cpp`, `EngineQuests.cpp` quest markers and map data read from the engine, `Core.cpp` / `ModData.cpp` engine-independent logic. `#if defined(PHOSPHOR_AE)` blocks belong to the separate AE experimental build and are inactive here. |
| `mod/Phosphor HUD/` | HUD page (`PrismaUI_F4/views/PhosphorHUD/hud.html`), MCM page, plugin INI and licenses. Portraits and figures are separate add-ons and are not included. |
| `docs/` | Architecture, the message contract between plugin and page, data sources, settings, build plan. |
| `schema/`, `examples/` | JSON Schema for one HUD message and example messages (also the in-game replay file). |
| `tests/` | Native core tests (`core_tests.cpp`) and browser tests for the HUD page (Python + Playwright). |
| `tools/` | Build, package, test and replay scripts. |

## Building

Requirements: Windows, MSVC 2022 (x64), [xmake](https://xmake.io) 3.x, a CommonLibF4-NG checkout, and the PrismaUI F4
2.1.1 plugin API header.

1. Put `PrismaUI_F4_API.h` from the PrismaUI F4 2.1.1 download (mod root, not the `F4SE/plugins` copy) into
   `third_party/`. It is PrismaUI's file and is not redistributed here.
2. Point `COMMONLIBF4_PATH` at your CommonLibF4-NG checkout.
3. Build: `xmake f -m release -y` then `xmake build PhosphorHUD`. The DLL lands in `build/release/`.
   Core tests: `xmake build phosphor-tests` then run `build/tests/phosphor-tests.exe`.
   `tools/build.ps1 -CommonLib <path>` wraps the same steps.
4. HUD page checks: `pip install playwright jsonschema`, `python -m playwright install chromium`,
   then `python tools/replay.py` (should print "all good") and `python tests/test_hud.py`.

## License

Phosphor HUD's own code is released under the MIT License (`LICENSE`). Third-party components keep their own
licenses: see `third_party/licenses/` and `mod/Phosphor HUD/licenses/`. The PrismaUI header is not included.
