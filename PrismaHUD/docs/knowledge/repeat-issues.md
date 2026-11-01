# Repeat issues across Prisma plugins

> Imported into this repo on 2026-08-07 from the workspace knowledge base
> (`Prisma/findings/repeat-issues.md`). Failure patterns that showed up in two or more plugins. Cross-plugin by nature; kept whole because the lessons apply here.

---

# Repeat Issues (cross-section patterns)

> Prisma findings knowledge base. Split from the former monolithic `Prisma/docs/FINDINGS.TXT`.
> Index: [README.md](./README.md) · Open issues: [open-items.md](./open-items.md) · Recent log: [recent-worklog.md](./recent-worklog.md)

---

================================================================================
SECTION R — REPEAT ISSUES (condensed from all sections above)
These are issues that have appeared in MULTIPLE plugins or sessions.
When you see these symptoms, check here first before full investigation.
================================================================================

REPEAT-1: cmake/commonlibf4.cmake pointing to wrong CommonLibF4 path
  Seen in: PrismaWorkshop, PrismaInventory_F4, PrismaTerminal_F4, PrismaShowcase_F4
  Symptom: Build fails — CommonLibF4 headers not found or linker can't resolve symbols.
  Pattern: cmake path was ../CommonLibF4_OG (does not exist) instead of ../../CommonLibF4
  Fix: In each plugin's cmake/commonlibf4.cmake, set path to E:\F4SE OG\CommonLibF4
       (the standard cmake+vcpkg build, NOT CommonLibF4_OG which is a stale fork).
  Rule: ALWAYS verify cmake path resolves to the correct CommonLibF4 on disk before build.

REPEAT-2: "PrismaUI_F4.dll: incompatible" on F4SE load
  Seen in: User reports, multiple sessions
  Cause A: Old DLL (UsesAddressLibrary flag wrong) → user needs latest build from GitHub.
  Cause B: User on F4SE NG (0.7.x) with OG-only DLL → they need OG F4SE or NG build.
  Distinguish: Check F4SE version in the error dialog title bar.
    F4SE 0.7.x = NG → if they're on OG Fallout 4 they need OG F4SE (0.6.23)
    F4SE 0.6.x = OG → old DLL, point them to latest GitHub release
  User message: "Grab the latest PrismaUI_F4.dll — an earlier build had a flag
    that caused F4SE to reject it. The latest version loads fine."

REPEAT-3: Shared_lock taken while mutating struct fields (data race)
  Seen in: PrismaHUD_F4 (HUDLayoutManager LiveMove, LoadAndApply)
  Pattern: std::shared_lock(mutex) used, then entry.x/y/w/h written → torn write.
  Rule: shared_lock = READ ONLY. ANY write to shared data requires unique_lock.
  Fix: std::unique_lock whenever any field is written, even inside a "mostly read" function.

REPEAT-4: RegisterJSListener callback touching RE:: (game-thread-only) data
  Seen in: PrismaJournal (requestClose, requestNewEntry)
  Pattern: Listener fires on Ultralight thread. RE::Calendar, RE::Actor, g_visible etc.
           accessed directly without dispatching to game thread → UB / crash.
  Rule: RegisterJSListener body: ONLY string/json work + AddTask dispatch.
        RE:: access ALWAYS inside AddTask or BindUIEvent (game thread).

REPEAT-5: JS function called from C++ InteropCall not defined on window
  Seen in: PrismaWorkshop (updatePlayerResources, setPlacingMode, showError),
           PrismaHUD_F4 (setHUDPosition), PrismaJournal (various)
  Pattern: C++ calls InteropCall(view, "fnName", ...) but JS never defines window.fnName.
           Silent no-op — no error in log, feature just doesn't work.
  Fix: For every InteropCall target, verify window.fnName = function(...) {} exists in JS.
  Rule: When adding a new InteropCall in C++, immediately add the stub in JS in the same commit.

