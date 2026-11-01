# PrismaHUD changelog

> Imported into this repo on 2026-08-07 from the workspace knowledge base
> (`Prisma/CHANGELOG.md`). Extracted: 79 of 412 entries in the workspace changelog mention PrismaHUD. The rest belong to other Prisma plugins and were deliberately left out rather than importing unrelated history.

---

## 2026-08-06 -- root CLAUDE.md: history moved here, paths made POSIX

The root `CLAUDE.md` carried its own change history in a footer, which duplicated the job of this
file and had stopped being updated: it read "Last updated: 2026-07-19" while the file's last commit
was 2026-08-05. That history is the three entries below, moved here whole. The footer now records
the date and points at this file, which is what its own last paragraph already said it should do.

Also converted its paths and shell snippets to this checkout, which has been Linux since at least
2026-08-05 while the body still spelled everything `E:\F4SE OG\...` in PowerShell. Workspace paths
are POSIX now and `python brain\search.py` is `python3 brain/search.py`, which is what actually
runs here. What stayed spelled the Windows way stayed on purpose and is labelled: the game's own
`Data\F4SE\Plugins\` and log folder, the MO2 mod tree, and the `.bat` deploy scripts, all of which
live on the box that runs Fallout 4. The MO2 deploy rule gained the `cp`/`rm -r` form beside the
`Copy-Item`/`Remove-Item` one, since the never-delete rule applies the same either way.

Two paths were wrong rather than merely Windows, and were checked against this box rather than
translated: the memory system is at `~/.claude/projects/-media-ciusr-Games-Storage-F4SE-OG/memory/`,
and the beads postinstall script is under `npm root -g`, here
`~/Apps/nodejs/lib/node_modules/@beads/bd/`.

### Moved out of the root CLAUDE.md footer

**Last updated:** 2026-07-19 (later same day): Extended the partial docs-hub reversal below to
the rest of `docs/README.md`'s index in three more passes, each triggered by "check for leftover
docs" — the pattern kept finding real gaps, not just diminishing returns:
1. **F4SE plugins & projects** (non-Prisma) — WastelandHunter, Permadeath, FallenWorldAI,
   FallenWorldCombat, FastTravelRestricted, HouseRules, Rad Mod, PipboyPower, Multiplayer, C#
   Bridge all moved the same way. Discovered mid-pass that several of these (PrismaCrafting,
   PrismaMCM, PrismaDebugger_F4, FalloutChat, PrismaHUD_F4, HouseRules, F4MP-Revival) are
   independently nested repos/submodules with real external remotes — their docs commits landed
   *inside* those repos, not this one; see FILE LOCATIONS QUICK REFERENCE below for which is which.
2. **Tooling section** (Plugin Tool, Patched Data, FO4AnimForge) — same pattern, missed in pass 1
   because it's a separate `docs/README.md` section from "F4SE plugins & projects."
3. **Top-level `docs/superpowers/`** — audited all 29 files individually (not a blind sweep, learned
   from the Prisma superpowers folder having misfiled content). 10 single-project pairs moved into
   their project's own `docs/superpowers/`; one (`modlist-org`) was initially misjudged as
   workspace-wide and corrected on a follow-up sweep once `Tools/Modlist Org/` was noticed to be a
   real buildable project, not just an activity description.

All of this shares one commit-safety lesson worth repeating for whoever picks this up next: **this
live checkout is actively shared by concurrent Claude Code sessions.** Staged-but-uncommitted
changes were repeatedly cleared by others' `git add`/commit cycles mid-task (verified via
`git ls-files`/`git cat-file -e` after every batch, not assumed) — nothing was lost, but expect to
re-verify and sometimes re-stage+recommit small changes more than once. Full narrative in
`FINDINGS-RELOCATE-EXECUTION-LOG-2026-07-19.md` at the workspace root.

**Prior — 2026-07-19 (earlier same day):** Executed a **partial** reversal of the 2026-07-13 docs-hub
centralization (audit/plan at `FINDINGS-RELOCATE-AUDIT-2026-07-19.md` /
`FINDINGS-RELOCATE-PLAN-2026-07-19.md` / `FINDINGS-RELOCATE-EXECUTION-LOG-2026-07-19.md`, workspace
root, R.'s explicit item-by-item approval): the ~11 per-plugin `docs/Prisma/<Plugin>/`
architecture/design-doc folders moved back into each plugin's own `Projects/.../docs/` folder (see
the WORKSPACE LAYOUT tree and FILE LOCATIONS QUICK REFERENCE above). The findings-KB
SESSION START/END workflow above is **unchanged** — `docs/Prisma/findings/`,
`docs/Prisma/CHANGELOG.md`, `docs/Prisma/COMMONLIB_FINDINGS.md`, and `docs/Prisma/Prisma Core/`
all stayed centralized; only the (b)/(c) framework-vs-plugin split from that audit's own reasoning
kept `Prisma Core\` in place (the framework's OG variant is a nested, untracked git repo — moving
docs into it would make them invisible to this repo's `git status`). Also fixed: 4 nested
`CLAUDE.md` files (`FallenWorldAI`, `FallenWorldCombat`, `PrismaDebugger_F4`, `C# Project`) plus a
5th found during this pass (`PrismaHUD_F4`) still citing the already-retired
`Projects/Prisma/docs/findings/` path from before 2026-07-13 — all now point at the current
`docs/Prisma/findings/`.

