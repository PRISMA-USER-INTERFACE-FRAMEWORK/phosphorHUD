# Project memory

> Condensed notes from past AI-assisted sessions, auto-loaded by [CLAUDE.md](../../CLAUDE.md).
> Moved here from the repo root on 2026-08-07 so all documentation lives under `docs/`.
> Entries are dated snapshots; some describe code that has since been rewritten.

## Update PrismaHUD_F4 to latest Prisma version with xmake
_Condensed from a deleted conversation on 2026-07-27._

**What this was about**
Continuation of PrismaHUD_F4 (Fallout 4 F4SE plugin) development — this segment specifically focused on diagnosing and architecturally fixing compass widget jitter and quest-detection failures, then producing a session handoff document for context-free continuation.

**Decisions made**
- Compass animation must be driven by CSS `transition` (compositor-driven, runs at display refresh rate) rather than JS `requestAnimationFrame` easing, because Ultralight's JS/rAF frame rate is low/uneven and causes visible jitter. This is the standing architecture for `hud_compass.html` going forward.
- `TESQuestTarget::alias` is at offset `0x10`, not `0x00` as originally assumed (verified via Ghidra + CommonLibSSE cross-check) — any future quest-target struct work must use this offset.
- Fallback plan if the CSS-transition fix still doesn't resolve jitter: abandon the custom compass, show the vanilla FO4 compass (native Scaleform) instead, and hide the custom widget via GFx `SetVariable` (same mechanism already used for ammo hide).
- Standing project constraints reaffirmed: no `git push` (local commits only), no em-dashes/en-dashes in authored content, never delete SDK/dependencies without a recovery plan, always build+deploy to the MO2 modlist (`E:\Modlists\Fallen World Alpha 2\mods\PrismaHUD\`) via targeted file copies only, never write to the original (non-copy) Ghidra project.

**What changed**
- `PrismaHUD_F4/assets/views/hud_compass.html` — rewritten: removed rAF JS easing; C++ now pushes heading, JS sets a target CSS transform with `transition:'transform 0.13s linear'`; uses continuous `_disp` heading with shortest-signed delta; seamless re-base logic (`_disp<-90||_disp>450` triggers ±360 shift + forced reflow via `void strip.offsetWidth`); poll interval 80ms; blip uses `left 0.13s linear`; constants `PX_PER_DEG=3.4`, `D_MIN=-180`, `D_MAX=540`, `DEF={xf:750/1920,yf:14/1080,wf:420/1920,hf:24/1080}`.
- `PrismaHUD_F4/src/HUDDataProvider.cpp` — `BuildCompassJSON` got throttled diagnostic logging (every 6th heavy call, ~3s) logging `objectives`, `displayed`, `targets`, `resolved`, `alias0`, `heading`, `questDir`; added `QuestTargetLite` struct with `alias` at verified offset `0x10` (`static_assert(offsetof(QuestTargetLite, alias) == 0x10)`).
- Deployed artifacts: rebuilt DLL (834,048 bytes) + updated `hud_compass.html` copied to `E:\Modlists\Fallen World Alpha 2\mods\PrismaHUD\`.
- `PrismaHUD_F4/xmake.lua` — CommonLibF4 include path corrected from `../../../CommonLibF4` to `../../../../CommonLibF4` (workspace reorg fix).
- Created `PrismaHUD_F4/docs/SESSION_HANDOFF.md` — the key deliverable of this segment, containing: immediate next step (await in-game diagnostic log), full compass change details, fallback plan, list of everything else fixed/deployed this session (vanilla ammo hide, ammo values fix, grenade widget fix, quickloot fix, layout manager fixes, main.cpp `CreateView` guard, astral-plane emoji purge), deferred TODOs, build/deploy reminder, and gotchas.

**Gotchas / findings**
- Astral-plane (4-byte UTF-8, ≥U+10000) characters in any HTML view cause a hard game freeze in Ultralight — must stay BMP-only. This was already found and fixed (`hud_quickloot.html`, `hud_editor.html` emoji purged) but is critical to remember for any future HTML edits.
- Engine save-load freeze (`BSStringPool::Entry::Release` / `BSFixedString` / `VDescTable::GetParam` race) is an ENGINE bug, not a plugin bug — mitigated only by cold-loading from Main Menu; possible additional mitigation is `bHeapAnalysis=false` in `AddictolCrashLogger.toml` (untested).
- GFx/Scaleform manipulation (e.g. hiding vanilla ammo/compass) must run on the main thread via `F4SE::GetTaskInterface()->AddTask()`.
- Compass HUD classes and quest-target functions are not named/present in CommonLibF4 or the Ghidra name DB — required manual struct reverse-engineering.
- The original (packed) Ghidra project gives garbage decompiles; only the corrected/unpacked project (`Fallout4_163_unpacked` at `E:\F4SE OG\Tools\Ghidra\fwgh_proj_1.10.163_unpacked`) should be used for RE queries.

**Open threads**
- Compass fix is unverified in-game. Waiting on the user to cold-load, track a quest, and report the `PrismaHUD compass:` log line plus whether jitter persists. Depending on the diagnostic counters (`displayed=0`, `targets>0/resolved=0`, `alias0` garbage, or all-good), next step is either confirm success, further diagnose the alias/offset resolution, or implement the vanilla-compass fallback.
- Deferred backlog (unchanged, tracked in `docs/TODO_DEFERRED.md` / `CODE_REVIEW_2026-06-30.md`): remove `s_diag` ammo logger, decide rads math formula, quickloot rarity source, off-screen widget bound, editor `GetAsyncKeyState` issue, editor aspect-locked scale + per-widget 90° rotate feature.

## PrismaHUD
_Condensed from a deleted conversation on 2026-07-27._

**What this was about** — Investigating why the "Manage" perk-activation button (e.g. WSFW's workbench perk) is missing in-game, part of the ongoing PrismaHUD_F4/PrismaUI_F4 HUDRollover/HUDQuickContainer suppression saga tracked in `CHECKLIST.md`.

**Decisions made**
- The previously-carried-forward claim that "HUDPlusPlus's sigscan conflicts with the HUDRollover call-site detour" is **false** and must not be cited again without re-verification — HUDPlusPlus is not installed anywhere in the `Fallen World Alpha 2` modlist (no mod folder, no DLL under any `F4SE\Plugins\`, no `HUDPlusPlus.log`).
- Per the KB's own pt7 lesson ("wrong twice already by reasoning ahead of the evidence"), no further static-analysis theorizing on the Manage-button cause is to happen until a live Shift+F9 in-game diagnostic test result is read from `PrismaHUD_F4.log`. This gating rule was written directly into `CHECKLIST.md`.
- The two candidate engine mechanisms for perk-driven activate buttons (`ActivateChoiceListener::PopulateData` indexed choice slots vs. `FavoritesManager::quickContainerMode.eventDataStruct` single `perkButtonText` slot) remain unreconciled; which one actually drives WSFW's "Manage" text is not yet determined and must be settled by runtime evidence, not code-comment assumptions.

**What changed**
- `docs/Prisma/findings/prismahud.md`: removed a stray false HUDPlusPlus reference in the neutral-keycap glyph section; appended a new `## [CORRECTION 2026-07-17] "HUDPlusPlus sigscan conflict" was never a real finding -- HUDPlusPlus is not installed` section documenting the disproof, restoring the real pt1-pt7 history, and noting `SuppressRolloverVisuals` (a previously `[FIXED]`-marked call-site detour retargeting `call ShowRollover` at RVA 0xab18b0→0xab19d0 to `HideRollover` 0xab1c30) is mysteriously absent from current `VanillaUISuppressor.cpp` with no revert explanation.
- `PrismaHUD_F4/CHECKLIST.md`: replaced the 🟡 "HUDRollover / missing Manage button" section body — removed the disproven "disable HUDPlusPlus" task, added the corrected history summary and a task list centered on the pending Shift+F9 live test, with an explicit "no more theories before the log is read" gate.
- No C++ source was modified and nothing was rebuilt this session. No git commit was made for these two doc edits (per CLAUDE.md's commit-after-every-change rule, this is still owed).

**Gotchas / findings**
- `SuppressRolloverVisuals` — a verified, KB-`[FIXED]`-tagged call-site-only fix in `PrismaUI_F4_OG/src/PrismaUI/VanillaUISuppressor.cpp` — does not exist in current source at all; it was reverted in some undocumented prior session, and the comment now in its place (near lines 224-236) repeats an **already-superseded** pt6 claim ("0xab18b0 is not an instruction boundary... do not reintroduce the detour"). The real bug (per pt7) was a buggy verification guard using `REL::Relocation<std::uint8_t*>::get()` which dereferences 8 bytes instead of reading raw instruction bytes — disassembly actually confirmed a real `CALL 0x140ab19d0`. Any reintroduction must use a corrected byte-verification guard (read raw bytes at `site.address()` directly).
- Ghidra headless query tool (`E:\F4SE OG\ReverseEngineering\Scripts\run_query.ps1` / `FW_Query.java`) has **no `xref:` command** — it silently falls back to a substring name search returning 0 matches. `decompile:` requires an exact function name, not a hex address; use `addrdc:0xRVA` instead to resolve+decompile the function containing an address. This blocked finding the actual populator of `QuickContainerStateData`.
- The `Bash` tool's underlying shell is plain `/usr/bin/bash`, not PowerShell — piping `powershell -Command "..."` output into `Select-Object` via `Bash` fails with `Select-Object: command not found`; use the dedicated `PowerShell` tool instead for PowerShell-specific commands.
- Runtime log (`PrismaHUD_F4.log`) confirms our own code actively vtable-suppresses both `HUDQuickContainer` and `HUDRollover` via `CanBeVisible` at every session start (`SuppressHUDWidget('HUDQuickContainer') -> true`, `SuppressHUDWidget('HUDRollover') -> true`), and that the diagnostic Shift+F9 toggle (`RegisterRolloverDiagToggle`, `main.cpp`) is present and registered in the currently deployed build.

**Open threads**
- Awaiting the user's Shift+F9 test result at the workbench (flips HUDRollover suppression live, dumps before/after `QuickContainerStateData`/`perkButtonText` state via `LogQuickContainerState` to `PrismaHUD_F4.log`) — this is the single blocking task.
- Once that evidence is read: either re-implement `SuppressRolloverVisuals` in `PrismaUI_F4_OG/src/PrismaUI/VanillaUISuppressor.cpp` (with the corrected byte-verification guard) if suppression is confirmed to kill the perk data, or pivot investigation if it isn't.
- After a fix: confirm in-game that Manage returns AND the vanilla "E) …" activate prompt stays hidden.
- Uncommitted: the two doc edits (`prismahud.md`, `CHECKLIST.md`) from this session still need a local commit per CLAUDE.md's rule.
- Paused, dependent on this resolution: the FW Locksmith door/container perk-button investigation (same underlying mechanism), and the HUDPlusPlus item-detail-card absorption task (premise needs re-evaluation now that HUDPlusPlus is confirmed not installed, though user said reference/absorption of its code is permitted regardless).
