# AE 0.9.22-ae1 source bundle

The companion `PhosphorHUD-AE-experimental-0.9.22-ae1-source.zip` contains the plugin source, HUD/MCM assets, contract, tests, build script, the exact CommonLib source trees used, dependency source archives and the installed dependency headers used by this build. Share it alongside the binary test ZIP. It contains no game files or third-party portrait/figure art.

Dependencies used:

- libxse/commonlibf4: `7c8c6f8a349ac85abf6fc8a7e5a71732d9339954`
- libxse/commonlib-shared: `29fbdb0e2dc548c9ab22f6964981d75090dc9094`, inside commonlibf4/lib
- spdlog 1.17.0, static Release library, MSVC `/MD`, std::format and wchar conversion enabled. This deliberately differs from upstream CommonLib's 1.16.0 build pin.
- nlohmann_json 3.12.0 and valijson 1.1.3 (header-only).
- MSVC 2022 x64, Windows SDK, Python 3. CMake is needed only to rebuild spdlog using the commands below.

Full dependency notices are included in the source trees and runtime ZIP. `SOURCE_MANIFEST.json` records SHA-256 hashes for every source-bundle member.

## Build from an extracted source bundle

Run from the extracted bundle root. No game installation is needed. Install/provide your development tools separately; the scripts do not install dependencies.

1. The spdlog 1.17.0 source is already unpacked at `vendor/spdlog-source/spdlog-1.17.0` (upstream archive: gabime/spdlog v1.17.0 on GitHub).
2. Build its static library and headers to a local prefix (PowerShell):

```powershell
cmake -S vendor/spdlog-source/spdlog-1.17.0 -B build/spdlog -G "Visual Studio 17 2022" -A x64 -DSPDLOG_USE_STD_FORMAT=ON -DSPDLOG_WCHAR_SUPPORT=ON -DSPDLOG_BUILD_SHARED=OFF -DSPDLOG_BUILD_EXAMPLE=OFF -DSPDLOG_BUILD_TESTS=OFF -DSPDLOG_INSTALL=ON -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL
cmake --build build/spdlog --config Release
cmake --install build/spdlog --config Release --prefix vendor/spdlog-built
```

3. Build the AE DLL, adjusting `--vcvars` to your MSVC installation:

```powershell
python ae/build.py --commonlib vendor/commonlibf4 --spdlog vendor/spdlog-built --json vendor/nlohmann_json --valijson vendor/valijson --vcvars "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
```

The output is `build/ae-experimental/PhosphorHUD.dll`. CommonLib, commonlib-shared, and the engine-independent core are built separately from OG. The script records compiler/linker response files and a build log beside the DLL. Do not use `--plugin-only` for a clean build. The source bundle includes all header files used by nlohmann_json and valijson (both MIT/BSD header-only). Their full upstream archives are on GitHub (nlohmann/json v3.12.0, tristanpenman/valijson v1.1.3).

The camera isolation regression can be run with `python ae/verify_camera_context.py --vcvars "<path to vcvars64.bat>"`. It compiles the actual camera-context block with counted stubs for AE and OG, proving that the AE crosshair switch prevents that access. This is an offline boundary test, not an engine test.

Run `python ae/verify_native_hud.py --vcvars "<path to vcvars64.bat>"` for 2,048 switch/request/restoration cases in each of AE and OG modes. It compiles the actual request mask and early-exit code, including initial compass-only hiding and all readers disabled. The stubs stop at the first UI access; they do not simulate Scaleform.

From an MSVC x64 development shell, compile `cl /nologo /std:c++latest /EHsc /Iae ae/options_test.cpp /Fe:build/options-test.exe`, then run `build/options-test.exe "<runtime ZIP extracted>/F4SE/Plugins/PhosphorHUD.ini"`. Assertions must remain enabled. It checks all 15 defaults, each single disable against every switch and ReadIf callback, all-disabled, section parsing, the packaged INI and 48 runtime combinations. Run `python ae/verify_og_source.py` for the reviewed 0.9.22 OG token baseline. The separate AE core can be tested by compiling `tests/core_tests.cpp` with the dependency include directories used in `build/ae-experimental/core/compile.rsp` and linking only `build/ae-experimental/core.lib`.

Run `python ae/verify_companion_markers.py --json vendor/nlohmann_json --vcvars "<path to vcvars64.bat>"` for the 0.9.22 marker checks. It compiles the actual companion-reader entry gate, marker construction and feed-notice condition with stubs: eight bearings/distances, null output, companion/height switches and 20-second notice boundaries in both AE and OG. This checks entry into the first actor-access boundary; it does not run live actor enumeration or the feed service.

This procedure builds the supplied source; compiler versions, timestamps and paths may change the binary hash. The release was built using the recorded cached spdlog library, whose package manifest is included in `vendor/spdlog/manifest.txt`; its complete source archive is supplied for rebuilding. No claim of bit-for-bit reproducibility is made.

The AE DLL accepts only Fallout 4 1.11.240.0 / F4SE 0.7.9 or newer compatible F4SE. Compilation and browser tests do not establish live engine compatibility. See `ae/TESTER_KIT.md` for runtime acceptance.

## Engine quest port verification

`ae/PhosphorHUD.ini` is the canonical experimental INI. Run `ae/verify_engine_quests.py --vcvars <vcvars64.bat>` for the actual reader/probe gates. The normal build compiles `ae/EngineLayoutChecks.h` against the pinned headers. `ae/verify_engine_addresses.py --commonlib <tree> --database <version-1-11-240-0.bin> --output <audit.json>` reproduces the helper-ID audit; the game address database is not redistributed. Engine quest and diagnostic TUs are included in the clean AE build. `bEngineProbe` remains off by default.