REPEAT-6: applyPos / JS init code running before BindUIEvent registers window functions
  Seen in: PrismaHUD_F4 (all 9 views), other plugins
  Pattern: Script body calls a function synchronously. BindUIEvent fires AFTER DOM ready
           callback. Function undefined at call time → TypeError silently swallowed.
  Fix: setTimeout(fn, 50) defers initial call past BindUIEvent registration window.
       Add || function(){} fallback before any call to guard against timing edge.
  Rule: Any JS that calls a window.* function registered by C++ must be deferred.

REPEAT-7: RegisterJSListener used when BindUIEvent (IVPrismaUI4) is available
  Seen in: PrismaDebugger_F4 (all listeners), PrismaJournal (partial)
  Pattern: Plugin requests IVPrismaUI4 but still calls RegisterJSListener for event bindings.
           RegisterJSListener fires on the Ultralight thread — any RE:: access inside the
           callback is UB. Bindings also may not persist on view reuse.
  Fix: Use BindUIEvent when api_ is IVPrismaUI4. BindUIEvent fires on the game thread
       (RE:: access safe directly) AND bindings persist across game reloads on reused views.
  Rule: Prefer BindUIEvent over RegisterJSListener whenever IVPrismaUI4 is available.
        RegisterJSListener is only acceptable when IVPrismaUI4 is not present.

REPEAT-8: JS listener re-registered on every game reload (double-callback risk)
  Seen in: PrismaDebugger_F4 (view reuse path), ItemUIFramework (correctly avoided)
  Pattern: OnPostLoadGame unconditionally calls the DomReady callback on reused views.
           With RegisterJSListener, this creates a second binding that fires in addition to
           the first — every JS→C++ call triggers the callback twice.
  Fix: Add a bool flag (e.g. panelListenersRegistered_) set to true after first registration.
       Guard DomReady callback with early-return if flag is true.
       Reset flag to false BEFORE CreateView when the view handle is invalid (about to be
       recreated). Do NOT reset the flag on the reuse path — BindUIEvent bindings already
       persist, so the flag correctly suppresses redundant re-registration.
  Pattern (C++):
    // OnPostLoadGame:
    if (!view || !api_->IsValid(view)) {
        listenersRegistered_ = false;        // new view, flag cleared before CreateView
        view = api_->CreateView(..., CB_DomReady);
    } else {
        CB_DomReady(view);                   // no-op if flag is true (BindUIEvent persists)
    }
    // CB_DomReady:
    if (self.listenersRegistered_) return;
    api_->BindUIEvent(v, "fnName", callback);
    self.listenersRegistered_ = true;
  Reference implementation: ItemUIFramework GUIEditorManager.cpp (g_listenersRegistered)
                            PrismaDebugger_F4 DebugManager.cpp (buttonListenersRegistered_,
                                                                 panelListenersRegistered_)

REPEAT-9: Drag ghost element uses style.left/style.top instead of transform
  Seen in: ItemUIFramework gui_editor-features.js (moveGhost), PrismaDebugger debug_panel.html
  Pattern: Ghost/drag overlay positioned with style.left/style.top inside (or near) mousemove.
           Even inside a rAF callback, left/top writes trigger layout + repaint each frame.
           transform: translate() is compositor-only — zero layout cost.
  Fix: Replace style.left and style.top on any drag ghost/floating element with
       style.transform = 'translate(' + x + 'px,' + y + 'px)'.
       Requires the element to be position:fixed or position:absolute with left:0, top:0
       as its base (so transform offset maps to screen coords correctly).
  Note: For position:fixed elements, translate(x, y) maps directly to viewport coords —
        no base left/top needed.
  Fixed in: ItemUIFramework gui_editor-features.js moveGhost() (2026-06-01)

