# Phosphor HUD 0.9.25-ae1 — AE source

Source for the Phosphor HUD F4SE plugin built for Fallout 4 **1.11.240** (Anniversary Edition) and its PrismaUI HUD
page. The playable mod is the AE main download; this archive is optional and only needed if you want to read or build
the code. It is also the corresponding source for the AE DLL, which links libxse/commonlibf4 (GPL-3.0 with the
modding and linking exceptions in `vendor/commonlibf4/EXCEPTIONS`).

## What's in here

| Path | What |
|---|---|
| `src/` | The shared plugin source. `#if defined(PHOSPHOR_AE)` blocks are the AE build. |
| `ae/` | AE runtime gate, the `[Experimental]` troubleshooting switches, the build script (`build.py`), build notes (`SOURCE_BUILD.md`), engine-access notes and offline verification probes. |
| `vendor/` | The exact libxse/commonlibf4 and commonlib-shared trees used, plus spdlog, nlohmann_json and valijson sources/headers. |
| `mod/Phosphor HUD/` | HUD page, MCM page. |
| `schema/`, `examples/`, `tests/`, `tools/` | Message contract, example messages, tests and helper scripts. |

## Building

See `ae/SOURCE_BUILD.md`. One file is not included: put `PrismaUI_F4_API.h` from the PrismaUI F4 download (mod root,
not the `F4SE/plugins` copy) into `third_party/`. It is PrismaUI's file and is not redistributed here.

## License

Phosphor HUD's own code is released under the MIT License (`LICENSE`). Third-party components keep their own
licenses: libxse/commonlibf4 and commonlib-shared are GPL-3.0 with exceptions (see `vendor/commonlibf4/LICENSE` and
`EXCEPTIONS`); others are in `third_party/licenses/` and the `vendor/` trees.