**Prior — 2026-07-18:** Executed Phase 3 of a full workspace org audit/plan
(`ORG-AUDIT-2026-07-18.md`/`ORG-PLAN-2026-07-18.md` at the workspace root) — corrected several
claims in this file the audit found were wrong relative to the actual filesystem: **`PrismaJournal_F4`
was reported "deleted entirely" but the journal feature was rebuilt from scratch afterward as the
active `PrismaJournal` project** (no `_F4` suffix, xmake/CEF, under heavy current development — see
the Content/UI_Systems bullets above and the CMAKE section); **`FW_PipboyPower` was wrongly called an
"EMPTY SHELL"** — it has real ESP/MCM/Scripts/Sound/spriggit content, it just has no xmake.lua because
it isn't a C++-built DLL (`PrismaPerks_F4` genuinely is empty, that claim was correct); **`Projects/Rad
Mod/tooling/` no longer exists** — record/ESP editing moved to `Tools/PluginEditTool/Patched Data/
Source\` and the Rad Mod section here now points there. Also added `PrismaUI_F4_NG_1.11.221_ISOLATED`
(an active Next-Gen/AE CEF porting track BRAIN.md's generator hadn't picked up — largely untracked,
see `brain/DRIFT.md`'s "no version control" section) to the Framework bucket description. See
`docs/Prisma/CHANGELOG.md` for the full dated history of this file's changes going forward; this
footer holds only the current entry.


## 2026-07-22 (evening) -- Found why Outfit Manager stopped showing up. It was us, from two weeks ago.

Decompiled Outfit Manager's own DLL rather than guessing again (two earlier passes wrongly wrote it
off as a third-party problem).

**What it does:** before opening its menu, Outfit Manager asks the framework "list every UI panel
that's currently loaded", and refuses to open if *any* other panel is on screen and not hidden.

**Why it broke:** that question used to return nothing at all. The "list every panel" function was an
unfinished stub until 2026-07-09, when it was implemented for real. From that day on it correctly
reports every panel -- including the ~14 always-on HUD widgets from PrismaHUD. So Outfit Manager now
always sees "something else is on screen" and quietly refuses, never even getting as far as asking us
to create its window. Nothing in Outfit Manager changed; the ground moved under it.

**The real gap on our side:** we never gave mods a way to tell the difference between a passive
always-on HUD widget and an actual interactive panel. The only question they could ask was "does any
panel exist", which is the wrong question, and the only answer available was misleading.

**Fix:** added a new V10 API so a mod can ask what it actually means:
- a panel can declare itself as a passive widget or a real interactive panel
- `IsAnyPanelVisible()` answers "is another Prisma UI genuinely in the way right now?" -- true only
  if another panel is focused, or declared interactive and visible. HUD widgets never trigger it.
- plus "which panel currently has focus", which we only exposed as a yes/no before

This is a brand-new interface rather than bolting methods onto the old one -- doing that in the past
shifted the numbering and made older mods call the wrong function entirely.

**Honest status: this does NOT fix Outfit Manager today.** It's a third-party mod using the old
question; it has to adopt the new one. We can't correct their logic from our side without making
`IsHidden` lie, which would break every other mod. Their check needs to become `IsAnyPanelVisible()`.

Also cleaned up: three per-panel lookup tables that were never emptied when a panel was destroyed.

Files: `src/PrismaUI_F4_API.h`, `src/API/API.{h,cpp}`, `src/PrismaUI/CefBridge.{h,cpp}`, `src/main.cpp`.
Full technical detail incl. the decompiled gate in `docs/Prisma/findings/prismaui-core.md`.



## 2026-07-18 — Workspace org audit/plan Phase 3: doc corrections + git hygiene (no code changes)

Full pass over `ORG-AUDIT-2026-07-18.md`/`ORG-PLAN-2026-07-18.md` (workspace root). Documentation
and git-hygiene only — no builds, no game interaction, nothing pushed to remote. 8 commits on
`feat/schematic-dedupe`; full per-item log in `ORG-EXECUTION-LOG-2026-07-18.md`.

- **CLAUDE.md corrected**: `PrismaJournal_F4` was reported deleted but the feature was rebuilt from
  scratch as the active `PrismaJournal` (no `_F4` suffix) — under heavy current development, not
  gone; `FW_PipboyPower` was wrongly called an empty shell (has real ESP/MCM/Scripts/Sound content,
  just no C++ build); `Projects\Rad Mod\tooling\` no longer exists (moved to `Tools\PluginEditTool\
  Patched Data\Source\`); added `PrismaUI_F4_NG_1.11.221_ISOLATED` (active NG/AE CEF porting track)
  to the Framework section — BRAIN.md's generator had missed it entirely.
- **`PrismaUI_F4_NG_1.11.221_ISOLATED` tracked** (159 files, commit `420638ca1`) after a file-by-file
  review — excluded a dead vendored Ultralight SDK archive and a broken/orphaned git submodule
  reference (`lib/commonlibf4`, whose `.git` file points at a gitdir that doesn't exist).
- **Ultralight-era doc drift fixed**, verified against live source rather than guessed: `docs/Prisma/
  Prisma Core/{html-views,view-lifecycle}.md` (render thread → main game thread via `AddTask`;
  in-game overlay Inspector → CEF DevTools opens externally, confirmed via `API.cpp`;
  "View Recovery" flagged unconfirmed, no matching code found), `api-reference.md` (one residual
  Ultralight line the 2026-07-09 pass missed), `PrismaDebugger/PERFORMANCE_AUDIT_PROMPT.md`
  (15-17fps Ultralight cap → 60fps CEF `windowless_frame_rate`, verified in `CefHostImpl.cpp`).
- **`docs/Rad Mod/ESP_BUILD_PIPELINE.md`** repointed at the real current script/deploy paths;
  **`docs/Patched Data/{PROGRESS,ESP_BUILD_PIPELINE}.md`** deleted as confirmed stale duplicate
  forks — but `docs/Patched Data/report.md`, which the audit had also flagged as a likely duplicate,
  turned out on inspection to be genuine Fallen World Crafting content and was kept.
  `ReverseEngineering/README.md` + `CLAUDE.md` now document the two previously-unlisted AE-runtime
  Ghidra projects (1.11.191, 1.11.221 — both `kAE` per `CommonLibF4`'s own runtime classifier, not a
  separate NG bucket).
- **`docs/Prisma/findings/README.md`** index gaps fixed (9 unindexed files added); the two
  dead-plugin topic files (`prismacharactercreator.md`, `prismaworkshop.md`) moved to a new
  `findings/archive/` folder mirroring `docs/Prisma/PrismaHUD/archive/`'s existing pattern.
- **Git hygiene**: committed 23 already-correct pending deletions (stale root doc mirror + a stale
  submodule reference); tracked `Projects/Rad Mod`'s 403 untracked real files (~844 MB — GasMask
  asset-authoring tree, audio, shipped ESPs) and `Projects/Blender`'s 13 storm-dome/black-hole
  authoring files, both reviewed for secrets/junk first; removed 27 verified-safe stale
  "Moved→docs/" redirect stubs (7 more found living inside nested repos — F4MP-Revival, FalloutChat,
  PrismaUISkyrim/framework, HouseRules — were deliberately left for those projects' own sessions).
  Regenerated BRAIN.md last, once the filesystem/CLAUDE.md changes had settled (53 projects, was 47).
- **Deliberately not executed** (flagged in the execution log for a human call instead): several
  untracked-therefore-not-git-reversible deletions the plan recommended (`UltraBright 1.4.0`,
  `Tools/Resaver`, root stray log/obj files, `ReverseEngineering/Logs/`, duplicate vendored tool
  checkouts under `Fallout4AITrainer`), the whole `Projects/Plugins/Multiplayer/` untracked tree, and
  anything the audit/plan already marked "needs owner's call" (`highwayman-dashboard`, `Tools/
  Modlist Org`, the orphaned `PrismaMap` folder). Also found but didn't fix: `Projects/Prisma/
  CLAUDE.md` is itself substantially stale (cmake+vcpkg, pre-reorg paths) — not part of this pass's
  scope, flagged for a dedicated follow-up.


## 2026-07-18 (pt3) — Full input/focus review: monitor/GPU/InputGuard/ESC/perf sweep

Requested full review across five dimensions after the ESC/Dock input-leak bug kept recurring in
different shapes. Ran five parallel deep-read audits over the framework and every dependent
plugin (not just log greps). Overlaps significantly with the concurrent portability review below
(pt2) — see `docs/Prisma/findings/prismaui-core.md`'s 2026-07-18 review entry for the
cross-reference.

**Fixed:** `CefBridge::Destroy()` now defensively `Unfocus()`s a still-focused view before
destroying it, closing the whole class of "missed `Unfocus()` before `Destroy()`" bug that
permanently wedges InputMask/MenuCursor/Dock-capture — found two live instances of exactly this
(PrismaPipboy's theme-toggle, PrismaAmazon's `kPreLoadGame` handler), fixed directly in both.
Prisma Dock no longer pops up over a PauseMenu-suppressing plugin (PrismaCrafting) via new
`VanillaUISuppressor::IsMenuSuppressed()`. `FocusMenu`'s shared pause-flag race between concurrent
`Focus()` calls narrowed. `CameraSpike`'s per-frame uncached ini read fixed (matches
`LocalMapSpike`'s pattern). PrismaAmazon's `xmake.lua` had a stale pre-reorg CommonLibF4 include
path, fixed to match sibling plugins.

**Documented as open** (real, confirmed, needs more than this pass — bd issues filed): GPU
adapter/LUID mismatch risk on hybrid laptops, no D3D11 device-lost/reset handling anywhere,
FalloutChat's `InputGuard` silencing every other plugin's hotkey via a global
`BSInputDeviceManager::pollingEnabled` disable, PrismaHUD widgets not scaling with resolution/DPI.

Built + deployed `PrismaUI_F4.dll` and `PrismaUI_F4_Host.dll` to the live modlist.

**Files:** `Framework/PrismaUI_F4_OG/src/{PrismaUI/CefBridge.cpp,PrismaUI/CameraSpike.cpp,
PrismaUI/VanillaUISuppressor.{cpp,h},Menus/FocusMenu/FocusMenu.cpp,main.cpp}`,
`UI_Systems/PrismaPipboy/src/main.cpp`, `UI_Systems/PrismaAmazon/{src/main.cpp,xmake.lua}`.

---


## 2026-07-17 (pt5) — PrismaCrafting stat bar: game-icons SVGs + happiness/pop read off the ref (RE)

Two follow-ups on the settlement stat bar.

**Icons.** Replaced the BMP text glyphs with real game-icons.net SVGs from
`PrismaHUD_F4/assets/icons/game-icons` (CC BY 3.0): sharp-smile / person / meat / water-drop /
lightning-arc / cross-shield / bed. Inlined as `viewBox 0 0 512 512` paths with `fill=currentColor`
so the view stays self-contained and the health classes recolour them. Also removes the last
astral-emoji risk from the bar (paths, not glyphs).

**Authoritative happiness + population (used the RE, not just CommonLib).** Last pass I read all 7 stats
from display globals and left "does GetActorValue work on a plain workshop REFR?" as an open item —
because CommonLib stubs `ActorValueOwner::GetActorValue` to 0. Decompiled it instead:
`TESObjectREFR::GetActorValue(TESObjectREFR*, ActorValueInfo*)` @ **OG RVA 0x4005A0** reads the ref's own
`ActorValueStorage` (`ActorValueStorage::GetModifiers`), and `GetActorValueStorage` @ 0x4180E0 confirms
REFR has that storage. So `main.cpp ReadRefAV` now reads **Happiness (0x00129157)** and **Population
(0x0012723E)** straight off `FindNearestValidWorkshop(player)` — authoritative, can't be stale — with
the global as an OG-gated fallback. The resource five stay on globals (their AVIFs resolve only through
the runtime `BGSDefaultObjectManager`; not in CommonLib's `DEFAULT_OBJECT` enum). bd f4se_og-6zw
narrowed to those five. New rule saved: [[feedback_use_re_not_just_commonlib]].

Plugin rebuilt + deployed (DLL hash verified); no framework change. Files: `src/main.cpp`,
`js/workshop.js`, `css/workshop.css`, `docs/Code Notes/PrismaCrafting.md`, findings.


## 2026-07-16 — Vanilla activate button actually suppresses; all notifications -> Prisma toasts

- **Vanilla activate button (HUDRollover) fixed.** `SuppressHUDWidget('HUDRollover')` had always
  returned `false` (all 8 other widgets returned `true`), so the vanilla "E) <activate text>" prompt was
  never hidden and double-drew next to PrismaHUD's own. Cause: it was special-cased onto a `ShowRollover`
  **call-site** detour whose hardcoded site `0xab18b0` isn't an instruction boundary — it lands
  mid-instruction inside `HUDRollover::UpdateDisplayObject` (`0xab1820`, sizeof=423), so its `E8` was a
  coincidental byte whose target resolved below the module base; the guard refused every time. Fixed by
  deleting the special-case + dead detour (73 lines) and letting it use the same vtable slot-7
  `CanBeVisible` path as every other widget (`0x2d192c8`). Addresses verified against the unpacked
  1.10.163 Ghidra dump. `kShowRollover` (`0xab19d0`) was correct all along — only the call site was junk.
- **All vanilla HUD notifications now render as Prisma toasts** (new `hud_notify` widget). Hooks
  `SendHUDMessage::ShowHUDMessage` — the one funnel Papyrus `Debug.Notification` and engine "X Added."
  messages share — via CommonLibF4's Address-Library **ID**, not a raw RVA (deliberate, given the above).
  Burst-safe: bounded queue drained into one InteropCall per 200ms tick, duplicate coalescing ("xN"),
  DOM capped at 5, one shared expiry sweep, `max-height`/`overflow-wrap` so it can't clip off-screen.
- **NotificationFilter disabled** in the MO2 profile (it bound the same Papyrus native — two plugins
  would fight over the same messages). Its spam filter is absorbed into `PrismaHUD_F4.ini`
  `[Notifications]`.
- Files: framework `VanillaUISuppressor.cpp`; PrismaHUD `NotificationController.{h,cpp}` (new),
  `hud_notify.{html,js,css}` (new), `main.cpp`, `HUDDataProvider.h`, deployed `PrismaHUD_F4.ini` (new).
  Built + deployed (framework DLL and `_Host.dll` together per the ABI rule); committed locally, not pushed.

---


## 2026-07-15 (pt2) — PrismaHUD quick-loot: corpse perk confirmed; fixed M/N/O, added Bug P trace

Corpse-mode perk button confirmed live (WH "Field Dress" on a radroach). Then four follow-up bugs
from the same test:
- **M (panel overlap):** WH's field-dressing menu is its own focused PrismaUI overlay (WH_Native
  bridge). Quick-loot only hid for a visible ContainerMenu, so it rendered underneath. Fixed:
  `ResolveTarget` returns null when the framework's modal `PrismaUI_FocusMenu` is open — generic for
  any focused Prisma overlay, no per-mod code.
- **N (perk placement):** engine perk button moved from the separate `ql-choices` strip into the
  `ql-footer` action row (`renderPerkFooter`). Mod-registered choices stay in `ql-choices`.
- **O (controller Triangle collision):** confirmed Prisma cross-wiring — perk and transfer both
  hardcoded to Y, and `QuickLootInput` ate Y for OpenFullContainer, making Field Dress unreachable on
  a pad. Fixed: when a perk button is present, don't consume Y (let vanilla fire it) and drop the pad
  Transfer hint. Gamepad perk button still hardcoded "Y" pending in-game confirm.
- **P (wrong-item take):** static analysis rules out a race, dead nav path, and JS desync — highlight
  and take are the same `m_sel` by construction. Added diagnostic logging (Take-time index/entry/list
  + poll-to-poll order drift) to capture the real cause from one repro. No fix yet.
- Files: `QuickLootController.{h,cpp}`, `hud_quickloot.{html,js,css}`. Built, deployed, committed to the
  PrismaHUD nested repo (not pushed).


## 2026-07-15 — PrismaHUD: generic perk-button surfacing for ANY mod + RegisterActivateChoice API

Investigated why the rollover didn't pick up Wasteland Hunter's / any mod's perk-activate button on
corpses. Confirmed the framework's `ActivateChoiceListener`/`GetActivateChoiceLabel` enumeration is
DEAD CODE in 1.10.163 (class never instantiated -- zero ctor callers, zero PopulateData calls in a
full session), and that the only working source, `QuickContainerStateData.perkButtonText`, was gated
to the workshop bench.

- **(1) Generic engine perk button.** Dropped the dead enumeration path. Added shared
  `QuickLootController::EnginePerkButtonLabel()` (reads `FavoritesManager` -> perkButtonText) and read
  it for ALL quick-container targets (corpse/container/bench). Any Activate-entry-point perk (WH
  `WH_Perk_Butcher` "Field Dress", Workshop Framework "Manage", any future mod) now surfaces with zero
  per-mod code. Press stays vanilla's (Space / gamepad Y). Documented limitation: one engine slot ->
  a single perk button per target.
- **(2) `IVPrismaHUD3` RegisterActivateChoice opt-in.** Mod-agnostic (mirrors RegisterWidget) escape
  hatch for a 2nd button on one target or custom-UI mods with no Activate perk. New
  `ActivateChoiceRegistry`; rendered in panel + prompt, triggered by number key (panel) / click.
- No mod-specific identifiers anywhere in PrismaHUD (grep-verified). Built, deployed to the live
  modlist, committed to the PrismaHUD nested repo (not pushed).
- Files: `ActivateChoiceRegistry.{h,cpp}` (new), `PrismaHUD_F4_API.h`, `QuickLootController.{h,cpp}`,
  `HUDDataProvider.cpp`, `main.cpp`, `assets/views/hud_quickloot.{js,css}`.
- Runtime-confirm pending: corpse-mode perkButtonText population (user to verify in-game).


## 2026-07-14 (pt7) — Framework + Host rebuilt as a genuine matched pair from cef-migration

Re-fetched the CEF 147 SDK (`setup-cef-sdk.ps1`; the archive was still in `build/cef-cache`, so no
re-download) after the OG/NG git cleanup deleted `build/cef-147.0.14`. With the SDK back, the
`PrismaUI_F4_Host` target -- which is CONDITIONALLY DEFINED on that SDK existing -- became buildable again.

Rebuilt and deployed from ONE commit (`cef-migration` @ `34cd9a8`):
- `PrismaUI_F4.dll` -> `9679500a`
- `PrismaUI_F4_Host.dll` -> `42e18225` (was `8646c927` -- the live Host was from a DIFFERENT build)
- `PrismaUI_F4_CefSubprocess.exe` -> `10105bab`
Old binaries backed up first; no MO2 folder deletions.

Verified: both halves compiled from `kCefHostABIVersion = 7`; live `libcef.dll` hash matches the SDK the
Host was compiled against.

Why it matters for the terminal freeze: `BindViewToGeometry` calls `GetViewSRV`, which on this branch is
REAL (not stubbed -- the root CLAUDE.md is stale on that point) and depends on the Host handing back a
per-view offscreen SRV over the ABI. A Host from a different build supplies none, which is exactly the
`BindViewToGeometry: view [27] has no dedicated offscreen frame yet` warning logged three times
immediately before the render thread blocked inside d3d11 and the game hard-froze on opening a terminal.

Also from that threaddump (119 threads): NOTHING was waiting on any lock added this session -- no
critical-section waits, no `ServiceEngineReads` frames, no PrismaHUD `DataLock`. The HUD serialization
fix and the ModelPreview read queue are both exonerated for the freeze.

---


## 2026-07-14 (pt6) — ABI pair repaired (OG); PrismaPipboy crash-on-open fixed

**Framework ABI (root cause of "crashes every load").** The framework repo was checked out on
`ng-1.11.221-parity`, so every ModelPreview commit and every build shipped that day was an **NG build
deployed into the OG modlist**. The NG tree had an uncommitted `CefHostABI.h` bump
(`kCefHostABIVersion` 7 -> 8, "v8: TickOffscreenFrames"), so the framework asked for ABI 8 and the OG
Host (ABI 7) refused: `[CefBridge] Host rejected ABI version 8`. Rebuilt `PrismaUI_F4.dll` from
`cef-migration` in an isolated worktree (never touching the concurrent session's NG files) and deployed it
next to the untouched OG Host. **Verified in-game:** `[CefBridge] Host ABI resolved (version 7)` +
`CEF path active (Host up; compositing enabled)`.

Note: `build/cef-147.0.14` is now EMPTY — the CEF 147 SDK was removed from disk (gitignored; lost in the
OG/NG git cleanup). The `PrismaUI_F4_Host` target is *conditionally defined* on that SDK existing, so it
silently is not a valid target until `setup-cef-sdk.ps1` re-fetches it (~320MB). The framework DLL builds
fine without it.

**PrismaPipboy crash on Pip-Boy open** (`src/GameData.cpp`, commit `ecc2f4f3`). AV inside
`PrismaPipboy.dll` reading `0xFFFFFFFFFFFFFFFF`, one millisecond after it logged the last Pipboy section
it had enumerated. `PushPipboyTree` iterated `PipboyDataManager::rootObject.memberMap` -- a `BSTHashMap`
the ENGINE inserts into while the Pip-Boy is opening -- from an `F4SE AddTask` thread (a POOL, not the
main thread). The walk raced the engine's inserts and hit a scatter-table sentinel. Neither
`PipboyDataManager` nor `PipboyObject` exposes a lock. Nothing consumed the output (no view JS references
`loadPipboy`), so the walk and its serializer are deleted rather than made safe.

This is a READ race in our DLL, not heap corruption -- a distinct bug from the XAudio2 family, which
remains open. Third plugin found holding the same false belief that `AddTask` runs on the main thread
(after PrismaHUD and PrismaCrafting).

---


## 2026-07-14 (pt5) — Revert the ModelPreview texture change; ModelPreview exonerated for the crash

**PrismaUI_F4 framework** (`src/PrismaUI/ModelPreview.cpp`, commit `b29db03`, reverts `3d7dc65`)

pt3 replaced the engine's `GetTexture()` with our own DDS loader. Both premises were wrong.

The loader cannot work: FO4 ships textures in DX10-format BA2 archives that the engine reads through its
texture streamer, so `BSResourceNiBinaryStream` cannot read a `.dds` out of them. The log showed
`read=false (0 bytes)` on 58 of 59 textures and every preview rendered white. `LoadTextureCached()` had
zero call sites because it CANNOT serve archived textures -- not because someone forgot to wire it up.

And the suspicion behind it is dead: the crash reproduces with ModelPreview doing nothing at all
(`previews=0`, bench never opened). ModelPreview is exonerated. The engine-read marshalling (`6669e92`)
stays, being independently correct.

Also confirmed: the 12:16 crash ran the OLD PrismaHUD binary (plugin loaded 12:03, synchronized DLL
deployed 12:07), so the HUD serialization fix from pt4 has not actually been tested yet.

---


## 2026-07-14 (pt4) — PrismaHUD: the HUD data path was racing; BindUIEvent never pinned to one thread

**PrismaHUD_F4** (`src/HUDDataProvider.{cpp,h}`, `src/QuickLootController.cpp`, commit `4d8658a`)

Investigating the recurring XAudio2 heap-corruption crash, the crash session showed ModelPreview did
nothing at all (`previews=0`, zero model/texture loads, bench never opened) yet the crash still happened
— so the last two fixes, though real, were not this bug. Cross-checking the other always-on plugins:

PrismaWheelMenu is exonerated by its own log (never opened a wheel; `BSInputEnableLayer` never engaged).

PrismaHUD was not. Its code asserts "all callbacks fire on the game thread via BindUIEvent". False: the
framework's own log line, which sits inside the `AddTask` callback, printed six thread ids across 7,467
fires. Those threads are engine threads (they also run co-save serialization, F4SE messages, and menu
sinks), so the allocator is valid — but co-save REVERT ran on tid 7008 and later on tid 24712 in the same
process, and an OS thread id never changes, so F4SE dispatches tasks from a POOL. AddTask gives allocator
safety, not mutual exclusion.

So the `BIND` macro was doing an unsynchronized read-modify-write of a static `std::string` on every
widget poll, `g_lastPrompt` was shared between the poll and the PickRefUpdateEvent sink, and the builders
walked engine containers concurrently. All now serialized under `HUDDataProvider::DataLock()` (recursive;
`InteropCall` stays outside the lock).

Not yet confirmed as the cause of the XAudio2 crash — it is a real bug regardless, and the repro run is
the confirmation. Note the same false assumption underpins PrismaCrafting's `AddTask` listener marshalling.

---


## 2026-07-13 (pt14) — Nested-repo audit: live API key fixed, 8 nested repos had real uncommitted work

**Why:** asked to find more issues after the brain build. Root `git status` first surfaced 27,705
files still tracked at dead pre-2026-06-30/07-13-reorg paths and 484,695 untracked files across
`Projects/`/`Tools/` — far beyond the 25-project scope fixed earlier the same day. **Deliberately
not auto-fixed** (too large to secret-scan/artifact-check safely in one pass, `.git` already 3.1
GB); logged as `bd f4se_og-1b8`, full detail in `findings/environment-setup.md`.

**Fixed — live credential exposure:** `.mcp.json` carries a fal.ai API key and was untracked with
no `.gitignore` entry; one stray `git add -A` would have baked it into local history permanently.
Gitignored. Scanned all other untracked root-tree files for the same pattern — no other hits.

**Fixed — checked all 13 nested repos individually** (root `git status` never shows their state).
8 had real problems:
- `PrismaUI_F4 New Gen` — three subsystems (`Translations`, `TextureOverlay`, `LocalMapSpike`) plus
  `DevToolsConfig.h` existed nowhere in git history; real CefBridge/ModelPreview/API/PapyrusVM
  fixes were uncommitted; its `docs/*.md` showed deleted because they'd moved to `docs/Prisma/
  Prisma Core/` in the root repo without a matching commit here; its own nested `lib/commonlibf4`
  was separately dirty. All committed.
- `F4MP-Revival` — the CMake→xmake migration (CLAUDE.md's CMAKE section never listed it) was
  entirely uncommitted: CMakeLists.txt deleted on disk, xmake.lua untracked. Plus real
  AISuppress/NetworkClient/Server/Validator work claimed "complete" in an earlier session's memory
  but never actually committed. Fixed.
- `PrismaDebugger_F4` and `CommonLibF4` had no `.gitignore` of their own: deploy-output DLL
  mirrors, `.idea/.vscode/.xmake`, and a 165 MB re-fetchable F4SE Address Library data dump were
  all sitting as untracked noise instead of excluded. Gitignored; real config (`package.json`,
  `previsscripts/`) rescued and committed.
- HouseRules, FalloutChat, PrismaHUD_F4, PrismaTerminal_F4, PrismaWheelMenu — smaller real
  uncommitted changes, all committed.

**Left alone on purpose:** `PrismaUI_F4 New Gen` went dirty again mid-audit with file timestamps
inside the same minute as the check — a different session editing it live. Did not touch it, nor
`docs/Prisma/Prisma Core/` or `docs/README.md`, which the same live session had mid-edit.

**Files:** `.gitignore` (root + `CommonLibF4/.gitignore` + new `PrismaDebugger_F4/.gitignore`),
`docs/Prisma/findings/environment-setup.md`, plus commits inside 8 nested repos (see their own
`git log`, not this repo's).

---


## 2026-07-13 (pt12) — Phase C live-verified from real gameplay logs; comment bloat cut

**Phase C moved from "test scripts written" to actually verified**, using the user's real 21:54-21:59
and 22:23 sessions rather than asking for another play session. `PrismaUI_F4.log` + each plugin's own
log + `f4se.log` are genuine live-execution evidence for most of the 7.

Result (full table with log citations now in `CEF_MIGRATION_PLAN.md` Phase C):
- **VERIFIED (3):** PrismaHUD_F4 (11 widget views + editor + keybinds, all DOM-ready, 453+ JS→native
  `requestUpdate` round-trips on one widget alone); PrismaCrafting (view + listeners, vanilla SWF
  suppression, JS `focusin tag=BUTTON` proving real clicks land); PrismaGasMask (dormant/undeployed —
  its Rad Mod `hud_filter.html` stand-in renders fine).
- **PARTIAL (3):** WastelandHunter (loads, Papyrus `WH_Native` bridge registered and all natives bound,
  but its UI was never opened); PrismaLocksmith (loads, engine hook firing, no lock picked);
  PrismaDesignerBridge (WS layer live on 127.0.0.1:8787; browser-visual half unconfirmed).
- **NOT VERIFIED (1):** PulowskiProtocol — DLL loads per `f4se.log` but produced no log at all, i.e.
  never triggered. Highest-value one left, since its `CreateMenuView()` was the bug found in pt13.
- **Bonus:** PrismaTerminal_F4's on-mesh path live-verified anyway (`BindViewToGeometry ... 'Screen:0'
  privateTexture=true`), plus PrismaWheelMenu and ModelPreviewTest.
- **Zero framework errors attributable to any plugin** across the whole session.

No code work is pending on any of the four; each just needs one in-game trigger (open the hunter hub,
pick a lock, activate an unpowered Pulowski shelter, open the designer in a browser).

**Comment bloat removed.** Per direct feedback, the verbose multi-paragraph comment blocks added over
the last several passes were cut down to 2-4 lines each while keeping the load-bearing "why" (the
non-obvious constraints a future reader can't re-derive: the windowed-browser/OSR interaction, the
SINGLETHREADED device hazard, the Referer token leak, the private-network proxy-port exception).
Touched: `ModelPreview.cpp`, `PrismaResourceRequestHandler.cpp`, `LoopbackVideoProxy.cpp`,
`CefHostImpl.cpp`, `DevToolsConfig.h`, `API.cpp`, `Cef/Harness/main.cpp`. All three targets rebuilt
clean and redeployed.


## 2026-07-13 (pt9) — Built the workspace "brain": generated project map + drift report + local search

**Why:** the workspace is ~695k files / 110 GB across 47 buildable projects. CLAUDE.md was the only
orientation layer, it is hand-written, and it had silently drifted from reality.

**Added** (new `brain/` toolchain, stdlib Python only — no pip, no MCP server, no external service):
- `BRAIN.md` (root) — generated entry point. 47 projects, category map, the 7 gotchas that cost
  in-game test cycles. CLAUDE.md now points here first and defers to it on any disagreement.
- `brain/build_brain.py` — scans the tree and regenerates everything below. Parses each `xmake.lua`
  for targets + deploy paths, scrapes source for engine hooks (`REL::ID`, `RE::VTABLE::`,
  `write_vfunc` slots), maps docs to projects, and diffs all of it against CLAUDE.md's claims.
- `brain/PROJECTS.md` — per-project build cmd, deploy target, hooks, docs, VCS state.
- `brain/DRIFT.md` — where CLAUDE.md disagrees with disk. The filesystem wins.
- `brain/KNOWLEDGE.md` — which of the four stores (findings / auto-memory / beads / changelog)
  answers which question. They are complementary, not duplicates.
- `brain/search.py` — BM25 index over 22.6k chunks of docs + first-party source + views + the
  offline CK wiki, in 5 namespaces. `python brain/search.py "cursor focus menu"`. The 55 MB index
  is gitignored (rebuilt in seconds); the generated markdown is committed.

**Found while building it (three real defects, all fixed):**
- **29 of 47 projects have no version control at all** — untracked *and* not gitignored, so they
  exist only on this disk (~33k LOC: FallenWorldAI, FallenWorldCombat, WeaponCond, PrismaMCM,
  PrismaInventory_F4, PrismaAuctionHouse, FO4AnimForge, ...). Same class of silent tracking gap that
  hid PrismaHUD_F4's move for two weeks. Logged `[OPEN][HIGH]`; remediation is per-project.
- **`memsearch@zilliztech` was never wired.** CLAUDE.md claimed it was enabled and that it powered
  the memory system. `.mcp.json` has only `fo4editor`, `blender`, `photoshop`, `fal-ai`. Corrected.
- **CLAUDE.md advertised dead projects.** `PrismaWorkshop` is deleted; `PrismaPerks_F4` and
  `FW_PipboyPower` are empty shells with no `xmake.lua` and no source. Meanwhile ~20 real projects
  (CrashDiag, HouseRules, PrismaWheelMenu, PrismaAmazon, FWKeyboard, FWLocksmith, ...) were never
  mentioned at all. Layout block corrected; the generated list is now authoritative.

**Files:** `BRAIN.md`, `brain/{common,build_brain,search}.py`, `brain/{PROJECTS,KNOWLEDGE,DRIFT}.md`,
`brain/index/projects.json`, `CLAUDE.md`, `.gitignore`, `docs/Prisma/findings/environment-setup.md`,
`docs/Prisma/findings/open-items.md`.

---


## 2026-07-13 (pt6) — `bd` actually fixed; cmake retired workspace-wide (1 migrated, 3 deleted)

**`bd` (beads) is genuinely working now**, not just documented-as-broken. Re-ran its postinstall
script directly (`node scripts/postinstall.js` from `%APPDATA%\npm\node_modules\@beads\bd\`) —
it downloaded and extracted `bd.exe` successfully on the first retry, no code/config changes
needed. Verified `bd version`, `bd ready`, `bd prime`, `bd memories` all work and the database
already has real content (11 ready issues, 5 `bd remember` memories) that was simply unreachable
before. Restored `CLAUDE.md`'s Beads Workflow section to accurate, working guidance (it had
briefly documented `bd` as broken-with-fallback in pt5, one turn earlier).

**cmake is retired workspace-wide — no exceptions, not even "legacy."** Audited every Prisma
plugin folder for `CMakeLists.txt`/`xmake.lua` presence:
- `PrismaHUD_F4`, `PrismaTerminal_F4`, `PrismaAuctionHouse`, `PrismaCrafting` already had both
  files (xmake actually used, cmake inert) — no action needed.
- `PrismaInventory_F4` had ONLY `CMakeLists.txt` — **migrated to xmake**, built clean. Real
  CommonLibF4-fork API porting was needed, not just a build-file translation (this fork differs
  from whatever the original cmake config was written against):
  - `PCH.h`: `F4SE::log`/`F4SE::stl` don't exist in this fork → `namespace logger = spdlog;` +
    manual `SHGetKnownFolderPath(FOLDERID_Documents)` log-path resolution in `main.cpp`
    (`F4SEPlugin_Query`), matching the pattern already proven in `PrismaHUD_F4`.
  - `InventoryReader.cpp`/`ItemPreviewManager.cpp`: old `RE/Bethesda/*.h`/`RE/NetImmerse/*.h`
    paths → this fork's alphabetical `RE/<Letter>/*.h` layout (`RE/B/BSGraphics.h`,
    `RE/B/BSModelDB.h`, `RE/I/Interface3D.h`, `RE/I/IMenu.h`, `RE/N/NiColor.h`).
  - `InventoryReader.cpp`: `item.GetDisplayFullName(0)` was ambiguous between this fork's
    `(uint32_t stackID)` and `(const ExtraDataList*)` overloads — disambiguated to
    `std::uint32_t{0}` (stack ID 0, the plain/base stack — confirmed from the `ForEachStack`
    call context, not guessed).
  - `ItemPreviewManager.cpp`: `RE::BSGraphics::RendererData::GetSingleton()` doesn't exist in this
    fork — the real accessor is the free function `RE::BSGraphics::GetRendererData()`. Its
    `context`/`texture` fields are `REX::W32::ID3D11*` wrappers, needing
    `reinterpret_cast<ID3D11DeviceContext*>`/`reinterpret_cast<ID3D11Resource*>` before passing to
    DirectXTK's `SaveWICTextureToFile` — matched the exact precedent already used in
    `PrismaUI_F4 New Gen`'s own `CefBridge.cpp`/`SensorMain.cpp`, not invented.
  - Also hit and fixed the known "Git Bash mis-detects mingw" xmake platform-detection gotcha —
    `xmake f -c` must run from PowerShell here, not Git Bash.
- `PrismaJournal_F4`, `PrismaShowcase_F4`, `PrismaCharacterCreator_F4` — **deleted entirely**, per
  explicit instruction after flagging what each actually contained first. All three were
  cmake-only, untracked in git (no recovery via history), combined ~4.3GB (mostly vendored
  build/vendor cruft — `PrismaCharacterCreator_F4` alone vendored bodyslide-and-outfit-studio,
  F4EE-patched, and a stripped F4SEPlugins-original/f4ee copy; first-party `src/` was small in
  both, 109KB/21 files and 25KB/5 files respectively). `PrismaJournal_F4` specifically was flagged
  as not just stale-build but **already non-functional**: its `DDSHelper.cpp` calls real
  Ultralight-only GPU-image-source APIs (`ultralight::Platform::instance().gpu_driver()`,
  `ultralight::ImageSource::CreateFromTexture`) from `main.cpp`'s
  `InitializeCustomImageSources()` to render its journal background — a real, called feature with
  no CEF equivalent, not a dead code path. `PrismaUISkyrim`'s cmake framework was left alone
  (different game/toolchain, out of scope for this Fallout-4-workspace rule).

Updated `CLAUDE.md`: workspace layout tree (removed the 3 deleted plugins from the Content/ list),
replaced the "CMAKE PATTERNS (legacy)" section with a "CMAKE — RETIRED, NEVER USE" section
documenting the above per-plugin status, removed the now-dead "cmake + MSVC libcpmt.lib" pitfall
entry, fixed a stale "a few plugins still carry a legacy CMakeLists.txt" line in BUILD & TEST.

Files: `Projects/Prisma/UI_Systems/PrismaInventory_F4/{xmake.lua (new), src/PCH.h, src/main.cpp,
src/InventoryReader.cpp, src/ItemPreviewManager.cpp}` (built + verified), `Projects/Prisma/Content/
{PrismaJournal_F4,PrismaShowcase_F4,PrismaCharacterCreator_F4}/` (deleted), `E:\F4SE OG\CLAUDE.md`.


## 2026-07-13 (pt4) — `Projects\Prisma\docs\` retired entirely; this file itself moved

Per explicit instruction, `Projects\Prisma\docs\` (the last remaining stub-filled directory from
pt2/pt3's cleanup) was deleted or merged into `E:\F4SE OG\docs\Prisma\` in full:

- **`findings\`** (30 topic files, whole folder) → `docs\Prisma\findings\`, moved intact as a unit
  to preserve every file's relative cross-links.
- **This `CHANGELOG.md`** (the file you're reading) → `docs\Prisma\CHANGELOG.md`.
- **`COMMONLIB_FINDINGS.md`** → `docs\Prisma\COMMONLIB_FINDINGS.md`.
- **`CEF_MIGRATION_PLAN.md`**, **`PHASE_BC_TEST_SCRIPTS.md`** → `docs\Prisma\Prisma Core\` (they're
  framework-specific, and Prisma Core already links to them).
- **`PrismaCrafting-Workshop-Placement-HANDOFF.md`**, **`-Rebuild-DesignNote.md`** →
  `docs\Prisma\PrismaCrafting\`.
- **`CHANGELOG-2026-06-08-RadOverhaul.md`** (a stray Rad Mod-specific changelog that had been
  sitting under Prisma the whole time) → `Projects\Rad Mod\docs\CHANGELOG-2026-06-08.md`, its
  actual project.
- **`superpowers\`** (skill-generated plans/specs spanning multiple plugins) → `docs\Prisma\superpowers\`.
- Deleted outright (all were already-retired pointer stubs or fully superseded from pt2/pt3):
  every `BUILD*.md` stub, `2026-06-05-PrismaCrafting-design.md`, `3D_RENDER_TARGET_GUIDE.md`,
  `MESHRENDER_FO4_PORT.md`, `PROGRESS.md`, `FINDINGS.md` (its whole job was pointing at `findings\`,
  redundant now that `findings\README.md` is directly discoverable), `CHANGELOG_collapsible.md`
  (an old alternate format, superseded), and this directory's own `README.md`.
- Directory removed entirely once empty.

**Fixed every cross-reference this created**, rather than leaving dangling links: `CLAUDE.md`
(SESSION START #1/#2, SESSION END #1/#4, the workspace layout tree, the Quick Command Reference
file-locations block, the PrismaHUD pointer link, the footer changelog) — plus, per explicit
request, added a standing rule to SESSION END #4 that `CHANGELOG.md` must be updated whenever
major work is done, not only when a session is wrapping up. Also fixed now-broken relative links
in `docs\Prisma\Prisma Core\{README,ARCHITECTURE,issues}.md`, `docs\Prisma\PrismaCrafting\
{README,KNOWN_ISSUES,ARCHITECTURE}.md`, `docs\Prisma\PrismaHUD\{README,STATUS}.md`, and one
internal reference inside `docs\Prisma\findings\prismacraft.md` itself. Historical CHANGELOG
entries that mention the old `Projects\Prisma\docs\...` paths were deliberately left as-is — a
changelog records what was true at the time, not a live index.

**Also, per explicit feedback**, simplified the PrismaUI_F4 framework's own public-facing
`README.md` (`Projects\Prisma\Framework\PrismaUI_F4 New Gen\README.md`) to drop the internal
workspace-reorg narrative (the "moved to the workspace docs hub", "Note on the external release
repo" commentary) that a GitHub reader of that repo doesn't need — it now just states what the
framework does and defers to its own `docs/` folder, without explaining our local doc-management
decisions.

Files: `docs\Prisma\{CHANGELOG.md,COMMONLIB_FINDINGS.md,findings\*,superpowers\*}` (moved),
`docs\Prisma\Prisma Core\{CEF_MIGRATION_PLAN.md,PHASE_BC_TEST_SCRIPTS.md}` (moved),
`docs\Prisma\PrismaCrafting\PrismaCrafting-Workshop-*.md` (moved), `Projects\Rad Mod\docs\
CHANGELOG-2026-06-08.md` (moved), `CLAUDE.md` (paths fixed + new changelog rule), several
`docs\Prisma\**\README.md`/`ARCHITECTURE.md`/`issues.md`/`STATUS.md`/`KNOWN_ISSUES.md` (relative
links fixed), `Projects\Prisma\Framework\PrismaUI_F4 New Gen\README.md` (simplified). No code
changes this entry.


## 2026-07-13 — PrismaHUD views: split into separate HTML/CSS/JS files + modernized JS syntax

All 11 PrismaHUD_F4 views (hud_ammo, hud_ap, hud_compass, hud_editor, hud_grenade, hud_health,
hud_interaction, hud_keybinds, hud_quickloot, hud_rads, hud_survival) were single files with inline
<style>/<script>. Split each into <name>.html (structure + <link rel="stylesheet">/<script src>) +
<name>.css + <name>.js, and modernized the JS.

- Verified relative-path resolution before touching anything: the framework serves views over a custom
  `prisma://views/...` CEF scheme (PrismaSchemeHandler.cpp), which already maps .css -> text/css and
  .js -> text/javascript correctly, and resolves sibling files the same as any normal website (already
  proven by the existing icons/*.png relative <img src> references) -- no framework change needed.
- JS modernization was conservative by design, since a regression here is a live-game UI bug: var ->
  let/const; string concatenation -> template literals; inline anonymous function(){} callbacks ->
  arrow functions; Array.prototype.forEach.call(nodeList,...) -> nodeList.forEach(...); unused catch
  bindings -> catch{} (ES2019). Named function declarations were DELIBERATELY KEPT as `function name(){}`
  (not converted to const arrow) wherever hoisting order could matter, to avoid introducing a
  temporal-dead-zone bug from a mechanical conversion. Every `this`-based event handler (hud_editor.html:
  btnSnap click, selSnap change, scaleRange/scaleNum input -- 4 total, found by grep, not assumed) was
  rewritten to use e.currentTarget/e.target explicitly instead of relying on `this` binding, since arrow
  functions don't bind their own `this` and a blind conversion there would have broken those handlers.
- Files: all 11 hud_*.html slimmed to structure only; new hud_*.css + hud_*.js per view (33 files total).
  No C++ changes -- CreateView() still takes the same .html path, sibling asset resolution is automatic.
  Built nothing (pure asset change); deployed the full file set (html+css+js) to the live modlist views
  directory. NOTE: a legacy, already-unused CMakeLists.txt in this plugin still references the old
  single-file .html copies -- left untouched (xmake is the real build path per this project's own docs;
  the cmake file predates hud_keybinds.html too and was already stale).
- RUNTIME-VERIFY: open the game, confirm every HUD widget (health/ap/compass/rads/ammo/grenade/survival/
  interaction/quick-loot) still renders and updates, and that Shift+F11 (editor) and LShift+F10
  (keybinds) still open and function correctly (drag/resize/save/rebind).


## 2026-07-12 (pt4) — Quick-loot shows only "R Transfer" at the settlement workshop bench

Requested: aiming at the workshop bench showed the full quick-loot item list; vanilla only ever offers
"Transfer" there, never a per-item take list. Fixed to match.

- Detection: TESObjectREFR::GetLock()-style CommonLibF4 verification, no guessing -- the workshop
  bench reference carries RE::Workshop::ExtraData (EXTRA_DATA_TYPE::kWorkshop, the same extra data the
  settlement's power-grid/deleted-item tracking lives on), confirmed via CommonLibF4 headers. Added
  QuickLootController::IsWorkshopBenchTarget(), set as a side effect of ResolveTarget() (reset false on
  every early-return path so it can't go stale from a previous target).
  m_active itself stays true for the bench (R must keep working) -- this is a SEPARATE flag, not folded
  into Active().
- BuildSnapshot() short-circuits to a minimal {"visible":true,"containerName":...,"transferOnly":true}
  payload for the bench, skipping the (potentially huge, hundreds-of-items) inventory walk entirely --
  a real perf win, not just a display change.
- QuickLootInput::OnButtonEvent gains `if (qlc->IsWorkshopBenchTarget() && !isOpenFull) return;` right
  after the existing active/live-pick gate -- Take/Take-All/scroll/arrow-select/activate-choices all
  fall through untouched (not consumed) for the bench; R is completely unaffected.
- hud_quickloot.html: new transferOnly branch in updateLoot() renders a minimal view -- the item list
  area shows a centered "R Transfer" message (reusing .ql-list's existing flex sizing, no new panel
  section), the carry bar and all footer hints except "R Transfer" itself are hidden via a new
  #root.transfer-only CSS state, and any stale row-mesh previews from a PREVIOUS non-bench target are
  explicitly cleared.
- Investigated the user's separately-flagged concern (a settlement-management mod, identified as
  "Settlement Menu Manager" via the actual modlist -- not guessed) that uses Spacebar near the bench:
  confirmed via its own Papyrus source (MainScript.psc) that SMM operates ENTIRELY INSIDE the vanilla
  WorkshopMenu (injects custom FormList categories into the build-mode tree, navigated with vanilla's
  own Space-to-confirm) and does not hook any pre-menu-open hotkey -- so it does NOT conflict with this
  fix, which only affects behavior WHILE AIMING, before any menu opens. If SMM's injected categories
  need to surface through PrismaCrafting's OWN reskinned workshop_helper.html (which fully suppresses
  the vanilla WorkshopMenu SWF), that would be a PrismaCrafting-side compatibility patch, not a
  PrismaHUD one -- flagged for the user to confirm scope/priority before any code is written there, per
  their own "secondary patch, not built into the hud by default" framing.
- Files: QuickLootController.h/.cpp, hud_quickloot.html. Built + deployed (game was closed). RUNTIME-
  VERIFY: aim at the workbench, confirm only "R Transfer" shows (no item list/carry bar), R still opens
  the workshop; aim at an ordinary container afterward and confirm the normal list still renders.


## 2026-07-12 (pt2) — HUD/quick-loot also suppressed during lockpicking

Follow-up to the Crafting/Terminal/Workshop suppression fix above -- same gap, same fix, one more
menu. Confirmed the vanilla lockpicking menu name via CommonLibF4 (RE::LockpickingMenu::MENU_NAME =
"LockpickingMenu", not guessed) and added it to MenuSuppressSink's tracked name set alongside
WorkshopMenu/TerminalMenu/ChemistryMenu/CookingMenu. No other code changes needed -- the mechanism
built for the Crafting/Terminal/Workshop fix (HUDLayoutManager::SetMenuSuppressed + the set-tracked
MenuOpenCloseEvent sink) already generalizes to any real vanilla RE::UI menu. File: main.cpp. Built
+ deployed PrismaHUD_F4.dll.


## 2026-07-12 — PrismaHUD rendered on top of Crafting/Terminal/Settlement (Workshop) menus — FIXED

Root cause: PrismaHUD had ZERO awareness of these menus. The only pre-existing menu sink
(HUDVisibilitySink) watches vanilla "HUDMenu" show/hide -- a different signal fired only for
loading screens/cutscenes/main-menu, never for Workshop/Terminal/Crafting. Confirmed via grep that
all three back onto real vanilla RE::UI menus (unlike quick-loot, a custom CEF overlay RE::UI can't
see at all): PrismaCrafting -> "WorkshopMenu"/"ChemistryMenu"/"CookingMenu", PrismaTerminal_F4 ->
"TerminalMenu" -- so a plain MenuOpenCloseEvent sink reliably detects all four.

- Added HUDLayoutManager::SetMenuSuppressed(bool), a new unconditional gate ANDed into
  ApplyEffectiveVisibility's show formula -- unconditional meaning it applies even to quick-loot,
  which is otherwise deliberately EXEMPT from the F2/combat-only master-hide gate (that exemption
  exists so quick-loot's own poll doesn't go dead when the user manually hides the HUD; menu-
  suppression is a different concern -- the player can't be interacting with a lootable container
  while inside one of these modal menus anyway, so no exemption is warranted here).
- Added a new MenuSuppressSink (main.cpp) tracking a SET of currently-open suppressing menu names
  (not a single bool, so two of these opening back-to-back can't have one's close event wrongly
  clear suppression while the other is still up) for WorkshopMenu/TerminalMenu/ChemistryMenu/
  CookingMenu, calling SetMenuSuppressed(!set.empty()) on each open/close.
- Files: HUDLayoutManager.h/.cpp, main.cpp. Built + deployed PrismaHUD_F4.dll.
- RUNTIME-VERIFY: open a chem/cooking bench, a terminal, and settlement build mode -- health/ammo/
  compass/quick-loot should all disappear while each is open and reappear on close.


## 2026-07-11 (pt24) — "Shadow overlay" bug conclusively NOT PrismaUI/CEF; video origin param + dock/boot sandbox cleanup

- **Shadow overlay investigation closed out**: systematically ruled out via live testing —
  PrismaWheelMenu, shell viewport/backbuffer sizing (confirmed exact 1:1 match), Rad Mod/
  FalloutAnomaly (gas-mask visor + weather/IMAD theory, including a `fw 0002B52A 1` console test),
  PrismaHUD, WastelandHunter, PrismaAmazon — all still showed the overlay when disabled individually.
  **Decisive test**: disabled the ENTIRE PrismaUI_F4 framework — overlay still present. This proves
  it cannot be PrismaUI/CEF or any dependent plugin; root cause is outside this codebase entirely
  (likely ENB, ReShade, a GPU-vendor overlay, or Windows Game Bar). Documented in
  `findings/prismaui-newgen.md` so this doesn't get re-investigated as a Prisma bug later.
- **YouTube Error 153 follow-up**: confirmed via a clean console log (zero errors from the YouTube
  frame) that the previous sandbox-scoping fix worked completely — Error 153 is a separate,
  genuine YouTube-side issue, a documented failure mode for embedding pages not served over a real
  http(s) origin (our `prisma://` scheme is what YouTube's servers actually see in Referer/Origin,
  regardless of how CEF treats the scheme locally). Added the officially documented
  `origin=`/`enablejsapi=1` IFrame Player API parameters to `test_video.html` as the correct next
  step — may not fully resolve a scheme-level limitation.
- **dock.html/boot.html sandbox cleanup**: Chromium's own console warned "an iframe which has both
  allow-scripts and allow-same-origin for its sandbox attribute can escape its sandboxing" for these
  two shell-system views once `allow-same-origin` was added for plugin views last entry — they share
  the shell's own origin (`prisma://shell`), so that combination is a genuine no-op there (and
  `allow-scripts` alone would reintroduce the opaque-origin storage breakage from before). Removed
  the sandbox attribute for them entirely — first-party trusted shell content, nothing to isolate.
  Plugin views (`prisma://views`, genuinely cross-origin) keep `allow-scripts allow-same-origin`
  unchanged.
- Files: `Framework\PrismaUI_F4 New Gen\shell\src\main.ts`,
  `Projects\Prisma\UI_Systems\ModelPreviewTest\assets\views\Interface\ModelPreviewTest\test_video.html`.
  Framework repo commits `436b1ea` (dock/boot sandbox), workspace repo commit `98707262` (video
  origin param) — both local only, not pushed.


## 2026-07-11 (pt23) — PrismaUI_F4 New Gen: YouTube "Error 153" fixed — our own sandbox was hijacking the player's calls

- After the previous fix (sandbox `allow-same-origin`), the black video panel was gone — real
  progress, YouTube's player chrome now rendered (progress bar, play/pause, volume slider) — but
  playback failed with YouTube's own **"Error 153: video player configuration error"**.
- **Cause**: `PrismaCefApp::OnContextCreated`'s network sandbox injection only checked
  `!frame->IsMain()`, which also matched the nested `youtube.com` iframe itself. Our own fetch/XHR
  wrapper (`NetworkSandbox::GenerateScript()`, whitelist-checked against OUR domain list) was
  therefore hijacking YouTube's OWN internal network calls — thumbnails, internal config/API
  endpoints on domains never on our whitelist (`ytimg.com`, `ggpht.com`, various Google API hosts) —
  silently blocking them and breaking the player's init sequence. A real YouTube-side error, not a
  CEF rendering/compositing bug: a frame we already chose to trust enough to load via `frame-src`
  whitelisting shouldn't also have its internal calls restricted by a different, unrelated whitelist
  meant for content we serve ourselves.
- **Fix**: scoped the injection to `frame->GetURL()` starting with `"prisma://"` (our own scheme) —
  whitelisted external embeds (YouTube or anything else added to `WHITELISTED_DOMAINS` later) are
  now left alone entirely, free to make whatever internal calls their own logic needs.
- Also investigated a separate user-reported "shadow overlay" screenshot this session — ruled out
  PrismaWheelMenu (user disabled it, still present) and confirmed the shell's backbuffer/viewport
  sizing is exact (`backbuffer 2560x1440, client 2560x1440 -> shell viewport 2560x1440`, scale
  1.000x1.000, from `PrismaUI_F4.log`'s `[CefBridge] sizes:` line) — no resize desync. Leading
  hypothesis was Rad Mod's gas-mask visor overlay (`radhud.html`'s `#maskOverlay`/`#visorFog`,
  confirmed real full-screen elements with auto-mask-equip enabled per `FalloutAnomaly.log`), but
  user then ruled out Rad Mod and PrismaHUD too (disabled both, still present) — **still open**,
  root cause not yet found; not a PrismaUI_F4-side bug based on everything checked so far.
- Files: `Framework\PrismaUI_F4 New Gen\src\Cef\Subprocess\PrismaCefApp.cpp`. Framework repo commit
  `b12d753` (local only, not pushed). Redeployed both `PrismaUI_F4_Host.dll` AND
  `PrismaUI_F4_CefSubprocess.exe` (renderer-process logic). Awaiting confirmation video actually
  plays now.


## 2026-07-11 (pt22) — PrismaHUD_F4: kill (not hide) builtin HUD views on vanilla HUDMenu close

- Decompile-verified that fully unloading vanilla `HUDMenu` itself is unsafe (`UI::CloseAllMenus`
  hardcodes an exemption list protecting it); found the engine's own native `ShowHUD()`/`CloseHUD()`
  calls instead, which fire real menu-stack open/close events for "HUDMenu" at loading/cutscene/
  main-menu transitions.
- `PrismaHUD_F4\src\main.cpp`: added `HUDVisibilitySink` (`RE::BSTEventSink<MenuOpenCloseEvent>`,
  registered on `RE::UI` at kGameDataReady via `InstallHUDVisibilitySink()`). On `"HUDMenu"` close,
  `DestroyBuiltinViews()` tears down all 9 always-on battle-HUD CEF views (`g_api->Destroy()`, not
  `Hide()`); on reopen, `CreateBuiltinViews()` rebuilds them (already idempotent per-field). Editor/
  keybinds overlay views untouched. Built clean, deployed to
  `E:\Modlists\Fallen World Alpha 2\mods\PrismaHUD\F4SE\Plugins\PrismaHUD_F4.dll`. Not yet in-game
  tested. See `docs\findings\prismahud.md` + `ReverseEngineering\03-FINDINGS.md` (2026-07-11 entries).


## 2026-07-10 (pt15) — Bug L root-caused + fixed: mesh worker queue depth, not scroll-gating

Root-caused with real log timestamps before any fix, per standing instruction. Not the same mechanism
as Bugs E/G/H (checked, since they share a general "visual element doesn't resolve promptly" symptom
class) -- this is initial-load latency on a correctly-functioning request path, not an identity/lifecycle
bug.

- Confirmed via PrismaHUD_F4.log: on a 28-item container open, row0/row1 ("10x25mm Round"/"Adhesive")
  loaded immediately, but row5 ("AI-2 Medkit") didn't report "loaded" until +6.2s, row6 +7.1s, row10
  +9.4s -- a monotonically increasing delay despite ALL of these being comfortably inside the static
  ±10-item preview window around a stationary selection, meaning they were all requested (api.show()
  fired) on the SAME first tick. This directly answers what was asked to confirm: rows are NOT waiting
  for a scroll to be requested; the delay is ModelPreview's single worker thread processing a batch of
  distinct new meshes strictly one at a time (disk/BA2 read + NIF parse + GPU buffer build per mesh).
  If a player naturally scrolls a few seconds into a big container (very normal), rows that finish
  loading around the same time create the illusion that scrolling caused it.
- User approved pursuing the real fix (parallelize the worker) contingent on verifying the shared D3D11
  device permits free-threaded resource creation first. CONFIRMED via a NEW Ghidra script
  (ReverseEngineering\Scripts\FindCallers.java, decompiles the real callers of a target RVA, not
  just the import thunk): BSGraphics::CreateD3DAndSwapChain (RVA 0x1d17460) calls
  D3D11CreateDeviceAndSwapChain with Flags=0 -- D3D11_CREATE_DEVICE_SINGLETHREADED (bit 0x1) is NOT set.
  Logged in ReverseEngineering\03-FINDINGS.md (2026-07-10 entry).
- FIX (framework repo, its own separate git history, commit 34f8c37): pooled ModelPreview's single
  worker thread to 3 (kWorkerCount). Added a mutex around g_lru/g_missing (unsynchronized before --
  safe with exactly 1 worker, undefined behavior with more). Shutdown() changed notify_one -> notify_all
  so every pooled worker actually wakes and exits (previously only one of N would have on shutdown).
  File: PrismaUI_F4 New Gen\src\PrismaUI\ModelPreview.cpp.
- Built + deployed all three affected DLLs (PrismaUI_F4.dll, PrismaHUD_F4.dll, PrismaWheelMenu.dll --
  the latter two were blocked by a running game process earlier this session; deployed once closed).
  RUNTIME-VERIFY: open a large, varied container and confirm rows well within view resolve within ~1-2s
  instead of climbing to 6-12s.


## 2026-07-10 (pt14) — Bugs E/F/G/H/I/J investigated + fixed; K confirmed not-yet-built

Root-caused with real evidence (PrismaHUD_F4.log grep, CK wiki) before any fix, per standing instruction.

- BUGS E + G + H (shared root cause, FIXED) — overlapping/stacked preview models, leftover model in an
  empty container, blank-space-plus-overlap after the list shrinks. Confirmed via log: 'row4' was reused
  across THREE different nifPaths in one session (RadAway.nif -> BottleWine02Empty.nif -> Holotape_Prop.nif)
  and 'row5' held AnimObjects\RussianStimPack.nif (the AI-2 Medkit mesh) -- previews were keyed by ROW
  INDEX, which is not a stable identity: the engine's inventory iteration order can change after any
  removal, and the same mutable state is touched from TWO independent call sites (the 200ms poll interval
  AND renderList()). FIX: key every preview by the item's own formId instead of its row index -- the same
  logical item keeps the same preview id for as long as it exists regardless of which row it renders at;
  the moment it's gone, its id is simply absent from the next computed active set (from either call site)
  and gets explicitly hidden. Files: hud_quickloot.html (wireStatusOnce, updateRowPreviews, renderList's
  data-formid attribute). NOTE: the "blank space" part of Bug G is NOT a layout bug -- .ql-list has a
  fixed-height flex container, so empty space below fewer items is expected/normal, same as any list UI;
  only the model-overlap symptom needed fixing.
- BUG F (FIXED) — the rollover footer stayed visible over the vanilla Transfer (R) menu for up to one poll
  interval, because nothing hid it PROACTIVELY when R fires -- ResolveTarget()'s "ContainerMenu visible"
  check only runs on the ~200ms poll. FIX: OpenFullContainer() now hides the rollover and drops exclusive
  mode immediately, synchronously, before firing ActivateRef(). m_container is deliberately left intact so
  returning from Transfer to the same container doesn't reset the selection. File: QuickLootController.cpp.
- BUG I (reviewed; one fix, one confirmed-not-a-bug) — over-capacity ("35/32") while still able to take
  items is CONFIRMED vanilla-permissive (Creation Kit wiki: IsOverEncumbered is a queryable state used for
  effects like blocking fast-travel; it never blocks looting, and NPCs are never over-encumbered). Real
  miscalculation found: the carry-bar "ghost" projection used the SELECTED item's per-unit weight instead
  of weight*count, understating the actual impact of taking a full stack (E removes the whole stack via
  entry.count). Fixed in hud_quickloot.html's updateLoot().
- BUG J (FIXED) — wheel (F) sometimes got stuck open, only right-click dismissed it. CONFIRMED regression
  from Feature 2, exactly as suspected: in toggle mode, OnHotkeyDown() shares ONE callback for both
  open-if-closed and close-if-open. BindingRegistry's suppressible=true dispatch gate wraps the ENTIRE
  onDown callback, so if exclusive mode engaged AFTER the wheel was already open (e.g. quick-loot's own
  proximity poll flips it on while the wheel is still up), the player's next F press meant to CLOSE it was
  silently swallowed by the SAME gate meant to block OPENING -- right-click still worked because it's a
  separate, non-suppressed cancel path. FIX: registered wheel_open with suppressible=false; added
  IVPrismaHUD2::IsExclusiveModeActive() (a query, not a dispatch gate) that WheelSession::OnHotkeyDown()
  consults explicitly, ONLY in its "open a NEW wheel" branches -- the close branch is never gated by
  anything. Files: PrismaHUD_F4_API.h (+ synced to wheel/WH copies), BindingRegistry.h, WheelSession.h/.cpp,
  PrismaWheelMenu main.cpp.
- BUG K (status confirmed, NOT a bug) — Field Dress still missing from the rollover because Feature 5's
  generic activate-choice enumeration was never implemented, only investigated/approved in design. The
  only existing related code (EnableActivateChoiceFilter/SuppressActivateChoicePerk) is a pre-existing
  SUPPRESSION tool from an earlier session, not the enumeration-and-display system Feature 5 designs.
  Nothing to fix yet -- Feature 5 is still queued.

Built PrismaHUD_F4.dll + PrismaWheelMenu.dll; hud_quickloot.html deployed successfully (no file lock). DLL
deployment BLOCKED this pass -- Fallout4.exe was running and holds both DLLs open; deploy on next game close.
RUNTIME-VERIFY once deployed: take items from a large container and confirm no stale/duplicate meshes;
empty a container fully and confirm no leftover model; press R and confirm the rollover disappears
immediately; open the wheel near a lootable target and confirm F still closes it via toggle.


## 2026-07-10 (pt13) — Feature 2 test-pass fixes: Bugs A/B/C/D

Four issues surfaced testing Feature 2 (LShift+F10 keybind menu, wheel/WH registry consumption).
Investigated with real log evidence (PrismaHUD_F4.log, PrismaUI_F4.log) before fixing, per standing
instruction. Two (A, B) share one root cause; C is a real gap in the Bug 3 fix; D is a small UX addition.

- BUG A + BUG B (shared root cause, FIXED) — "container has items scrolling can't reach" / "R needs two
  presses". NOT a scroll-range/pagination bug (verified: no item cap or truncation anywhere in
  BuildSnapshot() or renderList()). Real cause: LivePickIsCurrentTarget() (the Bug 2 fix) compared
  m_cachedPick (event-driven, effectively real-time) against m_container (poll-driven, refreshed only
  every ~200ms) with STRICT equality. Any normal camera micro-sway between poll ticks makes the crosshair
  ray transiently miss (pick briefly resolves to null) even though the player never stopped aiming at the
  same container -- the old check treated that null exactly like "aiming at something else," silently
  dropping the input (no log fires on that path, which is itself why it looked silent). For a single R tap
  this cost one dropped release (needs a retry); for continuous wheel scrolling it dropped scattered ticks
  throughout, reading exactly like a hard scroll limit. FIX: a momentarily-null live pick is now treated as
  "no new information" (keep trusting the last target) rather than "confirmed elsewhere" -- only a pick
  that resolves to a genuinely DIFFERENT object invalidates the gesture, which still fully blocks the
  original Bug 2 pencil scenario. Also added a throttled diagnostic log at the gate so any future
  occurrence is directly traceable instead of re-derived from first principles. File: QuickLootController.h/.cpp.
- BUG C (FIXED) — wheel (F) still opened while the quick-loot rollover was showing. Confirmed root cause:
  quick-loot is a custom PrismaUI overlay, so RE::UI's menuMode never reflects it (unlike a real vanilla
  menu) -- the wheel's Bug-3 menu-mode guard genuinely cannot see it. Wired QuickLootController into the
  SAME BindingRegistry::SetExclusiveInputMode() mechanism camp-placement already uses (SetExclusive() helper,
  toggled on real active/hidden transitions only). This exposed a real latent bug in the Feature 2 registry:
  _exclusive was a plain bool despite being documented as "ref-counted-safe" -- with a SECOND caller now
  wired in, one source's off() could have clobbered the other's still-active on() (e.g. approaching a
  corpse while a camp ghost is still being placed nearby). Converted to a real atomic reference count
  (_exclusiveCount) before that bug could ever fire. Files: BindingRegistry.h/.cpp, QuickLootController.h/.cpp,
  PrismaHUD_F4_API.h (doc comment corrected + synced to wheel/WH copies).
- BUG D (FIXED) — no on-screen indicator for the Feature 1 hide-rollover key. Added a footer hint ("<key>
  Hide") that reads the CURRENT binding live from BindingRegistry::GetBoundKey("rollover_hide") every JSON
  push -- never hardcoded, so a rebind in the keybind menu updates the hint automatically. Files:
  QuickLootController.cpp (added hideKeyName to the payload), hud_quickloot.html.

Built PrismaHUD_F4.dll + deployed (DLL + hud_quickloot.html). Wheel/WH DLLs unchanged this pass (only a
doc-comment sync to their copied API header). RUNTIME-VERIFY: R opens Transfer on the first press; wheel
scrolling reaches every item; F no longer opens the wheel while the loot rollover is visible; footer shows
the live hide-rollover key and updates if rebound.


## 2026-07-10 (pt12) — Feature 2: shared keybind registry (+ Feature 1, + intra-Prisma Feature 3)

Built the shared, user-rebindable hotkey system the user green-lit. One registry owned by PrismaHUD,
consumed by the wheel and WH — ends the three-independent-VK-maps fragmentation that caused Bug 3.

- NEW BindingRegistry (PrismaHUD_F4/src/BindingRegistry.h/.cpp): the single owner of every global
  hotkey. Actions register by stable id + default key; the registry binds them through PrismaHUD's
  KeyHandler at the persisted-or-default key, persists overrides to
  Documents\My Games\Fallout4\PrismaHUD\keybinds.json via an atomic tmp+rename+mutex write (the
  SaveLayoutFile pattern, NOT the non-atomic SaveSettings), and supports: rebind, reset-to-default,
  conflict lookup, an exclusive-input mode (suppresses suppressible actions), and a capture mode
  (suppresses ALL dispatch while the menu waits for a key so the captured key doesn't also fire).
  LoadFromDisk re-applies overrides to already-registered bindings so plugin load order doesn't matter.
- NEW V2 API (PrismaHUD_F4_API.h IVPrismaHUD2 + RequestBindingsAPI): RegisterBinding / UnregisterBinding
  / GetBoundKey / SetExclusiveInputMode. C-ABI callbacks (void(*)(void*)) — DLL-boundary safe. Header
  copied into PrismaWheelMenu and WastelandHunter.
- NEW keybind menu view (hud_keybinds.html) opened by LShift+F10 (hardcoded meta key, sibling to the
  editor's LShift+F11 — deliberately NOT rebindable so a bad rebind can't lock out the menu). Lists all
  actions grouped by category, rebind via live key capture, and a conflict-warning modal before applying.
- FEATURE 1: bindable "Hide Loot Rollover" (default Z) -> QuickLootController::ToggleUserHidden(), which
  force-closes the panel AND drops input capture (also a manual escape from the Bug 2 scenario).
- FEATURE 3 (intra-Prisma half): rebinding warns if the key already belongs to another registry action
  (CheckRebindConflict). This half is all-VK so it needs no scancode/VK translation. The VANILLA-bind
  conflict extension (RE::ControlMap) is the part that needs the live scancode<->VK dump verification —
  reported separately before that comparison logic is written, per the user's non-negotiable gate.
- Wheel: keyboard open key now registered through the registry (rebindable, suppressible=true);
  gamepad stays on the wheel's own sink. Falls back to INI-only if PrismaHUD is absent.
- WH: hub hotkey registered through the registry; camp placement calls SetExclusiveInputMode(true/false)
  around Begin/Tick(terminal)/End — this DELIVERS the deferred Bug 3 camp-placement half (the wheel's F
  can no longer fire while placing).
- All three DLLs built + deployed. RUNTIME-VERIFY: LShift+F10 opens the menu; rebinds persist; wheel key
  rebindable; camp placement suppresses the wheel.


## 2026-07-10 (pt10) — PrismaHUD_F4 quick-loot: full code review + fix first-open mesh race

- Full code review of QuickLootController.cpp, ModelPreview.cpp (framework), and hud_quickloot.html.
- FIXED the long-standing [OPEN][MED] "first mesh doesn't render until I scroll" bug. Root cause found by
  review (not guessed): preview show() calls were only driven by updateLoot()/renderList(), and C++
  (QuickLootController::OnRequestUpdate) only pushes updateLoot when the snapshot JSON changes. On a static
  container updateLoot fires once; if window.__prismaUI_modelPreview isn't injected at that instant
  (DOM-ready race) the previews bail and never retry until the JSON changes -- exactly what scrolling does.
  Fix: the existing 200ms setInterval now also calls updatePreview()/updateRowPreviews() (guarded by
  _visState && !_preview), so show() keeps retrying until the API is live. Safe: repeated identical show()s
  are cheap framework-side (same modelKey -> no reload, non-dirty). File: hud_quickloot.html. Deployed to
  live modlist. RUNTIME-VERIFY: open a container without scrolling; meshes should appear within ~200ms.
- Review also noted (no fix needed): base-value/weight approximation (documented), per-poll full player-
  inventory weight walk (correct, uncached), an async onStatus icon-hide index race (self-heals in 200ms),
  and two bits of dead code -- ModelPreview.cpp LoadTextureCached/g_texCache (superseded by the engine
  texture loader) and vestigial JS navItem/takeItem/takeAll stubs. Left in place; zero runtime impact.


## 2026-07-10 (pt9) — PrismaUI_F4 framework: actually fix sideways mesh previews (pt6 was incomplete)

- Meshes were STILL rendering sideways after the pt6 fix (holotapes, RadAway, several MISC items) -- pt6
  correctly stopped misclassifying flat shapes as elongated, but then left flat items completely unrotated,
  which does nothing for a mesh authored lying flat: our camera sits at a modest elevation looking mostly
  horizontally, so an object with its thin axis vertical is seen almost edge-on either way. Verified against
  logged extents (Holotape_Prop.nif 4.8x4.0x0.8, RadAway.nif 28.1x16.1x5.6 -- RadAway's ~1.75x axis gap was
  also being misclassified as elongated by the old 1.2x-vs-Z-only check).
- Replaced both special-cased branches with a proper min/mid/max classification: ELONGATED (longest axis
  >2x the middle axis) rotates the long axis to vertical; FLAT (middle axis >1.3x the smallest) now rotates
  the thin axis to face the camera -- the same X-rotation the armor/clothing branch already used, extended
  to generic items.
- File: ModelPreview.cpp (framework, not PrismaHUD). Built + deployed. RUNTIME-VERIFY.


## 2026-07-10 (pt8) — PrismaHUD quick-loot: strip internal bracket tags from item/container names

- Several mods prefix their FullName with an internal editor/tracking tag in brackets (e.g. "[D_WaterCan]
  Purified Water", "[BLT1] .22 LR Round", "[AI2] AI-2 Medkit") -- that's the real, engine-returned name,
  not something PrismaHUD adds, just not meant for players to see. Added `QLStripBracketTag()` to strip a
  leading "[...] " tag for display (keeps the name as-is if stripping would leave nothing), applied to
  both per-item names and the container/corpse header name.
- File: QuickLootController.cpp. Built + deployed. RUNTIME-VERIFY.


## 2026-07-10 (pt7) — PrismaHUD quick-loot: fix stale mesh preview lingering after emptying a container

- Root cause of a mesh (e.g. a desk fan) still rendering over the "Empty" placeholder after taking the
  last item: `renderList()`'s empty-container early-return path never called
  `updatePreview()`/`updateRowPreviews()` before returning, unlike the normal path. `navItem()`'s up/down
  handlers call `renderList()` directly with no sync call of their own, so a take-all landing right as a
  nav event fired could leave a row's mesh id un-hidden indefinitely. Fixed by syncing both preview
  functions inside the early-return branch itself.
- File: hud_quickloot.html. Deployed. RUNTIME-VERIFY.


## 2026-07-10 (pt6) — PrismaHUD quick-loot: armor mesh, carry weight (for real), sideways meshes, icon size

- Armor/clothing/jewelry always showed the category placeholder, never a mesh: `TESObjectARMO` does not
  inherit `TESModel` (confirmed via CommonLibF4 headers) -- the wearable mesh lives on the item's
  ArmorAddon(s) instead. Fixed by mirroring PrismaCrafting's proven pattern: walk `modelArray`, take the
  first addon's `bipedModel[0]`/`[1]` (male/female). File: QuickLootController.cpp.
- Carry weight was still 0/max after the previous fix -- `TESObjectREFR::GetWeightInContainer()`'s
  REL::VariantID isn't OG-verified and read back 0 in-game. Replaced with a manual sum over the player's
  own inventory using the exact same ForEachStack + GetFormWeight technique already proven for the looted
  container's totalWeight. No engine-address dependency. File: QuickLootController.cpp.
- Mesh previews rendered sideways for flat MISC items (holotapes, settings items): the orientation
  heuristic only checked "longest axis vs Z", so a flat item already lying correctly (broad face up) got
  "stood up" on its edge by coincidence. Added a check that the long axis must ALSO dominate the other
  horizontal axis (genuine elongation) before rotating -- bottles/syringes/ammo unaffected, flat items keep
  their correct default pose. File: ModelPreview.cpp (framework).
- Row mesh icons bumped from 22x22 to 34x34 (zoom 0.85->0.95) per direct "far too small" feedback. File:
  hud_quickloot.html.
- Built + deployed all three (PrismaHUD_F4.dll, PrismaUI_F4.dll framework, hud_quickloot.html).
  RUNTIME-VERIFY.


## 2026-07-10 (pt5) — PrismaUI_F4 framework: fix pink mesh preview (material vs. inline texture priority)

- Root cause of the AI-2 Medkit (Russian Stimpack Replacer) showing a pink/magenta mesh preview: NOT a mod
  bug, despite an earlier pass wrongly concluding that from a plausible-looking log line. Extracted the
  actual files from the mod's BA2 with bsab.exe and read the raw bytes directly: `Stimpack.BGSM` on disk is
  byte-for-byte correct (real StimPack_D/_N/_S.dds references), but the combined
  `AnimObjects\RussianStimPack.nif` (the file the Ingestible's MODL points at) has its own stale INLINE
  texture set on that shape -- confirmed in the raw NIF bytes -- literally `textures\Downloads`, a leftover
  dev path baked in at export time. The shape has BOTH that stale inline set AND a valid named external
  material. ModelPreview.cpp resolved inline-first, material-file only if inline was empty -- backwards
  from how the engine actually treats a shape once it names a material (the entire point of the BGSM
  system is that it's authoritative). We were preferring the mesh's own stale data over the correct
  material, landing on the engine's built-in missing-texture placeholder (pink) since the inline path
  doesn't exist.
- Fix: reversed the priority for regular (non-effect) shapes in ModelPreview.cpp's Pass 1.5 -- named
  material tried first, inline only as the fallback. Effect-shader (glass/glow .bgem) shapes unchanged.
- File: ModelPreview.cpp, in the PrismaUI_F4 New Gen FRAMEWORK (not PrismaHUD) -- fixes this class of bug
  for every plugin using the model-preview API. Built + deployed (PrismaUI_F4.dll). RUNTIME-VERIFY.


## 2026-07-10 (pt4) — PrismaHUD_F4 quick-loot: fix carry-weight showing current==max

- Root cause of the carry bar showing e.g. "32/32": `carryWeight` is the actor value for max carry
  CAPACITY (base + perks/SPECIAL) -- FO4 has no separate "current used weight" actor value, so reading it
  for both current (`GetActorValue`) and max (`GetPermanentActorValue`) just returned the same capacity
  number twice. Fixed by using `TESObjectREFR::GetWeightInContainer()` (the same engine function the
  vanilla Pip-Boy uses) for the current-weight side; the max/capacity side is unchanged.
- File: QuickLootController.cpp. Built + deployed. RUNTIME-VERIFY.
- CORRECTION to the prior entry below: "shows a pickaxe" was never actually reported -- the user's message
  was cut off mid-word ("showing pick...") and I filled in "pickaxe" myself instead of asking. The
  PrismaCrafting-icon-cache "evidence" is retracted too (that system is deprecated, not a reference).
  Identified the item is the Russian Stimpack Replacer's re-skin of the vanilla Stimpak ("[AI2] AI-2
  Medkit", FormID 023736:Fallout4.esm, Model.File `AnimObjects\RussianStimPack.nif`), but the actual
  symptom still needs to come from the user before diagnosing further.


## 2026-07-10 (pt3) — PrismaHUD_F4 quick-loot: fix mesh/icon overlap, add real scrolling for big containers

- Fixed the row mesh "overlapping with the placeholder icon" reported from a screenshot: the category
  icon fallback was never hidden once the real mesh composited on top, so both showed at once (visible
  through the mesh's zoom margin). Wired the model-preview onStatus callback to hide a row's icon once its
  mesh confirms loaded/textured, persisted per formId (`_meshReady{}`) and baked into the generated markup
  so it survives renderList()'s ~200ms full-innerHTML rebuild without flashing the icon back in.
- Added real scrolling for containers with more items than fit on screen. Mouse wheel is captured at the
  native input layer for selection (not routed to the DOM as a real wheel event), so `.ql-list` went from
  `overflow:hidden` to `overflow-y:auto` (+ a slim scrollbar) and, critically, `renderList()` now calls
  `scrollIntoView({block:'nearest'})` on the selected row every render -- the existing wheel/arrow
  selection mechanism now keeps the highlight on-screen through a container with hundreds of items.
  Also switched the per-row mesh-preview logic from a flat "first 16 items" cap to a +/-10 window around
  the current selection, checked against `.ql-list`'s actual visible bounds -- the compositor draws mesh
  overlays at raw backbuffer coordinates and ignores CSS overflow clipping, so a scrolled-out row would
  otherwise still float its mesh past the panel.
- File: hud_quickloot.html only. Deployed. RUNTIME-VERIFY.


## 2026-07-10 (pt2) — PrismaHUD_F4 quick-loot: fix rarity-dot CSS leak, per-item mesh row icons

- Fixed a real CSS bug from a screenshot report ("the rarity bars... just the grey rectangle does not
  look good"): `.rarity-common/.rarity-uncommon/.rarity-rare/.rarity-veryrare/.rarity-legendary` were
  bare class selectors meant only for the 6px `.rarity-dot`, but `.item-name` carries the same class for
  its own text-color rules -- so the background rules leaked onto the whole name, painting a big grey
  rectangle behind every common-rarity row. Scoped the background rules to `.rarity-dot.rarity-*`.
- Upgraded per-row icons from generic category glyphs to PER-ITEM mesh thumbnails, per the user's ask
  ("instead of ammo icons, scrap icons... if we can render a still image that will be good"). Reused the
  existing model-preview API per visible row (capped at 16 concurrent, `ROW_PREVIEW_CAP`), each with its
  own preview id and `spin:0` for a frozen/still render. The category PNG icon stays underneath as an
  instant fallback and gets visually replaced once the mesh composites on top.
- Investigated the reported "first mesh doesn't render until you scroll" bug at length (ModelPreview.cpp
  render/dirty logic, HUDLayoutManager's view Show/Hide gating) -- root cause not confirmed, the obvious
  theories don't hold up under code review. Added `wireStatusOnce()` (logs each preview's onStatus phase
  to the console/log for next time) and a first-open retry kick (+150ms/+400ms) as mitigations rather than
  guessing a fix. Logged as `[OPEN][MED]` in open-items.md for the next session to trace via the log.
- File: hud_quickloot.html only (icons from the previous entry reused). Deployed. RUNTIME-VERIFY.


## 2026-07-10 — PrismaHUD_F4 quick-loot: real category/caps icons, fix a footer-text regression

- Replaced the per-row category glyphs (unicode ⚔◈✚⌖◆) with actual PNG icons, supplied by the user in
  `assets/icons/Rollover/`. All source images were already properly alpha-transparent (confirmed via a
  quick Python/PIL flood-fill-from-border check -- no chroma-keying was actually needed despite looking
  opaque black/white in a flat preview). Auto-cropped + resized to 64x64, deployed as
  `assets/icons/cat-{weap,armo,aid,ammo,misc,caps}.png`, wired in via `<img>` instead of unicode spans.
  Also swapped the caps/value glyph (`&#x00A4;`) for the `cat-caps.png` icon in both the per-row value and
  the footer total.
- Photoshop MCP was unreachable this session (script timeout) -- processed the icons with a local PIL
  script instead (`process_icons.py` in the session scratchpad, not checked in).
- Fixed a regression introduced by the previous commit (`f949c66d`): the "Shift Transfer" footer hint had
  reverted to "R Transfer" in the working tree before this session touched the file. Restored to Shift.
- Files: hud_quickloot.html, assets/icons/cat-*.png, assets/icons/Rollover/ (source). Deployed (no DLL
  changes). RUNTIME-VERIFY (open-items.md) -- not closed on diff.


## 2026-07-09 (pt21) — PrismaHUD_F4 quick-loot: fix mesh preview overlap (too large/off-center/bleeding)

- Root cause of "3D mesh too large, off-center, bleeding outside the container": `#qlPreview` was a
  `position:absolute` overlay floating over `#root`, not reserved space in `.ql-header`'s flex layout,
  so it visually collided with the header's name/badge text. The compositor rect math (JS CSS pixels
  == backbuffer pixels, sphere-fit camera framing) checked out correct on inspection — this was a pure
  CSS overlap bug, not a coordinate/camera bug.
- Fix: `#qlPreview` is now a real flex child of `.ql-header` (44x44, `flex-shrink:0`, placed after the
  count badge) so it always reserves its own space; visibility toggles via opacity (not `display`) so
  header height stays constant instead of jumping per-item. Added `zoom:0.9` in the `.show()` call for
  extra margin now that the box is smaller.
- File: hud_quickloot.html. Built + deployed. RUNTIME-VERIFY (open-items.md) — not closed on diff.


## 2026-07-09 (pt20) — PrismaHUD_F4 quick-loot: value/weight fix, R-key conflict, 3D mesh preview of selected item

- **Value/weight wrong**: root cause was AlchemyItem (aid items) not inheriting TESValueForm -> value=0 for
  stimpaks/food/chems. Replaced per-type base-InstanceData reads with the engine-canonical statics
  `TESValueForm::GetFormValue` + `TESWeightForm::GetFormWeight` (correct for every form type).
- **"R Transfer" conflict**: R was a second escape-hatch to the vanilla transfer menu, but R is vanilla
  Reload/holster. Dropped the R check in ShouldSuppressContainer (Shift remains the "full menu" modifier);
  footer hint changed "R Transfer" -> "Shift Transfer".
- **"Actual icons"**: added a 3D MESH preview of the selected item via the framework's new model-preview API
  (`window.__prismaUI_modelPreview`). C++ ships each item's world-model NIF (`TESModel::GetModel()`) +
  coarse formType; a #qlPreview pane shows the highlighted item's mesh, updated on scroll, hidden on close.
- Files: QuickLootController.cpp, hud_quickloot.html. Built + deployed. All three are RUNTIME-VERIFY
  (open-items.md) — not closed on diff. (FO4 weapons preview only the dummy receiver by design.)


## 2026-07-09 (pt19) — PrismaDesignerBridge: removed the orphaned views-list code, kept the working bridge (and kept EnumerateViews — the debugger needs it)

- Follow-up to pt18's JS-side removal: asked to remove the now-dormant C++ view-enumeration code,
  gated on a project-wide zero-caller confirmation. The gate FAILED and changed the scope:
  PrismaDebugger_F4 calls `EnumerateViews` at two sites (register-console-on-all-views + the debug
  panel's view list) — a real, independent consumer. So EnumerateViews is NOT orphaned.
- User's call: keep the working bridge (Send-to-Game push / live preview), drop only the views-list.
- Removed from PrismaDesignerBridge/main.cpp: `DoListViews`, `DoReadView`, their two WS dispatch
  branches (`listViews`/`readView`), and the per-push `EnumerateViewsEx` trace. Kept the whole push
  path (`DoReload`), `ping`, and the game-state feed. Bridge rebuilt clean + deployed.
- Kept in the framework (untouched, not orphaned): `EnumerateViews` (V4) + its backing stack
  (`CefBridge::EnumerateViews`, `Abi_EnumerateViews`, the `g_views`/`g_stateMutex` iterator) — the
  debugger depends on it. Reverting it to a stub would break the debugger.
- `ViewEntry::owner` (judgment call): surfaced externally only via `EnumerateViewsEx`; even with its
  only external consumer (the bridge) gone, it's computed cheaply, already threaded through machinery
  that stays for the debugger, and is the obvious hook for future per-view attribution. Kept.
- V8 note (per request, so history isn't confusing later): `EnumerateViewsEx` was ADDED for the Live
  Views feature, then that feature was removed. `EnumerateViewsEx` itself was KEPT implemented in the
  framework (it's IVPrismaUI8's only method, so removing it would mean dropping the interface
  declaration and leaving V8 vacuous). It's now implemented-but-unused; harmless. The V8 interface
  version number and history are deliberately left intact.
- Framework was NOT touched — built clean in 1.56s with zero source recompiled (byte-identical), so
  PrismaHUD and the debugger are unaffected by construction (the framework DLL they load is unchanged).
- Files: `Tools/PrismaDesignerBridge/src/main.cpp`. Findings: `docs/findings/prismadesigner.md` pt16.


## 2026-07-09 (pt14) — PrismaDesigner import fidelity: JS-computed positioning recovered, gradient-panel detection fixed, script-execution ruled out with proof

- **hud_health.html / hud_compass.html / etc. (PrismaHUD) — root position now recovered.** All
  PrismaHUD widgets set their real default screen position via a literal `var DEF={xf,yf,...}` object
  in a `<script>` block, applied by JS at runtime (`#root` is always `position:fixed;left:0;top:0` in
  static CSS). New `_recoverRootPosition()` in `js/importer.js` regex-parses that literal object
  straight out of the raw HTML text (no code execution) and shifts every extracted element by the
  delta between `#root`'s measured (0,0) and its intended `DEF` position. Verified against
  hud_health.html (icon/bar now land at x:30,y:899 / x:57,y:902, was x:0-57,y:0 — screenshot
  confirmed on canvas) and hud_compass.html (`#shade` panel lands at x:750,y:14,w:420,h:24 — an exact
  match to its DEF values, confirming the fix isn't overfit to one file).
- **Real bug found + fixed while verifying:** gradient-only-background panels (`background:
  linear-gradient(...)`, no `background-color`) were invisible to the generic extractor entirely —
  it only ever checked `backgroundColor`. Found via hud_compass.html's `#shade` layer. Fixed with a
  `backgroundImage !== 'none'` check plus a best-effort flat-color approximation (first gradient
  color stop). chat.html picked up one more panel from this too (59 → 60 elements, no regression).
- **Script execution for dynamic content (chat lists, live counts) — investigated and rejected.**
  The only way to both run a plugin's own init JS AND read the resulting DOM back out is
  `sandbox="allow-scripts allow-same-origin"`, which is a documented sandbox-escape anti-pattern.
  Proved this empirically (not just cited) with a Playwright PoC: Chrome itself logs "can escape its
  sandboxing," and a fake payload using that combo read `window.parent.State.elements.length` and
  overwrote `window.parent.document.title` from inside the iframe. Since imported files are arbitrary
  third-party plugin HTML, this is not an acceptable trade — documented as a genuine permanent
  limitation, not pursued further.
- Files: `Tools/PrismaDesigner/js/importer.js` (`_recoverRootPosition`, gradient-panel detection +
  fill approximation). Findings: `docs/findings/prismadesigner.md` pt11.


## 2026-07-09 (pt13) — Formal Phase B/C pass: 7-plugin audit + test scripts, 1 real bug found+fixed

- First deliberate, itemized Phase B/C verification pass (per explicit user instruction — not
  "informally confirmed across sessions"). Each of the 7 live plugins got a fresh code audit against
  the 6 established CEF-migration bug classes plus a concrete step-by-step manual test script. Full
  write-up in new file `Projects\Prisma\docs\PHASE_BC_TEST_SCRIPTS.md`.
- **Real bug found and fixed:** PulowskiProtocol's `CreateMenuView()` was defined but never called
  anywhere — confirmed via static grep alone, not a live-test-only finding. `g_view` stayed 0 forever,
  so activating an unpowered Pulowski shelter silently opened no terminal at all. Fixed by calling it
  from `kPostLoadGame`/`kNewGame` per the framework's documented lifecycle pattern.
  `Projects\Prisma\UI_Systems\PulowskiProtocol\src\main.cpp`.
- **False positive corrected:** the audit initially flagged PrismaCrafting's `RegisterJSListener`
  usage as unsafe (touching `RE::` state off the game thread), based on the framework's own stale docs
  claiming those callbacks fire on "the Ultralight render thread." Traced directly against the current
  `API.cpp`: `RegisterJSListener` already marshals via `AddTask` today, identically to `BindUIEvent`.
  Fixed the docs instead of the (not-actually-broken) plugin code.
  `Framework\PrismaUI_F4 New Gen\docs\view-lifecycle.md`, `docs\api-reference.md`.
- 6 of 7 plugins came back clean (PrismaHUD_F4, PrismaCrafting, WastelandHunter, PrismaLocksmith, Rad
  Mod's `radhud.html` substituting for dormant PrismaGasMask, PrismaDesignerBridge). Two prior-session
  fixes re-confirmed intact: WastelandHunter's hub/field-dress re-focus fix, PrismaCrafting's
  WorkshopInputHook click-swallow fix + astral-emoji removal. One deployment gap found (not a code
  bug): PrismaLocksmith has no mod folder in the live modlist at all, needs build+deploy first.
- Removed the leftover dead Ultralight DLLs/resources from the live MO2 mod folder
  (`Prisma UI - NG and OG Web UI Framework Fallout 4\PrismaUI_F4\{libs,resources}`) per direct request
  — these were left in place during the pt11 source cleanup out of MO2-folder deletion caution.
- None of the 7 test scripts have been live-executed yet except PrismaDesignerBridge's WS-layer half
  (see its own pt13 entry) — that's the next concrete step.
  Files: `Projects\Prisma\docs\CEF_MIGRATION_PLAN.md`, `docs\PHASE_BC_TEST_SCRIPTS.md` (new),
  `docs\findings\prismaui-newgen.md` (pt13).


## 2026-07-09 (pt12) — PrismaDesigner: Live Views groups by plugin, "Import All" merges as one scene

- **Built:** Live Views now groups its list by owning plugin (via V8 `EnumerateViewsEx`'s owner
  field) instead of a flat list — PrismaHUD's 9 separate widget files (health/ap/compass/ammo/
  grenade/quickloot/interaction/editor/rads/survival) now show as one "PrismaHUD_F4.dll — 9 views"
  group with an "Import All" that merges every file into one combined scene in a single
  confirm+replace, plus per-file "Import" still available underneath for editing just one piece.
  New `Importer.importMultipleFromStrings()`/shared `_parseOne()` helper in `js/importer.js`,
  grouping + fan-in logic in `js/liveviews.js`. Verified via Playwright with a stubbed 4-view/
  2-owner fixture — correct grouping, correct merged count, correct combined element set. Full
  trail in `docs/findings/prismadesigner.md`.


## 2026-07-07 (pt15) — PrismaHUD_F4 v1 Task 1: uniform per-widget scale (schema {x,y,s})

- Replaced the free w/h box-resize with **position + uniform scale**. Layout schema per widget is now
  `{x, y, s}` (s = content scale multiplier; on-screen size = intrinsic default * s). All 11 widgets got
  `transform:scale(var(--s))` on `#root` (transform-origin top-left) and their position JS rewritten to
  `{x,y,s}` (offsetWidth stays intrinsic under transform, so the compass tape math needed no change).
- C++ (`HUDLayoutManager`): `WidgetEntry` x,y,s (dropped w,h); `IsSanePos` validates s∈[0.2,3.0];
  ApplyToView / BuildEditorPayload / SaveFromJS / LiveMove / LoadSlot / RegisterWidget all carry `s`;
  legacy `{x,y,w,h}` entries load with s=1 (positions preserved, size reset once).
- Editor (`hud_editor.html`): removed the 8 resize handles; added a **Scale control** (slider + numeric,
  0.40–2.50) in the properties panel; box footprint = intrinsic(defaultW*1920) * s via bw()/bh(); drag,
  align, distribute, undo, save all converted to scale.
- Files: HUDLayoutManager.cpp/.h, hud_editor.html + all 11 widget views (9 builtin + Rad hud_toxic/
  hud_filter). Built + deployed.
- **RUNTIME-VERIFY (still OPEN, not closed on diff):** compass blip/caret alignment at 0.7/0.75/1.0/preset
  scale; quick-loot backdrop-filter render + click/hover hit-testing under the transform. See open-items.md.


## 2026-07-07 (pt14) — PrismaHUD_F4: master HUD visibility (F2 hide key + combat-only mode) + always-on perf

- Hide-HUD keybind (default F2, VK-based, editable in the editor's new "HUD SETTINGS" section — click the
  key button and press a key to rebind). Toggles the whole HUD; session-only (not persisted).
- "Show HUD only in combat" setting (persisted): HUD stays hidden until the player enters combat. Combat
  read via PlayerCharacter::IsInCombat(), throttled ~200ms from the data path; hidden CEF views keep
  running JS timers (Abi_HideView doesn't call WasHidden), so the poll re-shows the HUD on combat.
- Performance: the data-path BIND macro (already skipped unchanged InteropCalls) now also skips the JSON
  build + push entirely while the HUD is master-hidden — a hidden HUD does almost no per-poll work.
  LoadAndApply enforces effective visibility at load so combat-only starts hidden out of combat.
- New master gate in HUDLayoutManager (effective = !manualHidden && (!combatOnly || inCombat)) AND'd into
  ApplyEffectiveVisibility. Settings persist to Documents\My Games\Fallout4\PrismaHUD\settings.json.
- Files: HUDLayoutManager.cpp/.h, HUDDataProvider.cpp/.h, main.cpp, hud_editor.html. Built + deployed;
  not yet runtime-confirmed.


## 2026-07-07 (pt13) — PrismaHUD_F4: quick-loot rendered at top-left in the editor (never positioned while hidden)

- With the pt12 editor preview making widgets render, quick loot showed its content at (0,0) top-left
  while its editor outline was at the correct position. Quick loot is the only widget created hidden, so
  its applyPos always bailed (a hidden view has no layout / `innerWidth==0`) and #root stayed at its CSS
  default left:0/top:0; nothing re-applied position when the editor showed it.
- Fix: C++ ShowAllWidgets now re-applies each view's position after Show; hud_quickloot.html stores the
  target as fractions and retries until the view is sized. Files: HUDLayoutManager.cpp, hud_quickloot.html.
- Built + deployed; not yet runtime-confirmed.


## 2026-07-07 (pt12) — PrismaHUD_F4 + Rad Mod: grenade=pistol real root cause, editor "open twice", editor preview for auto-hide widgets

In-game log/screenshot evidence drove all three.

- **Grenade widget showed the equipped pistol** — the pt11 `IsThrownWeapon()` fix was itself wrong. The
  grenade-scan trace logged `gun=true thrown=true` for the Kimber Custom pistol: `REX::TEnumSet::any()`
  is a bitwise-AND overlap test, but `WEAPON_TYPE` is a sequential-value enum (kGun=9=0b1001,
  kGrenade=0xA=0b1010 share bit 3), so `IsThrownWeapon()`/`IsGunWeapon()`/`IsMeleeWeapon()` all false-
  positive. Fixed with an exact type-value compare in `BuildGrenadeJSON`; trace now logs the raw type.
- **"Had to open PrismaHUD twice"** — on first open the editor view can be shown-but-not-laid-out, so
  `window.innerWidth` reads 0 and every box computes `x*0 = 0` (0.0000 coords, invisible). `initWidgets`
  now refreshes VP live and retries until the viewport is sized.
- **Auto-hide widgets were empty boxes in the editor** (Toxicity, Radiation, Gas Mask Filter, Survival,
  Interaction Prompt, Quick Loot, Grenade) — added an editor-preview hook: `HUDLayoutManager::
  SetEditorPreview` InteropCalls `window.setEditorPreview` on every registered view; each data-gated
  widget force-shows sample content while the editor is open (data handler honors a `_preview` flag).
- Files: `HUDDataProvider.cpp`, `HUDLayoutManager.cpp/.h`, `main.cpp`, `hud_editor.html`, hud_rads/
  survival/interaction/quickloot/grenade.html, Rad Mod hud_toxic/hud_filter.html. Built + deployed;
  not yet runtime-confirmed. New memory: [[reference_tenumset_any_is_bitwise_weapon_type]].


## 2026-07-07 (pt11) — PrismaHUD_F4 + Rad Mod: editor-drag reliability, grenade-name trace, Rad widget registration (3-bug pass)

Evidence-first investigation (three parallel trace agents + manual log/source verification) before any
fix, then implemented. All built + deployed; NOT yet runtime-confirmed.

- **Editor drag** (`hud_editor.html`): widgets only re-dragged after Reset because the `.wdz` box
  (inside `#overlay` z10, its own stacking context) slid under the toolbar/align-bar/props-panel/sibling
  widgets, which swallowed the next mousedown. Replaced per-element DOM hit-test with document-level
  manual rectangle hit-testing (selected-priority, topmost-first) + raise-selected-to-front, so a widget
  under chrome or another widget stays grabbable.
- **Grenade name** (`HUDDataProvider.cpp`): root cause (magic `9`==`kGun` matching the pistol) was already
  fixed via `IsThrownWeapon()` in the deployed DLL, but unprovable from logs (no formID trace, no grenade
  equipped in-session). Added a throttled `grenade-scan` diagnostic (formID + IsGunWeapon/IsThrownWeapon +
  name) so a retest with a grenade equipped yields real formID evidence.
- **Rad Overhaul widget registration**: (a) `WidgetDesc` defaults are FRACTIONS not pixels — the header
  comment wrongly said "pixels" and Rad Mod's toxicity widget passed raw pixels, so its editor box landed
  off-screen (30×1920). Corrected the toxic desc to fractions and fixed the header comment in both copies.
  (b) Extracted the FILTER/MASK bar panel out of the fullscreen `radhud.html` into a new standalone
  `hud_filter.html` view registered as `rad_filter` (the fullscreen visor must not be editor-managed);
  `PushFilter` now drives both views. Files: `RadHud.cpp/.h`, new `hud_filter.html`, trimmed
  `radhud.html`/`radhud.js`, `PrismaHUD_F4_API.h` (both copies).


## 2026-07-07 (pt7) — PrismaUI_F4 CEF: window.prisma bridge was dead code, ported off Ultralight/JSC

- **Root cause of WastelandHunter's fielddress.html "Butcher"/"Skin" buttons doing nothing:**
  `window.prisma` (getGlobal/setGlobal/getProperty/setProperty/emit) was implemented entirely
  against Ultralight/JavaScriptCore and only ever injected from Ultralight's `OnWindowObjectReady`
  callback. Since Ultralight is fully removed, that callback never fires, so `window.prisma` was
  silently `undefined` in every view under the CEF backend — not just WastelandHunter, framework-wide.
- Rewrote `PapyrusBridge.h/.cpp` against `CefBridge::RegisterJSListener`/`CefBridge::Invoke` (the same
  mechanism every plugin already uses) instead of raw JavaScriptCore + `Ultralight::View`.
  `CefBridge::CreateView` now always injects the bridge on domReady before any plugin's own domReady
  callback runs, for every view automatically — no plugin-side changes needed.
- Found via a 7-plugin code-scan pass (parallel agents reading PrismaHUD_F4, PrismaGasMask,
  PrismaCrafting, PrismaLocksmith, WastelandHunter, PulowskiProtocol, PrismaDesignerBridge for stale
  Ultralight-era assumptions before live-testing) — also surfaced that PrismaGasMask (UI_Systems) is
  dormant/dead code, and PrismaCrafting has two real untested-under-CEF risks (`WorkshopInputHook.cpp`
  installing its own raw window subclass alongside CEF's; astral-plane emoji in `workshop_helper.html`,
  the exact pattern that hard-froze Ultralight) — see `CEF_MIGRATION_PLAN.md` and
  `docs/findings/prismaui-newgen.md` pt6/pt7 for full detail.
  Files: `Projects\Prisma\Framework\PrismaUI_F4 New Gen\src\PrismaUI\PapyrusBridge.h/.cpp`,
  `src\PrismaUI\CefBridge.cpp`, `src\PrismaUI\Listeners.cpp`.

---


## 2026-07-07 (pt10) — PrismaHUD_F4: pin HUD views above every other mod's Prisma overlay (fixes gas-mask visor occlusion)

- User reported Rad Mod's gas-mask visor overlay (its own PrismaUI view) sometimes rendered on top of
  PrismaHUD's health/ammo/filter widgets, cutting them off.
- Root cause: PrismaUI_F4 composites all independent views into one sprite batch sorted by a per-view
  `order` int, auto-assigned as `maxExisting + 1` at `CreateView()` time — whichever mod's view is
  created LATER in a session simply paints on top. Neither Rad Mod nor PrismaHUD ever called the
  framework's existing `SetOrder()` API, so the stacking order was pure session-timing luck.
- Fix: `PrismaHUD_F4\src\main.cpp` `MakeView()` now calls `g_api->SetOrder(view, 1'000'000)` right
  after every view is created — PrismaHUD's widgets and layout editor now always paint on top of any
  other mod's Prisma view, regardless of creation order.
- Built + deployed (`PrismaHUD_F4.dll`). **Not yet runtime-confirmed.**

---


## 2026-07-07 (pt9) — PrismaHUD_F4: compass backing shade for readability on bright backgrounds

- User asked for a "light performance-friendly shading" so the compass reads on bright backgrounds
  (snow, daytime sky, etc.) — text-shadow alone wasn't enough contrast.
- `hud_compass.html`: added a static `#shade` layer behind the tape — a flat dark top-to-bottom
  gradient with a `mask-image` edge fade (transparent at the far left/right, opaque in the middle).
  Deliberately avoided `backdrop-filter`/blur (resamples the framebuffer every frame — real GPU cost);
  this is a flat gradient + mask, painted once and cached like any static layer.
- JS/CSS only, no rebuild. Deployed directly. **Not yet runtime-confirmed.**

---


## 2026-07-07 (pt8) — PrismaHUD_F4: compass turn-lag tuning + hexagon/diamond quest-blip redesign

- Follow-up to pt7: user said the compass still felt laggy while turning, and asked for a bigger
  yellow hexagon/diamond outline marker instead of the small filled dot.
- `hud_compass.html`: tightened the poll/transition pair from 80ms/0.13s to 50ms/0.08s (same
  "transition slightly > poll" ratio for continuous motion), cutting worst-case turn-to-tape latency
  from ~210ms to ~130ms. Replaced the circular `.blip` with an inline SVG hexagon (vertical/pointy
  top-bottom, ~15x20px, translucent yellow fill + yellow stroke outline + glow).
- JS/CSS only — no C++ change, no rebuild. Deployed directly to
  `mods\PrismaHUD\PrismaUI_F4\views\hud_compass.html`.
- **Not yet runtime-confirmed.**

---


## 2026-07-07 (pt7) — PrismaHUD_F4: compass quest blip now follows only Pip-Boy-tracked quests

- User reported the compass quest blip felt laggy/jittery and wasn't tracking "real" quest data.
  Root cause: `BuildCompassJSON()` picked the nearest resolved target across ALL displayed objectives
  from ALL active quests, not just the quest(s) the player toggled "Active"/tracked in the Pip-Boy —
  so the blip jumped to whichever untracked side-quest objective was geometrically nearest, snapping
  every time a different one won.
- Found the real mechanism by searching `ReverseEngineering\Symbols\Fallout4_functions.txt` (per the user's request)
  for compass/quest-target symbols, then decompiling the matching 1.10.163 functions: vanilla's own
  compass (`CalculateCompassMarkersFunctor::operator()`, RVA `0xa3c150`) gates every quest with
  `TESQuest::GetActive()` (RVA `0x5d6aa0` — bit `0x0800` of `QUEST_DATA::flags`) before generating any
  marker for it.
- Fix: `BuildCompassJSON()` now skips any objective whose `ownerQuest->data.flags & 0x0800 == 0` —
  a plain, already-verified CommonLibF4 struct field read, no raw-RVA patch. Added `tracked=N` to the
  throttled diagnostic log.
- Files: `PrismaHUD_F4\src\HUDDataProvider.cpp`. New RE tool: `ReverseEngineering\Scripts\
  ListFuncsInRange.java` (lists functions in an address window — needed to disambiguate templated
  `operator()` overloads that all share the same short name). Built clean, redeployed to the live
  modlist. **Not yet runtime-confirmed** — full decompile trail in `ReverseEngineering\
  03-FINDINGS.md` (pt4) and `docs\findings\prismahud.md` (pt4).

---


## 2026-07-07 (pt5) — CRITICAL FIX: PrismaHUD's just-shipped VTable patch had RVAs off by 32MB

- **The pt4 VTable patch (below) was deployed with all three RVAs short by exactly `0x2000000`** — a
  transcription error subtracting image base from `VTableDump.java`'s absolute addresses (dropped the
  leading `2`, e.g. `0xd13fb8` instead of `0x2d13fb8`). All three wrong addresses landed inside
  `BGSCreatedObjectManager::SaveGame`/`LoadGame` (player-crafted item/enchantment/potion
  serialization), not any HUD vtable — the 8-byte function-pointer write was corrupting live
  save/load-game machine code, applied every session at kGameDataReady. Likely crash or save
  corruption on the next save/load with that DLL active.
- **Caught and fixed before any save exercised the broken build.** Corrected RVAs to
  `0x2d13fb8`/`0x2d14038`/`0x2d14528`, rebuilt, and redeployed immediately, overwriting the broken
  DLL in `E:\Modlists\Fallen World Alpha 2\mods\PrismaHUD\`. Full postmortem and the general lesson
  (verify a hand-computed RVA against the *deployed* value, not just re-derive it) in
  `ReverseEngineering\03-FINDINGS.md`.
  Files: `Projects\Prisma\UI_Systems\PrismaHUD_F4\src\HUDDataProvider.cpp`,
  `ReverseEngineering\03-FINDINGS.md`, `docs\findings\prismahud.md`.

---


## 2026-07-07 (pt4) — PrismaHUD: vanilla HUD suppression fixed for real — native CanBeVisible VTable patch

- **The pt3 SetVariable-based hide did NOT work** (confirmed in-game via screenshot — vanilla ammo/AP
  still rendered). Root cause: each widget's own `UpdateDisplayObject()` re-derives Scaleform `_visible`
  from its virtual `CanBeVisible()` every engine tick, far faster than any poll could win the race.
- **User wrote a new Ghidra tool, `VTableDump.java`**, to dump a named vtable's raw function-pointer
  slots directly (`FW_Query.java` only searches Function symbols, so it can never find a virtual a class
  *doesn't* override). Found: `HUDComponentBase::CanBeVisible` is a real virtual at **slot [7]** in every
  `HUDComponentBase`-derived vtable (`HUDAmmoCounter`/`HUDExplosiveAmmoCounter` inherit it unmodified;
  `HUDCompass` has its own override at the same slot).
- **Replaced the SetVariable code with a one-time native VTable patch**, `InstallVanillaHUDSuppression()`
  (`HUDDataProvider.cpp`, called once from `main.cpp` kGameDataReady): patches slot 7 of
  `VTABLE_HUDAmmoCounter`/`VTABLE_HUDExplosiveAmmoCounter`/`VTABLE_HUDCompass` (RVAs `0xd13fb8`/
  `0xd14038`/`0xd14528`, 1.10.163 OG-only, gated behind `REX::FModule::IsRuntimeOG()`) to always return
  false. Permanent, one-shot — no poll, no GFx race. Built + deployed to
  `E:\Modlists\Fallen World Alpha 2\mods\PrismaHUD\`.
  Full decompiled `CanBeVisible` logic + vtable RVA table in `ReverseEngineering\03-FINDINGS.md`.
  Files: `Projects\Prisma\UI_Systems\PrismaHUD_F4\src\HUDDataProvider.{cpp,h}`,
  `Projects\Prisma\UI_Systems\PrismaHUD_F4\src\main.cpp`, `ReverseEngineering\03-FINDINGS.md`,
  `docs\findings\prismahud.md`, `ReverseEngineering\Scripts\VTableDump.java` (new).
  [OPEN] Not yet in-game verified (pending the user's next test pass, same session as the compass
  jitter/quest-blip retest already queued).

---


## 2026-06-30 (pt7) — PrismaHUD_F4 full code review + critical fixes

Three parallel reviewers (C++ correctness, silent-failure, HTML/JS views) audited the whole plugin; findings
documented in `UI_Systems/PrismaHUD_F4/docs/CODE_REVIEW_2026-06-30.md`. Fixed + built + deployed:
- **Astral-emoji hard-freeze** — `hud_quickloot.html` (🔫🛡💉🎯📦🔧🔥📏💀💰, rendered on every container open) and
  `hud_editor.html` lock button (🔒🔓) contained 4-byte chars that hard-freeze the game (known Ultralight bug).
  Replaced with BMP glyphs; verified 0 astral chars remain in any view.
- **Stack double-counting** — `ForEachStack` invokes its callback once per stack, but the callbacks also re-walked
  `nextStack`, so multi-stack items were summed ~k(k+1)/2× (inflated ammo reserve / grenade counts) and quick-loot
  emitted a duplicate row per stack. Fixed at all 4 sites (ammo/grenade sum one stack; quick-loot processes per item).
- **`equippedItems` data race** — now iterated under `equippedItemsLock` (BSAutoLock) in ammo + grenade builders.
- **Layout write safety** — `SaveLayoutFile` is exception-safe (detached thread), serialized, and atomic (temp+rename);
  corrupt `layout.json` is moved to `.corrupt` and logged instead of silently resetting the user's whole layout.
- **`CreateView` guard** (skip phantom view-0), **`RegisterWidget` id-dedupe** (honor reload/replace contract),
  vtable-mismatch + AV-throw diagnostic logs, QuickLoot stale-handle take feedback, and removed 114 lines of dead
  `#if 0 BuildQuickLootJSON`. Deferred by design: the GFx probe stays until it captures the vanilla-ammo path once.


## 2026-06-30 (pt4) — Ghidra-OG restored + compass quest-blip alias bug fixed

**Tooling** — restored a working FO4 1.10.163 reverse-engineering pipeline. The repo's bundled Ghidra
was **jar-stripped** (0 .jar files → couldn't launch) and the "ready" project had **never actually been
named** (only auto-analyzed; 12/199,088 functions named). Downloaded the matching `ghidra_12.1_PUBLIC_20260513`
to `E:\F4SE OG\ReverseEngineering\Ghidra\ghidra_12.1_PUBLIC`, installed `pyghidra` into Python 3.14, ran `CommonLibImport_F4_OG.py` on a
no-space scratch copy (`E:\F4SE OG\ReverseEngineering\Projects\fwgh_proj_OLD_packed_reference_only`) → 14,009 `Class::Method` functions. Query pipeline = `FW_Query.java`
via `analyzeHeadless` (~1–2 min/lookup). Original project on E: untouched. (See `reference_ghidra_og_setup` memory.)

**PrismaHUD_F4** — `src/HUDDataProvider.cpp`. First fix from the restored pipeline:
- **Compass quest blip pointed at the wrong target / none.** `QuestTargetLite` assumed `TESQuestTarget`
  was `{ int32 alias @0x00; uint32 flags }`. Verified against the named binary + CommonLibSSE (byte-identical)
  + FO4 `TESCondition==0x8`: the real layout is `flags@0x00, conditions@0x08, alias@0x10, sizeof 0x60`. The
  old code read the flags byte (0/1) as the alias → `GetAliasedRef(0/1)` → wrong/missing blip. Corrected
  `alias` to `0x10` with `static_assert(offsetof==0x10)`. Built (xmake, 22s) + deployed to live MO2.
- **Build note:** the 2026-06-30 workspace reorg (`Prisma`→`Projects\Prisma\`, `CommonLibF4` stayed at root)
  broke `includes("../../../CommonLibF4")`; build with `COMMONLIBF4_PATH=E:\F4SE OG\CommonLibF4` + delete `build\`.
- **xmake.lua path fixed permanently** (reorg confirmed permanent): include default `../../../CommonLibF4`
  → `../../../../CommonLibF4` (4 levels up). Builds clean now with no env override.
- **Compass redesigned** — `assets/views/hud_compass.html`. Was 600×50 with a footer panel + edge gradient
  fades, and stuttered because it animated `strip.style.left` every frame (forces full layout/repaint of
  the 144-tick row in Ultralight). Rebuilt as a minimal tick tape: no panel/gradients/footer, 420×24,
  cardinal letters + minor/major ticks only, and the tape is one GPU layer moved via
  `transform:translate3d(...)` each frame (no per-frame layout) → glides instead of stuttering. Deployed.


## 2026-06-30 (pt3) — PrismaHUD_F4: fix in-game misalignment, compass stutter, icons

**PrismaHUD_F4** — `assets/views/hud_*.html` (all 9), `src/HUDLayoutManager.cpp`, `src/HUDDataProvider.cpp`, `src/main.cpp`, `assets/icons/`.

- **Misalignment root cause** (health/survival squished, "align does nothing"): the per-view 50ms `applyPos(DEFAULT)` timer fired unconditionally and overwrote whatever position C++ pushed. Added a `_posSet` flag so the default timer self-skips once a real position arrives, and C++ now pushes the position on each view's DOM-ready. Saved layouts are now authoritative — no race.
- **Corrupt layout data**: `MigrateToFractions` mistook corrupt fractions (`>1.0`) for legacy pixels and divided by 1080 (teleporting widgets to y≈0). Fixed threshold (`>2.0`) and added `IsSanePos()` rejection on load/report/save + a 0-viewport guard in every view. Reset the user's corrupt `layout.json`.
- **Compass stutter**: was repainting every 250ms with no interpolation. Now eases the displayed heading toward a target every `requestAnimationFrame` (correct 360° wrap), polls at 100ms, and the C++ cell/quest scan is throttled to every 5th call so the faster poll stays cheap.
- **Icons**: copied the Rad Mod icon set (RGBA/alpha) into PrismaHUD; wired health→heart-rate, rads→radioactive, AP→running-man, **ammo→ammo, grenade→hand-grenade** as tinted mask icons (ammo.png is palette+tRNS, grenade RGBA — both valid masks).
- **"No big black menus"**: removed the dark panel backgrounds (`rgba(12,14,18,0.72)` + backdrop-blur + border) from the always-on widgets — ammo, grenade, compass, survival, interaction — so they float as clean icon+text/tape readouts with text-shadows for legibility. Grenade default widened 80→96px for the icon. Quickloot keeps a panel (contextual loot list, not always-on).
- Verified by an independent review: align JS, the move→LiveMove→ApplyToView path, and the QuickLoot transfer are all correct.


## 2026-06-30 (pt2) — PrismaHUD_F4: interactive quick-loot, suppress vanilla compact ContainerMenu

**PrismaHUD_F4** — new `src/QuickLootController.cpp/.h`; `src/main.cpp`, `src/HUDDataProvider.*`, `assets/views/hud_quickloot.html`.

- Reverse-engineered "Jack's Disable Quick Loot.esp" (Reference_Tools\Mod) with the FO4RecordEditor MCP — it's pure data: a Quest PlayerRef-alias spell granting a hidden perk with `Activate`/`ReplaceDefault` "Search" entries. Used it to understand the suppression, then implemented in C++ instead (user's choice: self-contained, no ESP).
- **Hijack the Activate key**: `QuickLootInput : RE::PlayerInputHandler`, front-inserted into `PlayerControls::handlers`, consumes Activate/wheel via `event->handled = kStop` — so the vanilla green compact ContainerMenu popup never opens. Tap = take highlighted, hold (≥0.35s) = take all, mouse-wheel = navigate, **Shift+Activate** = fall through to the full vanilla transfer menu.
- **Transfers** via `TESObjectREFR::RemoveItem({obj,count}, reason=kStoreContainer, otherContainer=player)`.
- `QuickLootController` owns the targeted container (`Console::GetPickRef`), the ordered item snapshot (indices match the JS list), selection, and pushes `updateLoot` (now with a `sel` field the view honours).
- Deployed: created `mods\PrismaHUD` and enabled it (`-PrismaHUD` → `+PrismaHUD` in modlist.txt).
- **Not yet in-game tested** — verify the front-insert + `handled=kStop` actually blocks the vanilla ActivateHandler, and that transfers behave for stacks/legendaries.


## 2026-06-30 — PrismaHUD_F4: newest-PrismaUI sync + bug sweep (graphical + concurrency)

**PrismaHUD_F4** — `src/PrismaUI_F4_API.h`, `src/HUDLayoutManager.cpp/.h`, `src/main.cpp`, `assets/views/hud_editor.html`, `assets/views/hud_compass.html`.

- Synced PrismaUI API header to newest **V5** (adds `IVPrismaUI5`); plugin still requests `IVPrismaUI4`. Builds clean on xmake.
- **Concurrency:** `SaveLayoutFile()` was reading the shared widget/visibility maps with no lock from the detached save thread — a data race that could crash. Added a `shared_lock` to match `SaveSlotsFile`.
- **Editor WYSIWYG:** drag boxes rendered 60px below the live HUD widgets (overlay offset vs full-viewport coords). Set `#overlay { top:0 }` so boxes sit exactly on the real widgets.
- **Compass:** cardinal labels were ~27px off the centre notch (centred on root width not strip-wrap content width, and on tick edges not label centres). Rewrote tape + quest-blip math to use the measured container width and half-tick label centring.
- **Visibility model:** split transient JS auto-hide (`reportVisibility`) from the persisted editor eye-toggle — added `m_runtimeVisible`/`SetRuntimeVisible` + `ApplyEffectiveVisibility`. Fixes wrong eye state in the editor, users being unable to permanently hide auto-hide widgets, and constant gameplay disk writes.
- **Survival widget** default height 48→58 /1080 to match its rendered HTML.
- Notes: emoji confirmed safe in Ultralight (quickloot left as-is); `hud_container.html` is an orphan; live MO2 deploy path is `E:\Modlists\Fallen World Alpha 2\mods\PrismaHUD\` (not yet installed).


## 2026-06-08 — PrismaHUD_F4: tactical look, working compass quest direction, cmake to xmake

Took the sleek tactical HUD look (sloped bars, severity-tinted icons, value-shows-at-75%) that was
built in the Rad Overhaul and ported it into the standalone PrismaHUD_F4 vitals widgets (health, AP,
rads), keeping each widget's existing data/layout contract so the in-game HUD editor still works. Icons
are inlined as sub-1KB base64 data URIs, so there is no icon-path or deploy risk.

Finished the compass: the heading and location name already worked; implemented the quest-marker
bearing in HUDDataProvider (iterate the player's displayed objectives, resolve each target with
GetAliasedRef, output the nearest target's absolute bearing for the compass blip). The only unverified
piece is a small local TESQuestTarget layout that needs an in-game check.

Build infrastructure: the workspace CommonLibF4 became xmake-only, which broke the cmake build for
PrismaHUD_F4 (and would break a fresh configure of every cmake Prisma plugin). Converted PrismaHUD_F4
to xmake and switched its logging from the missing F4SE::log to spdlog directly. Builds clean and was
deployed as a new standalone MO2 mod folder "PrismaHUD" (not auto-enabled).

Still to do: the rad overhaul registering its tissue widget with PrismaHUD, and keeping the ghoul
overlay in the rad overhaul.

Files: UI_Systems/PrismaHUD_F4/xmake.lua (new), src/PCH.h, src/main.cpp, src/HUDDataProvider.cpp,
cmake/commonlibf4.cmake, assets/views/hud_health.html, hud_ap.html, hud_rads.html;
docs/FINDINGS.TXT, docs/COMMONLIB_FINDINGS.md.


## 2026-06-02 — PrismaHUD_F4 full in-game editor redesign (spec + plan)

- **Design spec** committed at `docs/superpowers/specs/2026-06-02-prismaHUD-full-editor-design.md`
- **7 C++ bugs diagnosed** across `src/main.cpp`, `HUDLayoutManager.h/cpp`, `HUDDataProvider.cpp`:
  API type mismatch (IVPrismaUI2* vs IVPrismaUI4*), 3× threading violations
  (RegisterJSListener used for game-state events), duplicate listener re-registration,
  file I/O inside mutex, health JSON missing rads data, stale absolute-pixel coords in layout.json
- **Architecture decisions locked in**: fractional coordinate storage (0.0–1.0), HUDEditorState
  class (4 named save slots + widget visibility map), undo/redo JS-only (50-entry stack),
  alignment tools, BindUIEvent-only for all editor JS→C++ events, coordinate migration in LoadLayoutFile
- **New widgets designed**: hud_quickloot.html (full Quick Loot panel replacing hud_container.html),
  all 9 HUD views redesigned in Clean Modern Dark style, compass enriched with location + quest marker
- **Quick Loot system**: keyboard-driven (Up/Down/E/R), rarity dot + FA category icon + stat icons,
  C++ side provides BuildQuickLootJSON + BindQuickLootUpdate + BindQuickLootNav, weight tracking real
- **Font Awesome Solid**: to be bundled locally (fa-solid-900.woff2 + all.min.css in assets/)
  for in-game Ultralight rendering — same approach used in FalloutChat
- Implementation not yet started — plan file is next step

---


## 2026-06-02 — PrismaHUD GPU contention fix (second-monitor video lag)

### PrismaHUD — dirty-check prevents unnecessary Ultralight re-renders
Root cause: 9 Ultralight views × ~66 JS→C++→JS round trips/sec = GPU constantly
re-rendering D3D11 textures for identical data, starving DWM and hardware video decode.

- `HUDDataProvider.cpp` BIND macro: added `static std::string s_last` per lambda.
  Skips `InteropCall` (and the GPU re-render it triggers) when JSON is unchanged.
- `BuildContainerJSON`: replaced `std::sqrt` with squared-distance comparison.
- All 9 HTML views: throttled `setInterval` from hyper-frequent to sensible rates
  (health 100→250ms, ammo 100→200ms, container 500→1500ms, survival 500→2000ms, etc.)
- Also created `COMMONLIB_FINDINGS.md` — persistent research log for confirmed
  CommonLibF4 struct layouts and API availability.

**Files:** `src/HUDDataProvider.cpp`, `assets/views/hud_*.html` (9 files)
**Built and deployed:** `do_build_hud.bat` → MO2

---


## 2026-06-02 — Open-issues sweep (all FINDINGS items)

### FalloutChat — double cursor fixed
**Problem:** Both OS cursor and game GFx cursor visible simultaneously when chat was open.
`Focus()` releases mouse capture so Windows shows its cursor on top of the GFx cursor.
**Fix:** Added `while (::ShowCursor(FALSE) >= 0) {}` after `Focus()` to force the OS cursor
hidden regardless of Windows ref-count. `while (::ShowCursor(TRUE) < 0) {}` on close restores it.
The existing `CursorMenu->uiMovie->SetVisible(false)` hides the GFx cursor in parallel.
- `Plugins/FalloutChat/src/ChatUI.cpp` — ToggleChat()

### PrismaHUD — hunger/thirst now live from game AVs
**Problem:** Survival food/water bars always showed 0% — `av->hunger` / `av->thirst` do not
exist on CommonLibF4's `ActorValue` singleton; they're race/survival-mode-specific forms.
**Fix:** Lazy first-call lookup via `TESForm::GetFormByEditorID<RE::ActorValueInfo>("Hunger")`
and `"Dehydration"`. Cached as file-level statics; gracefully returns 0 if not found (no
survival mode). `GetAV(player, &s_avHunger)` reuses the existing null-safe AV helper.
- `Prisma/PrismaHUD_F4/src/HUDDataProvider.cpp` — BuildSurvivalJSON()

### PrismaHUD — interaction prompt now reads crosshair target
**Problem:** `BuildPromptJSON` returned an empty stub — widget always hidden.
**Fix:** `RE::Console::GetPickRef()` (REL::ID 170742) returns the current crosshair
`ObjectRefHandle` updated each frame by the engine's activation detection code.
`base->As<RE::TESBoundObject>()->GetActivateText(ref, result)` (vtable index 0x60) extracts
the activation string ("Activate [Name]", "Open", etc.).
- `Prisma/PrismaHUD_F4/src/HUDDataProvider.cpp` — BuildPromptJSON()

### PrismaCharacterCreator — FaceMorphProvider wired
**Problem:** Face Regions tab always empty — TESRace field path was unknown (TODO comment).
**Fix:** Confirmed path: `race->faceRelatedData[sex]->facialBoneRegions`
(`BSTArray<BGSCharacterMorph::FacialBoneRegion*>*`). Each region's `id`, `BGSLocalizedString name`,
and per-axis min/max ranges are extracted by aggregating all `boneMinMaxMap` entries.
Chargen morph groups wired via `faceRelatedData[sex]->morphGroups`
(`BSTArray<BGSCharacterMorph::Group*>*`) — group name, index-as-id, and all preset names/ids.
- `Prisma/PrismaCharacterCreator_F4/src/FaceMorphProvider.cpp` — BuildFromActor()

### PrismaCharacterCreator — TintProvider now enumerates available layers
**Problem:** Tint layers list always empty. `npc->tintingData` (`BGSCharacterTint::Entries*`)
is not defined in CommonLibF4, so current applied values are inaccessible without F4EE.
**Fix:** `GetEntries` now iterates `race->faceRelatedData[sex]->tintingTemplate`
(`BGSCharacterTint::Template::Groups*` — IS defined in CommonLibF4). All available tint
layers are listed with name, slot, uniqueID. Default HSV 0/0/100 (white) since current
applied colors still require F4EE runtime offsets.
- `Prisma/PrismaCharacterCreator_F4/src/TintProvider.cpp` — GetEntries()

### PrismaMCM — playerName injected into mcmInit payload
**Problem:** `State.playerName` always fell back to "SOLE SURVIVOR" — C++ never sent it.
**Fix:** Added player name lookup via `TESFullName::GetFullName(*npc)` on the player's TESNPC,
stored as `root["playerName"]` in `BuildPayload`. Used in the nuke easter egg missile report.
- `Prisma/PrismaMCM/src/ConfigScanner.cpp` — BuildPayload()

### PrismaMCM — dead `window.prisma.call` branch removed
**Problem:** `prismaCall()` had a dead `if (window.prisma && window.prisma.call)` branch —
`window.prisma.call` is not a method in PrismaUI's Papyrus bridge API; the branch never fired.
**Fix:** Removed the dead branch from `mcm.js` and `button.html`. Only `window[name](arg)` remains.
- `Prisma/PrismaMCM/Interface/PrismaMCM/mcm.js` — prismaCall()
- `Prisma/PrismaMCM/Interface/PrismaMCM/button.html` — openMCMUI()

### PrismaMCM — dead `#debug-cursor` CSS removed
**Problem:** 43-line `#debug-cursor` CSS block (amber crosshair overlay) was in production `mcm.css`
but the corresponding HTML element had already been removed. Dead code.
**Fix:** Entire block removed: `#debug-cursor`, `::before`, `::after`, `.cursor-dot` rules.
- `Prisma/PrismaMCM/Interface/PrismaMCM/mcm.css`

### Reference tools — x64dbg label database export script
**What:** New Python script generates a `Fallout4.dd64` x64dbg label database from all 24,777
named OG symbols. Place next to `Fallout4.exe`; x64dbg auto-loads it on attach — full function
name resolution during live debugging with zero plugin required.
**How:** Calls `collect_relocations_og()` from existing `reloc_parser_og.py`, pairs each symbol's
RVA with image base `0x140000000`, writes x64dbg `.dd64` JSON format. Closes bd `f4se_og-ivv`.
- `Reference/BethesdaGhidraScripts/scripts/commonlibf4/export_x64dbg_labels.py` (NEW)

---


## 2026-06-02 — PrismaHUD_F4 full-editor redesign implementation

### C++ bug fixes (7 bugs from 2026-06-02 design spec)
- **Bug 1.1** — `HUDLayoutManager::m_api` changed from `IVPrismaUI2*` to `IVPrismaUI4*`; `SetAPI()` signature updated. Enables `BindUIEvent` calls.
- **Bug 1.2** — `OnEditorDomReady`: all 3 editor listeners (moveWidget, saveLayout, closeEditor) switched from `RegisterJSListener` (Ultralight thread) to `BindUIEvent` (game thread). Eliminates data races on `m_savedPos` and `SaveLayoutFile`.
- **Bug 1.3** — `BuildEditorPayload()` now emits `defaultX/Y/W/H` from `WidgetDesc`. "Reset to default" in editor JS now uses actual recorded defaults.
- **Bug 1.4** — `bool m_editorListenersRegistered` guard added. `OnEditorDomReady` skips registration on second call (view survives reload). Reset when view handle is recreated.
- **Bug 1.5** — `SaveFromJS()`: snapshots `m_savedPos` under unique_lock, releases lock, then writes to disk outside the lock. Prevents game thread stall during file I/O.
- **Bug 1.6** — `g_editorOpen` changed to `std::atomic<bool>`. Eliminates write race between key handler and old Ultralight-thread paths.
- **Bug 1.7** — `BuildHealthJSON()` now includes `rads` and `radsMax` fields. Health bar rads overlay now renders correctly.

### Architecture additions
- `HUDLayoutManager`: new methods `SetWidgetVisible`, `GetSlotsPayload`, `LoadSlot`, `SaveSlot`. New persistence: `slots.json` (4 save slots) and `"visibility"` key in `layout.json`.
- `HUDLayoutManager`: fractional coordinate system throughout (0.0–1.0). Old `layout.json` files with values > 1.0 auto-migrated on load.
- `HUDLayoutManager`: `SlotData` struct (renamed from `SaveSlot` to avoid method name collision in MSVC).
- `main.cpp`: new `BindUIEvent` listeners — `setWidgetVisible`, `loadSlot`, `saveSlot`, `reportVisibility` (per widget).
- `main.cpp`: MAKE macro defaults converted from absolute pixels to fractions. `initSlots` payload sent on editor open.
- `HUDDataProvider.cpp`: `BuildCompassJSON` enriched with `location` (cell name) and `questDir` stub (-1.0).
- `HUDDataProvider.cpp`: `BuildQuickLootJSON` + `BindQuickLootUpdate` added. Container renamed to `prisma_quickloot`.
- `CMakeLists.txt`: `hud_container.html` → `hud_quickloot.html`.

### HTML redesign — Clean Modern Dark (all 9 widgets + editor)
All widgets converted to fractional `setHUDPosition` (multiplied by `window.innerWidth/Height`).
Each auto-hiding widget calls `window.reportVisibility` on state change.
- **hud_health.html** — glassy dark background, accent left bar (green), rads overlay, thin-weight numbers.
- **hud_ap.html** — blue accent bar.
- **hud_compass.html** — scrolling tape, location name footer, quest blip dot.
- **hud_rads.html** — auto-hides when rads == 0.
- **hud_ammo.html** — auto-hides when no weapon equipped.
- **hud_grenade.html** — auto-hides when no grenade/mine equipped.
- **hud_survival.html** — auto-hides when all AVs are 0 (non-survival game).
- **hud_interaction.html** — key badge + action text + target name, auto-hides.
- **hud_quickloot.html** — new file replacing hud_container.html. Full quick loot panel: header (icon+name+weight), item list (rarity dots, category icons), carry weight bar, keyboard hints.
- **hud_editor.html** — full rewrite: alignment bar (8 ops), save slots (4), visibility eye toggles, undo/redo (50-entry stack), fractional coord inputs, snap dropdown (Off/5/10/20px), panel collapse arrow, multi-select via Shift+click, Tab to cycle widgets.

### Open items
- Quick loot item weight/value/stats APIs unverified — returns 0 for now.
- Compass `questDir` always -1.0 — active quest marker bearing not yet implemented.

---


## 2026-06-02 — PrismaHUD_F4 quest direction RE investigation

### Research: compass questDir implementation path
Investigated the full RE path needed to implement the compass quest marker blip.

- `BGSQuestObjective` and `QuestTargetArray` confirmed forward-declared only in CommonLibF4 — no field access possible without raw offsets.
- `BGSRefAlias` and `BGSBaseAlias` bodies confirmed in headers; `GenericFillData` is opaque `uint64_t data[3]`.
- `PlayerCharacter::CheckForQuestTargetUpdate` identified at 0x140D60880 (1.11.191) via `IDAImportNames_1.11.191.0.py`.
- Address library cross-reference: REL::IDs 2232998/2233000 (NG-only, not in OG 1.10.163 bin).
- IDA workflow fully documented in FINDINGS.TXT Section 4: unpack exe → import names → go to 0x140D60880 → F5 pseudocode → paste offsets.
- IDADebugger structural hash script role clarified: for finding OG equivalents after NG analysis, not for struct layout extraction.
- questDir remains -1.0 stub until IDA session is completed.

---


## 2026-07-13 (pt16) — PrismaHUD_F4 quick-loot: workshop-bench duplicate "R Transfer" + missing perk-choice fix

### Bug fixes
- **Duplicate "R Transfer" hint** — the transfer-only view rendered "R Transfer" twice: once as the
  centered `.ql-transfer-msg` in the item-list area, and again via the footer's R key-hint, which
  (unlike every other footer hint) was never wrapped in a class that gets hidden in transfer-only
  mode. Fixed by giving the footer's R hint its own `.ql-hint-transfer` class and hiding it under
  `#root.transfer-only` in `hud_quickloot.css` — the centered message is now the one indicator.
- **Workshop Framework's "Manage Workbench" perk choice never appeared** — `QuickLootController::
  BuildSnapshot()`'s workshop-bench branch early-returns with a minimal `transferOnly` payload
  BEFORE the `activateChoices` computation block ever ran, so any perk-added activate choice on the
  bench (confirmed via Workshop Framework's own MCM config: `ManageWorkbench:FrameworkOptions`,
  "Choosing Manage on a settlement's Workbench will bring up the Manage Settlement menu") was silently
  dropped from the JSON. Fixed by hoisting the `activateChoices` computation above the workshop-bench
  branch in `QuickLootController.cpp` and including it in both the `transferOnly` and normal payloads.
- **Manage-choice number keys were swallowed even once shown** — `QuickLootInput::OnButtonEvent`'s
  workshop-bench input gate only let `isOpenFull` (R) through; `isChoice1/2/3` (number keys 1/2/3,
  the real engine buttonIndex for activate choices) were blocked. Fixed to also allow
  `isChoice1`/`isChoice2`/`isChoice3` through the gate.
- **JS side dropped the choices row unconditionally** — `hud_quickloot.js`'s `transferOnly` render
  branch did `qlChoices.innerHTML = ''` unconditionally instead of calling `renderChoices(d.
  activateChoices || [])`. Fixed to render choices in transfer-only mode too.

### Correction: mod identification
- An earlier session entry attributed "press spacebar to manage the settlement" to Settlement Menu
  Manager. User corrected this — it is actually **Workshop Framework** (kinggath,
  github.com/kinggath/WorkshopFramework). Confirmed via WSFW's own MCM `config.json`
  ("Manage Workbench" alternate-activation switcher, default ON, `WorkshopFramework.esm|15132`).
  WSFW adds its "Manage" option through the same vanilla `ActivateChoiceListener`/perk-driven
  activate-choice mechanism as WH Butchering's "Field Dress" — the existing `activateChoices`
  plumbing in PrismaHUD is the correct integration point (see bug fixes above), no separate
  Settlement-Menu-Manager-specific patch is needed.

### Files changed
- `QuickLootController.cpp` — `BuildSnapshot()` activateChoices hoisted above workshop-bench
  early-return; `OnButtonEvent()` workshop-bench gate widened to allow isChoice1/2/3.
- `hud_quickloot.html` — footer R-transfer hint tagged `.ql-hint-transfer`.
- `hud_quickloot.css` — `.ql-hint-transfer` hidden under `#root.transfer-only`.
- `hud_quickloot.js` — transferOnly branch now calls `renderChoices(d.activateChoices || [])`.

Built + deployed (PrismaHUD_F4.dll + hud_quickloot.html/css/js) to live modlist. Not yet in-game
verified by the user.

---


## 2026-07-13 (pt17) — PrismaHUD_F4: fix vanilla HUDRollover double-rendering the interaction prompt

### Bug fix
- **"The E button keeps moving to different places"** — root cause was NOT our own
  `hud_interaction` widget's position (verified static: no per-poll reposition, only changes
  via the layout editor / persisted `layout.json`). Vanilla's own `HUDRollover` widget
  (`CenterGroup_mc.RolloverWidget_mc`, FO4's built-in "E) <activate text>" prompt for every
  activatable target) was deliberately left unsuppressed on an outdated assumption that
  hiding it would break perk-driven activate choices (WH Butchering's Field Dress, Workshop
  Framework's Manage Workbench). It kept rendering its own centered prompt alongside
  PrismaHUD's `hud_interaction` widget — two independent, differently-laid-out prompts for
  the same target read as "the E button keeps moving."
- Verified via RVA (not guessed) that the assumption was wrong: activate-choice CAPTURE is a
  detour on `ActivateChoiceListener::PopulateData` at vtable RVA `0x2d21120`
  (`VanillaUISuppressor.h` part D) — a completely separate object/vtable from `HUDRollover`'s
  own `CanBeVisible`-suppression vtable at RVA `0x2d192c8`. Hiding the widget's render (slot 7)
  cannot touch the listener's own data-population detour (slot 2, different vtable) — so
  suppressing `HUDRollover` does not affect PrismaHUD's own activate-choice capture/render
  (`QuickLootController`'s `activateChoices` / `GetActivateChoiceLabel`/`TriggerActivateChoice`,
  which surfaces Field Dress/Manage Workbench through its own UI, independent of the vanilla
  widget's visibility).
- Fix: added `"HUDRollover"` to `kSuppressedWidgets` in `HUDDataProvider.cpp`.

Built + deployed (PrismaHUD_F4.dll) to the live modlist. Not yet in-game verified by the user.

---


## 2026-07-13 (pt18) — PrismaHUD_F4: workshop-bench false-negative, PauseMenu leak, component-tag name artifact

### Bug fixes (from user screenshot + report)
- **Workshop bench showed the full item list instead of Transfer-only.** The detection
  (`ref->extraList->HasType<Workshop::ExtraData>()`) was checking the SETTLEMENT's own
  power-grid/tracking state, not "is this a shared-pool workshop storage container" — a
  container can be fully workshop-linked with zero `ExtraData` of its own. Added
  `RE::Workshop::FindNearestValidWorkshop(*ref)` as a second detection path (FormID match
  against the ref itself) — a real native function already proven in this codebase via
  PrismaCrafting's `GetWorkshopSourceRefs()`. This very likely also explains the reported
  "quick container / activate view slow to react" for workshop targets: the false-negative
  meant every 200ms poll ran the full inventory walk against the shared settlement pool
  (easily hundreds of items) instead of the fast Transfer-only path.
- **HUD rendered on top of the ESC/pause menu** — `"PauseMenu"` (verified `RE::PauseMenu::
  MENU_NAME`) was never in `MenuSuppressSink`'s watched-menu list. Added.
- **Item names showing a raw `"{{{Adhesive, Cloth}}}"` artifact** — verified via
  FO4RecordEditor: the record's actual `FullName` literally contains this component-tag
  string (`Modern Replacer - AIO.esp` override on `DuctTape01`). `QLStripBracketTag` extended
  to also strip any `{{{...}}}` run, not just the existing leading `[Tag]` handling.

Built + deployed (PrismaHUD_F4.dll) to the live modlist. Not yet in-game verified by the user.

---


## 2026-07-13 (pt19) — PrismaHUD_F4: merge workshop Transfer/choices into the centered interaction prompt

### Fix
- **"Transfer should show next to E" / "activator button off to different sides"** — the workshop
  bench's Transfer-only mini-panel was a SEPARATE CEF widget from the centered interaction prompt,
  each with its own independently-saved position (confirmed via the live `layout.json`: the prompt
  was saved off-center from earlier dragging). Two widgets, two positions, for what vanilla shows as
  one row. Fix: the workshop-bench branch of `QuickLootController::BuildSnapshot` no longer shows its
  own panel; `WorkshopActivateChoicesJSON()` is merged into `HUDDataProvider::BuildPromptJSON`'s
  payload as an `extras` array (`R` Transfer + any perk choice), rendered inline in
  `hud_interaction.html/css/js` next to the main `E) <action>` text — one widget, one position.
- The user's live saved position for `prisma_prompt` is still off-center — that's their own
  out-of-repo layout data, not something to edit unilaterally; needs the in-game editor's Reset, or
  explicit go-ahead.
- **Investigated, not resolved:** "E button slow to show up" on a plain loose-item target (rules out
  the previous heavy-inventory-walk theory — no container involved). Traced the full pipeline
  end-to-end, found no inefficiency. Logged as an open item pending re-test after this batch of fixes.

Built + deployed (PrismaHUD_F4.dll + hud_interaction.html/css/js) to the live modlist.

---


## 2026-07-13 (pt20) — PrismaHUD_F4: interaction prompt / quick-loot now appear instantly (was 1-2s)

### Fix
- **"Takes 1-2 seconds to show Take, should be instant."** Root cause was NOT a slow poll but a
  **throttled** one: both target-driven views are hidden when there's no target, the framework's
  `Hide()` bottoms out in `display:none` on the view's iframe (CEF shell `hideView()`), and Chromium
  throttles `setInterval` in a hidden frame to ~1Hz. Each view's own JS poll was the only thing
  detecting "a target appeared" — so the view had to notice its own wake-up using a timer the browser
  had throttled *because* the view was asleep. No poll-interval tuning can fix that.
- Fix: event-driven show path. The `PickRefUpdateEvent` sink (the engine's own "crosshair target
  changed" signal) now pushes both widgets immediately on an actual change, via a new
  `HUDDataProvider::PushPromptNow()` and `QuickLootController::OnRequestUpdate()`. `InteropCall` runs
  script directly rather than scheduling a timer, so it lands even on a `display:none` view; the JS
  then reports visibility, which un-hides it. The JS polls remain as steady-state refresh for an
  already-visible (unthrottled) view and as a backstop.
- `BindPromptUpdate` no longer uses the `BIND` macro — its dirty-check state has to be shared with
  `PushPromptNow`, and `BIND` traps `s_last` inside its own lambda.

Built. **NOT yet deployed** — Fallout4.exe was running (DLL file-locked) at build time.

---


## 2026-07-13 (pt21) — PrismaHUD_F4: rollover/prompt double-hint, Pip-Boy leak, prompt ordering; Manage-Workbench deep dive

### Fixes
- **Rollover open => centered E/R prompt no longer shows.** The quick-loot panel already renders its
  own E/T/R/Select hints; the centered prompt was a second copy in a different place. Suppressed
  whenever the panel genuinely owns the screen. The workshop bench stays the deliberate exception
  (the panel doesn't render there, so Transfer/Manage live in the centered prompt).
- **HUD no longer shows over the Pip-Boy.** Added `PipboyMenu` (verified `RE::PipboyMenu::MENU_NAME`)
  to the menu-suppress list.
- **Ordering bug:** the pick-ref sink pushed the prompt BEFORE refreshing quick-loot state, so on the
  frame you looked at a new target the prompt published the previous target's state (bench extras
  missing). Swapped.

### Manage Workbench — deep dive, static trail exhausted, still open
- Confirmed via FO4RecordEditor that `WSFW_ActivationsPerk [015131]` IS the mechanism we hook:
  `EntryPoint=Activate`, `ButtonLabel="Manage"`, conditions `GetGlobalValue(...)==1` and target
  `HasKeyword(WorkshopKeyword)`.
- **Ruled out my own HUDRollover suppression as the cause** (contradicting the old warning comment):
  decompiled `HUDRollover::UpdateDisplayObject` (RVA `0xab1820`) — its `ShowRollover` data path is
  gated on the widget's own dirty flags, NOT `CanBeVisible`, and `ShowRollover` never calls
  `PopulateData` anyway. Hiding the widget cannot break the capture.
- Remaining unknowns are runtime-only. Added a throttled diagnostic to the framework's
  `Detour_PopulateData` logging every call's btn/valid/listener/label. Next in-game pass at a
  workbench will settle whether the engine ever offers the choice (perk/condition problem, not ours)
  or whether our capture drops it (ours).

Built + deployed: PrismaUI_F4.dll + PrismaUI_F4_Host.dll (together, per the ABI rule) and PrismaHUD_F4.dll.

---


## 2026-07-13 (pt22) — PrismaUI/PrismaHUD: Manage Workbench solved — HUDRollover suppression was killing the capture

### Root cause (settled by the pt21 diagnostic, one in-game pass)
A full session at a settlement workbench with the diagnostic build live produced **zero**
`PopulateData btn=` lines — not even `btn=0`. The capture detour was never called at all.
`ActivateChoiceListener::PopulateData` runs **upstream**, as part of building the rollover's data, and
that producer is gated on the widget being able to be visible — so suppressing `HUDRollover` via the
`CanBeVisible` vtable patch (added in pt1) meant nothing was ever populated for us to capture. This
**corrects pt5**, which wrongly "ruled out" the suppression by decompiling only the *consumers* of the
rollover data (`UpdateDisplayObject`, `ShowRollover` — neither calls `PopulateData`, which is true but
irrelevant). The original warning comment in `kSuppressedWidgets` was right; I overrode it.

### Fix
`VanillaUISuppressor::SuppressHUDWidget` now special-cases `HUDRollover` and routes it to a new
visuals-only path: `UpdateDisplayObject` contains exactly one `call ShowRollover` (call site RVA
`0xab18b0` → `0xab19d0`), retargeted via REL trampoline `write_call<5>` to a detour that calls
`HideRollover` (`0xab1c30`) instead. The widget's whole data pass — including the `PopulateData` the
activate-choice capture depends on — runs exactly as vanilla; only the drawing is suppressed. Guarded
by a pre-patch check that the bytes really are an `E8` rel32 CALL to `ShowRollover`. Callers keep the
identical API; PrismaHUD needed no change.

Built + deployed: PrismaUI_F4.dll + PrismaUI_F4_Host.dll.


## 2026-08-07 — PrismaHUD: issue tracker stood up, two backlog items fixed

**Tracking.** `github.com/PRISMA-USER-INTERFACE-FRAMEWORK/PrismaHUD` had no issues at all. Filed the
whole backlog from `CHECKLIST.md` and the plugin's `docs/STATUS.md`: 21 open issues (verification pass,
the Manage-button investigation, gamepad perk button, SPECIAL re-skin steps 2-5, five item-detail card
features, two diagnostics removals, two design decisions, four editor tasks, the off-screen bound, the
GetAsyncKeyState convention, and the uniform-scale verify items) plus 2 closed record issues capturing
the shipped baseline and the 2026-07-17 session's completed work. Added `status:` and `area:` label sets.

**Fixed (commit 5936755, both UNBUILT — no MSVC toolchain on the authoring machine).**

- `#19` — `HUDLayoutManager::IsSanePos` bounded only the widget origin, so a legal edge origin plus a
  large scale left the widget mostly off-screen with no recovery short of deleting `layout.json`. New
  `ClampExtent` bounds `origin + intrinsic * s` to keep 25% on screen per axis; `ApplyToView` clamps into
  locals (shared lock, and it is the live-drag path), while `RecoverOffscreen` mutates the entry and its
  `m_savedPos` JSON at load time so the recovery persists and gets logged.
- `#12` — removed the `s_diag` ammo diagnostic logger and the `eqCount`/`dataNull` locals it was the sole
  consumer of. Kept the `s_vt` vtable-mismatch warning, which is a CommonLibF4 offset-drift regression
  detector rather than a diagnostic.

Reasoning captured in `docs/Code Notes/PrismaHUD.md` (`ClampExtent`/`RecoverOffscreen` and `BuildAmmoJSON`
sections). Next step is a Windows `xmake` build + deploy before either fix is trusted.


## 2026-08-07 (pt2) — PrismaHUD: backlog pass on the game-independent issues

All UNBUILT (Linux session; the Windows partition carrying MSVC/the SDK is not mounted, so `clang-cl`
here has no sysroot). The prior successful build was on Windows -- `host = "windows"`, `vs = "2026"`.

- `#6` quick-loot caps-per-weight (bbdb5fa) — **view-only, no rebuild**: the row JSON already carried
  value and weight. Per-unit ratio, omitted rather than `inf` when either is 0.
- `#13` quick-loot legendary (f2a6e01) — value bands and legendary split into two independent fields.
  Detection via the engine's `ExtraDataList::GetLegendaryMod`; the legendary mod lives on stack extra
  data, not the base form, which is why the existing `nullptr`-instance reads never saw one.
- `#17` slot rename (79aaa87) — was recorded as unstarted, was actually built and broken three ways:
  renaming ran through `SaveSlot` and so destroyed the slot's layout, Escape committed instead of
  cancelling, and `{once:true}` on keydown killed Enter/Escape after the first keystroke. New
  `HUDLayoutManager::RenameSlot` plus a `renameSlot` UI event.
- `#14` rads math — decided (fraction of max-HP reduction) but deliberately NOT coded: the denominator
  depends on whether `GetPermAV(health)` is already rad-reduced, which needs the game or Ghidra.

Reasoning in `docs/Code Notes/PrismaHUD.md`. Lesson recorded in the findings KB: a negative symbol-grep
is a hint, not a finding — it produced one false negative out of four v1-spec tasks.


## 2026-08-07 (pt3) — PrismaHUD: quick-loot detail + layout export/import

- `#10` junk scrap components (8262b77) — `TESObjectMISC::componentData` @0x158, header-verified.
- `#8` weapon ammo type (8262b77) — partial; fire rate and accuracy left open because neither maps to
  an unambiguous field (the only "accuracy" is a bonus, not the displayed stat).
- `#16` layout export/import (3e5c37f) — files in `PrismaHUD\exports\`, with a whitelist sanitizer that
  also breaks reserved DOS device names (`CON.json` resolves to the device, so filtering alone fails).
- `#7` documented as blocked: quest-item, skill-book and already-read all need Ghidra or an in-game
  probe; details in findings/prismahud.md so it is not re-derived.


## 2026-08-07 (pt4) — PrismaHUD: #20, #15, #18-rotate

- `#20` (8325b60) — one `InputMods::ShiftDown` for the whole plugin. Ratified GetAsyncKeyState rather
  than moving to routed events (a latched modifier bool goes stale when an input chain stops
  delivering; a poll cannot). Fixed the real defect: two of six sites demanded LEFT Shift, so
  right-Shift users could not open the editor or keybind menu.
- `#15` (47b83a4) — built-in presets (Defaults/Compact/Large), derived from each widget's registered
  defaults rather than a coordinate table so third-party widgets are covered.
- `#18` (1ff5169) — per-widget 90-degree rotation across all 10 views; schema `{x,y,s}` -> `{x,y,s,r}`.
  Icon-picker half left unbuilt pending a design decision. Note: the widget views do NOT share one
  positioning template (three shapes), so sweeps need per-file assertions.


## 2026-08-07 (pt5) — PrismaHUD: #18 icon picker (closes #18)

Per-widget primary-icon override (511ca32). Curated 7-icon table in C++ rather than the 4180-SVG
game-icons pack; that table is also the security boundary, since SetWidgetIcon accepts nothing outside
it and the filename reaching a view's CSS url() therefore needs no sanitising. Widgets opt in via
reportIconSupport at DOM ready, so the editor never shows a picker that would do nothing — Health, AP
and Rads opt in, being the only three with the single-masked-icon pattern. Persists in layout.json,
re-pushed from ApplyLayout on reload.