REPEAT-10: F4SE plugin missing F4SEPlugin_Query export — OG F4SE rejects the DLL
  Seen in: PrismaMCM (2026-06-17, no MCM pause-menu button); PrismaDebugger_F4 (2026-08-06,
           "the button did not open the menu" — plugin never loaded at all, ever, since its
           2026-06-08 initial commit predates this finding by 9 days); latent risk in any
           plugin using the commonlibf4.plugin xmake rule without Query in source.
  Symptom: f4se.log: "plugin ...\X.dll does not appear to be an F4SE plugin". The plugin
           never loads — its OWN log file is stale/absent while other plugins ran. No crash.
  Cause: The commonlibf4.plugin rule's default template (CommonLibF4/res/commonlibf4-plugin.cpp.in)
         emits the NG F4SEPlugin_Version data export ONLY. OG F4SE (0.6.23) needs the classic
         F4SEPlugin_Query. A DLL exporting Load+Version but not Query is rejected.
  Diagnose: dumpbin /exports X.dll | findstr F4SEPlugin  (need Load + Query + Version)
  Fix (pick ONE Query source — both = duplicate-symbol link error):
    [BEST] Define F4SEPlugin_Query in src/main.cpp (build-system-independent).
    [GOOD] add_rules("commonlibf4.plugin", { ..., plugin_template = .../local-template-with-Query }).
  ANTI-PATTERN: a build .bat that copies the plugin's template over the SHARED CommonLibF4/res
    template — bare `xmake` then loses Query, AND it leaks that plugin's named Query into other
    plugins built from the shared template. Never mutate the shared res file.
  Rule: New F4SE plugin? Verify Load+Query+Version in the built DLL before declaring it works.
  See: COMMONLIB_FINDINGS.md Section 0a; memory reference_og_f4se_needs_query_export.

REPEAT-11: PrismaUI text input (search box) drops typing — view Focus()ed before FINISH-loading
  Seen in: PrismaCrafting (crafting.html), PrismaAmazon/VaultAzon (index.html) — 2026-06-17
  Symptom: player can click a text <input> but typed characters never appear. In-view,
           window.__prismaImeFocusNotify is undefined (log it from an onfocus to confirm).
  Cause: PrismaUI forwards WM_CHAR only while g_isFocusedTextInputActive is true, set by a
         focusin tracker the framework injects ON Focus() — but ONLY when the view's
         isLoadingFinished is true (set in OnFinishLoading, which fires AFTER the DOM-ready
         callback) and it NEVER retries. CreateView() returns synchronously and the page loads
         later, so Focus()ing at menu-open OR at DOM-ready runs the injector too early -> it
         bails -> no tracker -> every key dropped. The injector also runs synchronously on the
         Ultralight worker thread, so it can't catch up.
  Fix: Focus the view on the JS `load` event, not DOM-ready. Page fires window.prisma_loaded()
       on `load` (+~30ms); C++ binds prisma_loaded and Focus()es THEN. By `load`, finish-loading
       has happened and the tracker installs into the real page. Also give inputs
       `-webkit-user-select:text; user-select:text; pointer-events:auto` so Ultralight grants edit
       focus and focusin fires.
  Rule: NEVER Focus a PrismaUI view that has a text input until its page has FINISHED loading
        (JS `load`), not merely DOM-ready. (PrismaMCM/others that focus an already-loaded view
        don't hit this; on-demand views destroyed+recreated each open always do.)
  See: SECTION 8 PrismaCrafting + PrismaAmazon entries (2026-06-17).

REPEAT-13: S7 Papyrus runaway-script storm
  Seen in: E:\Modlists\Fallen World Alpha 2 — multiple sessions
  Symptom: game stutters / freezes while Papyrus log fills disk. Log grows to 900+ MB
    across a session (15M+ lines). Scripts: S7:Effect:ModGlobal, player aura/cloak ability.
  Cause: S7:Effect:ModGlobal OnEffectFinish event triggers repeatedly per frame from a
    player cloak ability stuck re-applying on nearby NPCs.
  Fix: disable S7 System.esp and S7 Skill System - True Skyrim Fix.esp.
  Diagnostic: healthy Papyrus log = few hundred KB per session. Over 10 MB = runaway scripts.
  Rule: Papyrus performance killers are SUSPENDED STACKS (stuck Utility.Wait / WaitFor3DLoad)
    and active queue depth — NOT instance count (65k serialized instances is normal and fast).

REPEAT-14: Scaleform heap crash (Addictol fix — WRONG bScaleformAllocator=false path)
  Seen in: Fallen World modlist — Buffout4/Addictol config errors
  Symptom: EXCEPTION_ACCESS_VIOLATION deep inside MemoryHeapPT::AllocAutoHeap
    (Scaleform DrawTextManager::SetViewport path) = Scaleform heap exhaustion.
  Wrong fix: uScaleformHeapSize and uScaleformPageSize in Buffout4.toml only take
    effect when bScaleformAllocator = false. With that setting the game uses the VANILLA
    fixed-size Scaleform heap — cranking uScaleformHeapSize to 2048 does nothing useful.
  Correct fix: set bScaleformAllocator = true in Buffout4.toml. This replaces the
    fixed-size heap with Addictol's dynamic allocator, preventing the grow failure.

REPEAT-12: CSS `zoom` (or transform:scale) on a PrismaUI panel pushes the game cursor behind it
  Seen in: PrismaCrafting UI-scale slider — 2026-06-17
  Symptom: after adding a `zoom`-based scale control, the vanilla/F4SE mouse cursor renders BEHIND
           the view and text can squish. (The framework draws its own cursor sprite only while a
           view holds input capture; `zoom` changes how the view composites and the engine cursor
           ends up under it.)
  Cause: `zoom`/`transform` create a stacking/compositing context and don't play well with the
         framework's D3D cursor sprite + the Ultralight view surface. Inverse-% compensation to
         keep the box "fixed" also makes text reflow at a tiny width (squish), worst on fixed-width
         or max-width panels.
  Fix: scale the UI by a FONT-SIZE multiplier, not zoom. Put `:root{--ui-fs:1}` and make every CSS
       `font-size: calc(<px> * var(--ui-fs))`; the slider sets `--ui-fs` on the root. No stacking
       context, panel box unchanged, cursor unaffected, text scales and the lists scroll.
  Rule: To make a PrismaUI menu scalable, scale font-size via a CSS var — never `zoom`/`transform`
        on the panel.

REPEAT-15: A None-guarded requirement gate silently passes (never blocks)
  Seen in: WastelandHunter WH_ButcheringQuest.DoHarvest — "Require Tools" did nothing (2026-07-02)
  Symptom: A config toggle is ON but its gate never fires; the guarded action always succeeds.
  Pattern: the gate ANDs in a "is the form wired" guard next to the real test, e.g.
    `If RequireX == 1 && ItemProp != None && GetItemCount(ItemProp) < 1  ; reject`
    When ItemProp is None (deferred / never assigned), `ItemProp != None` is false so the WHOLE
    condition is false and the reject branch is skipped = the requirement is silently disabled.
  Fix: put the None-safety INSIDE the requirement helper and default to the SAFE (blocking) answer
    when the form is unresolved, or don't gate on the optional form at all. Here: a `IsFieldTool()`
    helper that returns true only on a positive match (equipped knife via ma_Knife / WeaponsKnifeList
    / a patch keyword) and false otherwise, so `RequireTools==1 && !IsFieldTool(player)` actually
    blocks. Also audit sibling functions for the SAME missing gate (DoSkin had none at all).
  Rule: a requirement check must FAIL CLOSED. Never let an unresolved optional form flip a gate open.
  RECURRED (2026-07-03, Opus review pass): the SAME pattern was still live in two sibling gates the
    first fix missed — WH_ButcheringQuest.DoHarvest cannibal gate (`ActorTypeHuman != None && ...
    && CannibalPerk != None && !HasPerk`) and DoScrap blowtorch gate (`WH_Cfg_RequireBlowtorch != None
    && ...`). Both now fail closed: the optional props fall back to a runtime GetFormFromFile lookup
    (ActorTypeHuman 0x02CB72, Cannibal01 0x04B259) and an unresolved perk BLOCKS; the blowtorch config
    defaults to REQUIRED when its global can't resolve. LESSON: when you fix one fail-open gate, grep
    the whole script for `!= None &&` next to a `.GetValueInt() == 1` and audit EVERY one, not just the
    reported call site.


REPEAT-17: PrismaUI_F4.dll / PrismaUI_F4_Host.dll ABI version mismatch (shim/Host from different builds)
  Seen in: 2026-07-12 (first diagnosis, ABI 5, OG tree), 2026-07-14 (OG modlist got an NG-branch build
    of the shim deployed over it), 2026-07-17 (the SEPARATE isolated AE/NG test install —
    `E:\SteamLibrary\...\MO2\mods\PrismaUI_F4 (NG-1.11.221 ISOLATED TEST...)`, its own dedicated MO2
    instance — had a same-day ABI-8 shim rebuild deployed while its own Host DLL was still a build from
    hours BEFORE that day's ABI bump; this occurrence had nothing to do with the OG modlist at all).
  Symptom: total blank/no UI, OR every CreateView() blocks for its full timeout then fails. F4SE log
    (`PrismaUI_F4.log`) shows `[CefBridge] Host rejected ABI version N (framework/Host build mismatch)`.
  Cause: `PrismaUI_F4.dll` (shim) and `PrismaUI_F4_Host.dll` (CEF Host) both embed
    `CefHost/CefHostABI.h`'s `kCefHostABIVersion`. Whenever ONE of the pair gets rebuilt/redeployed
    without the other — whether across two DIFFERENT installs (OG modlist vs. the isolated AE/NG test
    install are fully separate MO2 instances, never share files) or just within ONE install's own
    build history (a same-tree ABI bump landing in the shim before the Host got rebuilt), the version
    check fails.
  Detect: grep `PrismaUI_F4.log` for "ABI version" FIRST on any "no UI" / "froze after load" report.
    FIRST confirm WHICH install/MO2 instance actually produced that log (check `gamePath`/mod folder in
    that instance's `ModOrganizer.ini`) before assuming it's the OG modlist — don't guess. Then
    MD5-hash the deployed shim + Host against that SAME tree's own `build\windows\x64\release\` output
    and compare build timestamps to the tree's own ABI-bump commit.
  Fix: rebuild BOTH `PrismaUI_F4` and `PrismaUI_F4_Host` from the correct tree for that specific
    install, deploy both together. The isolated NG tree may need its `build\cef-147.0.14\` SDK copied
    in from the OG tree first (robocopy, ~850MB local copy, faster than re-running
    `setup-cef-sdk.ps1`'s ~320MB download) if its own CEF SDK was never fetched/was cleaned.
  Rule: never assume which physical install/tree a log file came from — verify it (ModOrganizer.ini
    `gamePath`, or by checking which install's DLLs are newest) before diagnosing a cross-tree
    contamination that may not exist. See `docs\Prisma\findings\prismaui-core.md` (dated entries) and
    `docs\Prisma\Prisma Core\limitations.md`.


REPEAT-16: FO4RecordEditor writes ESM masters OUT OF LOAD ORDER -> game hangs on load
  Seen in: WastelandHunter WH_Butchering.esp after DLCCoast.esm was added as a master (2026-07-03).
  Symptom: enabling ONE ESP deterministically hard-freezes FO4 on load (sticks on the loading widget,
    NO crash log). Bisects cleanly to that ESP; check_plugin AND scan_broken_refs both pass CLEAN.
  Cause: the editor saves with MastersListContent=Iterate and never sets MastersListOrdering, so
    Mutagen's default put the DLC ESM before Fallout4.esm (DLCCoast.esm, Fallout4.esm, ...). FO4 needs
    masters in ascending load order (a master's own masters first), so Fallout4.esm MUST precede
    DLCCoast.esm. The engine hangs on an out-of-order master list; the editor tolerates it (resolves by
    its own index map) so every editor scan is green.
  Detect: inspect the RAW header order, not the editor -
    `head -c 2000 X.esp | tr -c '[:print:]' '\n' | grep -iE '\.es[pml]$'`. First real master must be
    Fallout4.esm (or the earliest-loading ESM).
  Fix: re-save via Mutagen with `MastersListOrdering = new MastersListOrderingByLoadOrder(<load order>)`
    + MastersListContent=NoCheck (keep the existing master SET, only reorder; Mutagen re-indexes every
    FormLink). Standalone tool: `Tools\PluginEditTool\mastersort\` (`dotnet mastersort.dll <in> <out>`).
    Deploy under the CORRECT filename (matching internal ModKey) or scan_broken_refs throws phantom
    self-ref "breaks". Full write-up: memory reference_fo4editor_master_order_freeze.
  Rule: after adding any DLC/ESM master in the editor, ALWAYS verify the raw header master order before
    shipping. Ideal permanent fix: patch WriteService.cs to set MastersListOrdering by load order.

REPEAT-17: A lock the other party never acquires cannot serialise access to a SHARED resource
  Seen in: PrismaUI_F4 CEF compositor vs. the Fallout4 render thread, sessions 11-15 of the
    nvwgf2umx crash investigation (2026-07-19, prismacraft.md). Four sessions and three patches were
    spent on a mutex that was structurally incapable of fixing the bug.
  Symptom: you add synchronisation around a resource, the fault does not change AT ALL - same stack,
    same failure hash - so you conclude the lock has a coverage GAP, find a real one, close it, and
    the fault still does not change. Repeat. Each gap is genuine, which is what makes the loop
    convincing.
  Cause: the resource was SHARED, not OWNED. Our `GetSharedD3DContextMutex()` guarded the game's
    D3D11 immediate context, but only Prisma's own call sites ever acquired it. The engine draws
    through the same context from `Fallout4.exe`/the D3D11 runtime - code we cannot modify, so it
    could never be made to participate. A perfect, gap-free version of that mutex would still have
    let every engine-vs-Prisma collision through. It was also only held inside our `Present` hook,
    a sliver of each frame, leaving the rest of the frame unguarded by construction.
  Detect: ask "who else touches this, and does MY lock run in their code path?" If the other party is
    the engine, a driver, a runtime, or another DLL you do not own, the answer is no. A second tell:
    invariance of the failure signature across patches. A real race that is being progressively
    constrained should show shifting timing; a byte-identical failure hash after two independent
    gap fixes means you have not touched the actual collision.
  Fix: use a protection mechanism that BOTH parties are subject to, which usually means one the
    runtime/engine already provides and applies below the caller.
    `ID3D11Multithread::SetMultithreadProtected(TRUE)` moves the lock inside the D3D11 runtime,
    where it covers the engine's calls too - the one thing an application-level mutex can never do.
    Check for such a mechanism BEFORE building your own.
  Rule: before writing synchronisation, classify the resource as OWNED or SHARED. For OWNED, your own
    lock is fine. For SHARED, an application-level lock is the wrong tool by construction - look for
    runtime/engine-level protection first, and prefer removing the sharing entirely (give yourself
    your own device/context/handle) over locking around it.
  Caveat: runtime-level protection is usually BLUNT - it locks the shared resource process-wide,
    including for the party you were trying not to disturb. Correct first, then measure the cost.

## REPEAT-18 — A shallow search is not evidence of absence

2026-07-19, PrismaUI_F4 cross-device paint hop. `find -maxdepth 2` over the modlist returned no
`d3d11.dll`, and that was reported to R. as "no ENB or graphics proxy anywhere in your setup" —
used to kill a correct theory. ENB was in fact present the whole time, in
`mods\<name>\Root\d3d11.dll`, deployed into the game root at launch by MO2 Root Builder and
therefore invisible both to a depth-limited search and to a listing of the game folder while the
game is closed.

Same shape as the earlier ENB confusion in this file's entry on `GetCreationFlags()` being read
through ENB's wrapper: **anything that only exists in the game root while the game is RUNNING
cannot be checked by looking at the game root while it is closed.** MO2's virtual file system and
Root Builder both do this.

Rule: a negative result only rules something out if the search could have found it. Before writing
"there is no X", state where X would live if it existed and confirm the search covered that. For
this workspace that means the full `mods\` tree including `Root\` subfolders, `overwrite\`, and the
enabled-mod list in `profiles\<profile>\modlist.txt` — not the game directory alone.

## REPEAT-19 — A retry counter is the wrong guard for a deterministic failure

2026-07-19, PrismaUI_F4 cross-device paint hop. A fallback existed specifically so a broken paint
path would degrade to the working one instead of showing nothing. It demoted after 30 consecutive
failures. The failure was deterministic — identical every frame — so the counter bought nothing and
cost everything: the shell paints continuously and demoted instantly (so it looked fine), while the
journal opens and paints a handful of times, never reached 30, never demoted, and stayed blank for
the entire session.

Two rules out of it:

1. **Capability is decided once, up front, by attempting the real thing.** If a path either works or
   doesn't for the whole process lifetime, probe it at construction — do not discover it by failing
   during use. The probe must exercise the WHOLE chain (here: create → shared handle → open on the
   *other* device → create the query). Probing only the first step passes on a mismatched adapter
   and then fails at the first frame, which is the same bug wearing a different hat.
2. **A guard tuned on a busy client will not protect a quiet one.** Anything gated on "N events"
   silently never fires for low-traffic instances. Ask what the quietest client does before picking
   a threshold — or don't use a threshold.

Retry counters remain correct for genuinely intermittent failures. That is the distinction: is this
"sometimes fails" or "does not work here"?

## REPEAT-20 — `modlist.txt` is STALE while MO2 is open; verify with the plugin's own log

2026-07-20. Read `profiles/<profile>/modlist.txt` and reported `-Amrel PDA` as disabled, told R. to
skip the PrismaPipboy regression check on that basis. R. checked anyway: the PDA was running fine,
and `PrismaPipboy.log` had been written minutes earlier. The mod was enabled; the file on disk was
out of date.

MO2 flushes `modlist.txt` on exit / profile switch, so **anything toggled in the MO2 UI during a
session is not on disk yet**. Every enable/disable question answered from that file mid-session is a
guess wearing the costume of evidence. This is the third modlist misread in one day.

**Verify enablement from runtime artifacts, not the profile file:**
- `%USERPROFILE%\Documents\My Games\Fallout4\F4SE\<Plugin>.log` — mtime proves it loaded, and when.
- The crash log's loaded-module list, if you have one from the relevant run.
- `plugins.txt` for ESP/ESL activity (`*name.esp`), which is a different question from mod enablement.

`modlist.txt` is still fine for "does this mod exist and where does its content live" — just not for
"is it on right now". Same family as REPEAT-18: a lookup that *can* be stale is not evidence of
absence.

## REPEAT-21 — a generated PCH forwarder can outlive the workspace move that invalidates it

2026-08-07. `PrismaUI_F4`, `commonlibf4` and `commonlib-shared` all failed with a bare `error: PCH.h`
and no compiler output at all — the least informative error in the build system. Cause: xmake's
generated forwarders under `build-wine/.gens/**/PCH.h` still contained

```c
#include "Z:/media/user/Games-Storage/F4SE OG/Projects/Prisma/Framework/PrismaUI_F4_OG/src/PCH.h"
```

i.e. the **pre-2026-06-30 workspace root**, months after the reorg. `build-linux.sh` already knows
about this failure mode — it builds into `build-wine/` precisely because `build/.gens` bakes in
absolute `E:\` paths — but `build-wine/.gens` then accumulates exactly the same staleness the next
time the tree moves. `rm -rf <builddir>/.gens` and rebuild; they are generated files, nothing is lost.

**Generalise:** any *generated* file holding an absolute path survives a move that invalidates it,
and the move is silent. Same family as REPEAT-1 (a build file pointing at a CommonLibF4 path that no
longer exists). When a build fails with an error naming only a file and giving no compiler
diagnostic, `grep -r` the build directory for the old root before anything else.
