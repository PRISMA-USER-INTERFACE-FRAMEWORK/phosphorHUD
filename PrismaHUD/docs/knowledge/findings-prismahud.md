# PrismaHUD findings KB

> Imported into this repo on 2026-08-07 from the workspace knowledge base
> (`Prisma/findings/prismahud.md`). Day-to-day bug history with full root-cause writeups. The authoritative record of what broke, why, and what the fix actually was.

---

# PrismaHUD_F4

> Prisma findings knowledge base. Split from the former monolithic `Prisma/docs/FINDINGS.TXT`.
> Index: [README.md](./README.md) · Open issues: [open-items.md](./open-items.md) · Recent log: [recent-worklog.md](./recent-worklog.md)

---

-- Session 2026-07-17: message-box body glyphs -- neutral keycaps on keyboard (engine limitation) --

[FIXED/LIMITATION] On keyboard with no controller in use, vanilla tutorial message BODIES (e.g. the
  workshop tutorial) still showed PlayStation/Xbox button art. NOT our device logic -- an ENGINE-LEVEL
  behavior: with bGamepadEnable=1 (in the player's Fallout4Prefs.ini) the engine bakes
  <font face='$Controller_Buttons_Inverted'>...</font> glyphs into message text whenever gamepad is
  ENABLED in settings, regardless of physical connection or active use. By the time the string reaches
  us via MessageBoxData the glyphs are already substituted (log-confirmed: it hands us the pre-baked
  font markup), so the user-event that produced each glyph is already gone -- we cannot rebuild the
  keyboard-phrased string, and bGamepadEnable=0 is not an option (it disables controller support wholly).
  RESOLUTION (option 2, honest-incomplete > confident-wrong): when OUR device tracking says keyboard,
  render each baked glyph as a NEUTRAL monochrome keycap (.mb-btnchip) instead of colored controller
  art, making NO keyboard-key claim. A best-effort char->key map was rejected: context-dependent glyphs
  (d-pad-as-category-cycle has no fixed keyboard equivalent) would map to plausible-but-wrong keys, which
  MISLEADS rather than merely being non-specific -- same standard as the BTN chip / unresolved-icon
  fallbacks. Detection reuses the decompiled CG.CHAR table (README-Controllers.md). The bottom nav hints
  (Select/Confirm/Cancel) are our own device-aware code via CG.renderKey and are unaffected.
  PERMANENT ENGINE LIMITATION -- not fixable further without losing controller support; documented here
  the same as other inherent-limitation items this session.


-- Session 2026-07-16 (pt2): CRASH from a misused trampoline API; the real hitch cause --

[FIXED/REVERTED][CRASH][Bug R] The OnActivate entry-detour (added pt1 to stop vanilla's double-take)
  CRASHED the game on taking from a container. Addictol log crash-2026-07-16-13-36-18:
      EXCEPTION_ACCESS_VIOLATION at 0x7FF765E10000
      "Tried to EXECUTE memory at 0x7FF765E10000" (memory not readable)
      RCX: (HUDQuickContainerDataModel*)    <- the 'this' for OnActivate
      R14: (ButtonEvent*)                   <- an Activate press in flight
  ROOT CAUSE -- **write_jmp5 is NOT an entry-detour/trampoline API**. Its implementation:
      const auto original = ASM::JMP5::TARGET(a_src);   // reads a_src AS IF it were already a JMP
      ASM::JMP5 assembly(a_src, allocate_branch5(a_dst));
      REL::WriteSafeData(a_src, assembly);
      return original;
  It REPLACES an existing JMP (thunk/jump-table entry) and returns that jump's OLD target -- exactly
  as write_call5 replaces an existing CALL. It creates NO trampoline for the overwritten bytes. Used
  at a function ENTRY it (a) decoded OnActivate's PROLOGUE as a JMP rel32, making the "original"
  pointer garbage, and (b) destroyed the prologue unrecoverably. Every path that forwarded to the
  original jumped to garbage -> "tried to execute" AV with RCX = the model pointer. Matches the log
  exactly. Reverted; Bug P (vanilla double-take) is OPEN again -- correctly, a crash outranks it.
  **THIS FORK HAS NO ENTRY-DETOUR PRIMITIVE. write_jmp != write_branch.** REL::Trampoline offers only
  write_call5/6 + write_jmp5/6, and BOTH assume the target already holds that instruction. To detour
  an engine function here you must: patch a CALL SITE with write_call5 (which returns a genuine
  original target), patch a vtable slot, or use an engine event. The framework's original CALL-SITE
  approach for ShowRollover was right all along -- only its RVA was stale. (I claimed twice that this
  fork had an entry-detour primitive; both claims were wrong. Corrected here and in the code.)
  NEXT ATTEMPT at the double-take must write_call5 the TakeSelectedItem CALL SITE inside OnActivate
  (case 0/6 of the decompiled switch), never the entry.

[FIXED][PERF][Bug Q] Hitching persisted after the framework log demotion -- a SECOND, separate cause.
  VERIFIED the first fix landed rather than assuming: fresh framework log 2,628,023 -> 161,113 bytes,
  "BindUIEvent fired" 25,828 -> 0, info lines 27,691 -> 1,127.
  The remainder was PrismaHUD's OWN per-poll diagnostics at info, with flush_on(spdlog::level::info)
  = a SYNCHRONOUS DISK FLUSH per line: the compass quest scan (throttled % 6 of a ~250ms poll),
  ammo, and grenade-scan. Measured cadence from the live log: a metronomic ~1.5s
  (+1.505 +1.499 +1.485 +1.514 +1.487 +1.511 ...), i.e. a disk flush every ~1.5s = the hitch.
  Why it read as "CAMERA DESYNC" and not a generic hitch: the compass diagnostic fires INSIDE the
  compass/heading path (its line literally carries heading=130 -> 137 -> 117), so the stall lands on
  the widget tracking camera rotation. Demoted all three to debug. Also deleted the Bug P drift
  diagnostic, which rebuilt a per-item signature string on EVERY BuildSnapshot (~5Hz at a container).
  RULE: with flush_on(info), an info log on ANY periodic path costs frames. Periodic diagnostics
  belong at debug.

-- Session 2026-07-16: vanilla activate button finally suppressible; all notifications -> Prisma toasts --

[FIXED][ROOT CAUSE][Bug P + "takes two/multiple"] Every Activate press took TWO items, the second an
  unrelated row. OUR CODE WAS INNOCENT -- the deployed TAKE trace showed exactly one correct take per
  press ("TAKE sel=0 -> 0x1EC13E 'Shipment of Cloth - 25' x1"). The extra item is VANILLA's:
  HUDQuickContainerDataModel runs its OWN input handling (ShouldHandleEvent/HandleEvent), which a
  PlayerControls handler's handled=kStop does NOT block (already documented here for ContainerMenu), so
  vanilla called TakeSelectedItem on ITS selection while we took ours. Because vanilla's selection is
  not our highlight, this is ALSO the whole of Bug P ("wrong/random item") -- one root cause, not two.
  DECOMPILE-VERIFIED (unpacked 1.10.163), HUDQuickContainerDataModel::OnActivate @ RVA 0xa92350:
      switch (mode @ this+0xf4) {
        case 0/6: ... else TakeSelectedItem(this);        <-- vanilla's take (loot: container/corpse)
        case 1/3/5: PlayerCharacter::ActivatePickRef();   <-- opens another menu
        case 2/7:   TransferBestPowerArmorBattery();      <-- power armor
        case 4:     ToggleWorkshopMode();                 <-- workshop bench
      }
  FIX: entry-detour OnActivate (write_jmp<5>) and skip ONLY while our panel owns the take (live
  container/corpse, latched, not user-hidden, not the bench). All other modes still run, so bench /
  power armor / open-a-menu are untouched. Field Dress is NOT in OnActivate (the perk is a separate
  Space/pad-perk path), so it survives -- that's why blanket-blocking ShouldHandleEvent would have been
  wrong. Guarded: OG-only (raw RVA), and refuses to stack if another mod hooked the entry first.
  LESSON: this is why the diagnostic trace was worth a build instead of a guessed fix -- it proved our
  take correct and redirected the search to vanilla entirely.

[FIXED][PERF] "Visible hitch every second": the FRAMEWORK logged EVERY JS->C++ call at info level
  (BindUIEvent/RegisterJSListener "fired"), and PrismaHUD's logger uses flush_on(info) = a SYNCHRONOUS
  disk flush per line. Every view polls requestUpdate at ~5Hz, so ~10 views produced ~50 disk flushes
  per second. Measured in one session: 27,691 info lines, 25,828 of them "BindUIEvent fired" (25,704
  just "requestUpdate") = 93% of all log volume, 2.6MB of per-poll noise. Pre-existing; the new notify
  view added an 11th poller and pushed it past the perception threshold. FIX: demoted both to debug.
  The framework never calls set_level, so spdlog's default (info) filters them -- verified against a
  live log (zero [D] lines while FocusMenu's logger::debug calls emit nothing). Diagnostics must never
  cost frames; raise the level deliberately when the trace is actually needed.

[FIXED][ROOT CAUSE] The vanilla activate button ("E) <activate text>", HUDRollover) was NEVER actually
  suppressed. SuppressHUDWidget('HUDRollover') returned FALSE while all 8 other widgets returned true.
  Proven from the framework's own log:
    "REFUSING to patch HUDRollover visuals -- the instruction at RVA 0xab18b0 is not a CALL to
     ShowRollover (0xab19d0). Bytes: e8 4b e7 45 ff -> target RVA 0xfffffffffff10000"
  HUDRollover was special-cased onto a ShowRollover CALL-SITE detour whose hardcoded site (0xab18b0) is
  NOT an instruction boundary -- it lands mid-instruction inside HUDRollover::UpdateDisplayObject
  (0xab1820, sizeof=423), so its "E8" was a coincidental byte whose rel32 resolved BELOW the module
  base. The guard refused every time (correctly). NOTE kShowRollover (0xab19d0) was right all along;
  only the call site was junk.
  FIX: deleted the special-case + the whole dead detour block; HUDRollover now uses the SAME vtable
  slot-7 (CanBeVisible) path as every other widget, via its existing kKnownWidgets entry 0x2d192c8.
  No call site to rot, proven mechanism (8/8), keeps its own "slot must point into .text" guard, and
  cannot disturb the activate-choice data pipeline (PopulateData is on a different vtable).
  VERIFIED against the unpacked 1.10.163 dump (not guessed): HUDRollover::ShowRollover 0xab19d0
  (sizeof=597), HideRollover 0xab1c30, ShowActivateButton 0xab1ca0, HideActivateButton 0xab1e40,
  UpdateDisplayObject 0xab1820, vtable 0x2d192c8.
  CORRECTION to an earlier note: this fork's REL::Trampoline DOES have an entry-detour primitive -- it
  is named write_jmp<5>/write_jmp5, not write_branch. The vtable path was still preferred (simpler, no
  trampoline), but "no entry detour available" was wrong.

[NEW] All vanilla HUD notifications now render as Prisma toasts (hud_notify widget, 'prisma_notify').
  Interception: hook on RE::SendHUDMessage::ShowHUDMessage -- the ONE funnel both Papyrus
  Debug.Notification and the engine's own "X Added." messages pass through, so a single hook catches
  everything (hooking the Papyrus native instead would only catch script messages). Address from
  CommonLibF4's Address-Library ID (ID::SendHUDMessage::ShowHUDMessage = {1163005, 2222440}), NOT a raw
  RVA -- deliberately, given the HUDRollover lesson above. The detour never forwards to the original,
  so the vanilla popup never receives the message; no widget suppression needed.
  Burst-safety (hard requirement: no lag, no off-screen clipping): bounded C++ queue (64) drained by the
  view's 200ms poll into ONE InteropCall per tick carrying the whole batch; identical consecutive text
  coalesces to an "xN" counter; JS caps the DOM at 5 nodes and expires via ONE shared 250ms sweep (not a
  timer per toast); fixed width + max-height:44vh + overflow-wrap; animates transform/opacity only (GPU).
  Threading: ShowHUDMessage is called from whatever thread raised the message, so the detour only locks
  and appends -- no RE:: work, no view calls off the game thread.

[REMOVED] The standalone NotificationFilter plugin is now DISABLED in the MO2 profile
  (modlist.txt "+NotificationFilter" -> "-NotificationFilter"). It bound the Debug.Notification Papyrus
  native; running it alongside the new hook would mean two plugins fighting over the same messages. Its
  spam filter (patterns + post-load mute window) was absorbed into NotificationController::LoadConfig,
  read from PrismaHUD_F4.ini [Notifications] -- without that, removing it would have handed every
  "<mod> initialized" spam line straight back as a toast.

-- Session 2026-07-15 (pt2): corpse perk button confirmed live; four follow-up quick-loot bugs --

[CONFIRMED] Corpse-mode perkButtonText population is real: WH's "Field Dress" renders on a radroach
  corpse via the generic EnginePerkButtonLabel() read. Item (1) from pt1 is runtime-confirmed/closed.

[FIXED v2] Bug M (panel overlap): WH's field-dressing menu is its OWN focused PrismaUI overlay (opened
  by WH_Native.ShowFieldDress via the WH_Native DLL bridge -- NOT a vanilla MessageBox/ContainerMenu).
  Quick-loot's ResolveTarget only hid for a visible ContainerMenu, so the panel rendered underneath.
  FIRST FIX WAS DEAD: checked ui->GetMenuOpen("PrismaUI_FocusMenu"), but FocusMenu::Creator is NEVER
  registered with the UI system (grep-confirmed -- no ui->Register call), so that menu name is never a
  queryable open menu; GetMenuOpen always returned false (the cursor works independently via
  MenuCursor::RegisterCursor). This ALSO caused the controller "buttons firing wrong" report: the panel
  stayed active under WH's focused menu, so QuickLootInput kept stealing the buttons meant for WH.
  REAL FIX: gate on the framework's g_focusedView atomic via IVPrismaUI::HasAnyActiveFocus() (in
  IVPrismaUI1, available on all versions) -- true whenever ANY view holds focus. ResolveTarget returns
  null (hides panel + m_active=false releases input); OnButtonEvent also checks AnyViewFocused() live so
  the release is immediate. Quick-loot is passive/never focused, so it never self-triggers.
  LESSON: to detect "is a Prisma overlay focused", use HasAnyActiveFocus(), NOT GetMenuOpen(FocusMenu) --
  FocusMenu is an unregistered helper, not a queryable menu.

[OPEN][MED] Device labels stuck on keyboard while on a controller: ControlLabels device tracking is a
  thin delegate to the framework (IVPrismaUI9). The framework's ControllerGlyphs IS installed (log:
  "input-device tracking installed (MenuControls)") but its sink is on MenuControls (menus only), so
  gameplay device changes must come from PrismaHUD's QuickLootInput.ShouldHandleEvent ->
  ControlLabels::NoteInputDevice -> fw->NoteInputDevice. The framework log shows ZERO "input device ->
  GAMEPAD" change lines, so the gameplay note isn't reaching it. Added a NoteInputDevice diagnostic
  (logs device + frameworkConnected on change) to pin it down next repro. Suspect either the note isn't
  firing on pad input or g_fw is momentarily null.

[FIXED] Bug N (perk button placement): the engine perk button rendered in the separate ql-choices strip
  (above the CARRY bar) instead of the ql-footer action row. Moved inline into ql-footer via
  renderPerkFooter(). Mod-registered choices (RegisterActivateChoice) intentionally stay in ql-choices
  (arbitrary-count opt-in list, kept distinct so the footer can't grow unpredictably).

[FIXED] Bug O (controller Triangle/Y collision -- Prisma cross-wiring, NOT engine-expected):
  ControlLabels hardcoded gamepad perk=Y AND transfer=Y, and QuickLootInput consumed Y (0x18000) for
  OpenFullContainer on a corpse -- so Triangle did Transfer and Field Dress was unreachable on a pad.
  FIX: when a perk button is present on a non-bench pad target, QuickLootInput no longer eats Y (lets
  the vanilla perk fire) and BuildSnapshot clears the pad Transfer hint (no double-label). Keyboard
  (Space/R/E, all distinct) was never affected. NOTE: the gamepad perk button is still hardcoded "Y" --
  UNVERIFIED against the engine (keyboard Space IS confirmed). User to report the real pad button from
  the repro; should then be read live from ControlMap rather than hardcoded.

[FIXED 2026-07-16] Bug P SOLVED -- it was VANILLA taking a second item, not our code. See the
  2026-07-16 entry above for the full root cause: HUDQuickContainerDataModel::OnActivate (RVA 0xa92350,
  decompile-verified) calls TakeSelectedItem on ITS OWN selection in the loot modes, on the same
  Activate press we take ours. handled=kStop in a PlayerControls handler does not block it. Two items
  per press, and vanilla's was an unrelated row -- hence both "takes two/multiple" and "takes the
  wrong/random item". The TAKE trace deployed for this bug is what proved our side innocent (always one
  correct x1 take), which is exactly why the trace was worth deploying instead of guess-fixing.
  bd f4se_og-ke0 closed.

[SUPERSEDED -- see above] Bug P (Take grabs wrong/non-highlighted item, both keyboard & controller): static analysis
  RULES OUT the obvious causes -- not a data race (BindUIEvent -> F4SE AddTask -> game thread, so
  OnRequestUpdate and TakeSelected are serialized), navItem/takeItem are dead stubs (never called by
  C++), and the JS is clean (no sort/filter; _sel = d.sel every push; renderList iterates in order), so
  highlight and take target are the same m_sel BY CONSTRUCTION. Remaining suspects need live data:
  (a) BGSInventoryList::ForEachStack iteration-order drift between polls (m_sel is positional), or
  (b) RemoveItem-by-base-object taking a different instance when one base has multiple stacks.
  Diagnostic logging DEPLOYED (TakeSelected logs m_sel + taken entry + full ordered m_items; BuildSnapshot
  logs poll-to-poll order/content drift). Awaiting the trace from one repro. Log:
  Documents\My Games\Fallout4\F4SE\PrismaHUD_F4.log, grep "QuickLoot TRACE".

-- Session 2026-07-15: rollover perk button made generic for ANY mod + RegisterActivateChoice opt-in --

[FIXED] The Prisma rollover only surfaced a perk-added activate button (e.g. Workshop Framework's
  "Manage") at the WORKSHOP BENCH, never on a corpse/container. Wasteland Hunter's "Field Dress"
  (`WH_Perk_Butcher`, EntryPoint=Activate, ButtonLabel="Field Dress" -- verified via FO4Edit) is the
  identical mechanism as WSFW's Manage, but on a dead actor, so it never appeared.
  ROOT CAUSE (structural, two parts): (a) the corpse/container PANEL's activate list was fed by the
  framework's `GetActivateChoiceLabel` -> `ActivateChoiceListener::PopulateData` capture, which is DEAD
  CODE in 1.10.163 -- the class has zero ctor callers and logged zero PopulateData calls across a full
  session (both vtable slots verified pointing at 0x832870/0x8329a0). (b) the ONE mechanism that works,
  `QuickContainerStateData.perkButtonText` (read from `FavoritesManager`), was gated to
  `IsWorkshopBenchTarget()` only.
  FIX: dropped the dead path entirely; added shared `QuickLootController::EnginePerkButtonLabel()` and
  read it for ALL quick-container targets (corpse/container/bench). The engine populates perkButtonText
  from ANY Activate-entry-point perk generically -- zero per-mod code (grep-confirmed: no mod name/FormID
  checks anywhere in PrismaHUD). Press stays vanilla's (Space / gamepad Y); PrismaHUD renders the hint.
  KNOWN LIMITATION (documented, by design): perkButtonText is a SINGLE engine slot -> one perk button per
  target. Two mods adding a perk button to the same target can't both show (multi-slot ActivateChoiceListener
  is dead). Not a silent gap -- flagged in code + header.
  RUNTIME-CONFIRM PENDING: bench perkButtonText is confirmed live; corpse-mode population is a strong
  inference from the identical mechanism + engine `HasPerkButton(QuickContainerStateData*)` @ 0xa8e020,
  but wants one in-game check on a corpse before it's called proven (user will verify).

[NEW/API] `IVPrismaHUD3` -- `RegisterActivateChoice(id,label,iconId,onTrigger,context)` /
  `UnregisterActivateChoice(id)`, fetched via `RequestActivateChoicesAPI()`. Mod-agnostic opt-in (mirrors
  RegisterWidget) for the two cases the single engine slot structurally can't cover: a 2nd perk button on
  one target, and custom-UI mods with no Activate perk (nothing populates QuickContainerStateData). New
  `ActivateChoiceRegistry` singleton; choices render in the panel + prompt, triggered by number key (panel,
  via QuickLootInput) or click. `TriggerActivateChoice` repurposed from the dead framework capture to the
  registry. LIMITATION: prompt-context (bench/door) registered choices are visual + click best-effort in
  v1 (no gameplay-chain number-key handler there); a mod needing a guaranteed hardware key pairs it with a
  V2 BindingRegistry hotkey. Files: ActivateChoiceRegistry.{h,cpp}, PrismaHUD_F4_API.h, QuickLootController,
  HUDDataProvider, main.cpp, hud_quickloot.{js,css}.

-- Session 2026-07-11: builtin HUD killed (not hidden) during loading/cutscene/main-menu --

[FIXED/NEW] User wanted the always-on battle-HUD widgets (health/ap/compass/rads/ammo/grenade/
  quickloot/survival/prompt) to be fully torn down -- not just CSS-hidden -- whenever the vanilla
  HUD is off (loading screens, cutscenes, main menu), matching the engine's own behavior instead of
  reinventing loading/cutscene/main-menu detection ourselves.
  Prior investigation (same session) into "can we just kill the vanilla HUDMenu movie outright"
  found that's NOT safe: `UI::CloseAllMenus` (decompiled, 1.10.163 OG) hardcodes an EXEMPTION list
  -- HUDMenu, PowerArmorHUDMenu, DebugTextMenu, CursorMenu, ButtonBarMenu are never swept closed by
  the engine's own bulk-close. But the SAME decompile pass found the engine DOES have its own
  native `ShowHUD()`/`CloseHUD()` SendHUDMessage helpers (RVA 0xae1f10 / 0xae1f80) that legitimately
  send kShow/kHide to "HUDMenu" via UIMessageQueue at loading/cutscene/main-menu transitions --
  which is exactly the trigger the user wanted, and it already exists natively.
  IMPLEMENTED: PrismaHUD_F4\src\main.cpp now registers its own `RE::BSTEventSink<MenuOpenCloseEvent>`
  (HUDVisibilitySink, installed in kGameDataReady via InstallHUDVisibilitySink()) directly on
  RE::UI -- no framework (PrismaUI_F4) changes needed, same pattern VanillaUISuppressor's
  VanillaMenuSink already uses successfully. On `menuName=="HUDMenu"`:
    - opening=false (HUDMenu closing) -> DestroyBuiltinViews() calls g_api->Destroy() on all 9
      g_builtin views (not Hide()) and resets g_builtin to all-zero.
    - opening=true (HUDMenu reopening) -> CreateBuiltinViews() (already per-field idempotent via
      its `if (!g_builtin.field)` MAKE-macro guard, so safe to call redundantly alongside the
      existing kPostLoadGame/kNewGame call).
  Only the 9 always-on battle-HUD views are affected; the editor (Shift+F11) and keybinds
  (Shift+F10) overlay views are untouched (already hidden-by-default, user-toggled, not part of
  the "constant HUD" the user was asking about).
  Built clean, deployed to `E:\Modlists\Fallen World Alpha 2\mods\PrismaHUD\F4SE\Plugins\PrismaHUD_F4.dll`.
  [OPEN] Not yet in-game tested -- verify: (1) HUD widgets actually vanish during a loading screen
  and a cutscene and the main menu, (2) they cleanly reappear after each, with no duplicate/ghost
  views and no stale position/state, (3) log lines "builtin HUD views destroyed (vanilla HUD
  closing)" / "HUDMenu reopened -- builtin views recreated" appear at the right moments in
  PrismaHUD_F4.log. Also unverified: whether Pip-Boy or other full-screen menus also toggle
  HUDMenu's kShow/kHide (if so, the HUD would also kill/revive around those -- not necessarily
  undesirable, but untested).

-- Session 2026-07-10: quick-loot E-key take never fired (QUserEvent vs VK idCode) --

[FIXED] Quick-loot: "pressing E does not take the item" (reported worst on single-item containers, but
  the bug was general -- E never took ANY item via the panel).
  ROOT CAUSE (found from the live log, not guessed): in QuickLootInput::OnButtonEvent the take action
  was gated by `ev->QUserEvent() == "Activate"`, but raw keyboard ButtonEvents for E arrive in this
  handler with idCode 0x45 (VK 'E') and a QUserEvent() that is NOT the "Activate" string in the
  crosshair-loot input context -> the gate was always false, so TakeSelected() was never called. Proof
  in PrismaHUD_F4.log: `QuickLoot DIAG: keyboard idCode=0x45 reached handler` fired (E arrives) and R
  (idCode 0x52) fired OpenFullContainer right after (so Active() was true) -- yet ZERO TakeSelected
  lines. R/T/arrows all worked because they are detected by idCode; E was the ONE loot key detected by
  the fragile user-event string.
  FIX: detect Activate by `(device==kKeyboard && idCode==0x45) || QUserEvent()=="Activate"` -- VK code
  first (symmetric with the keys that provably work), user-event OR kept so a rebound/gamepad Activate
  still works. Removed the now-redundant per-key DIAG spam and the verbose TakeSelected entry log.
  LESSON: for PlayerControls button handlers, prefer VK idCode matching over QUserEvent string compares
  -- the userEvent string is context-dependent and can be empty for a key that is otherwise correctly
  bound (idCode here is VK, not DIK).
  Files: QuickLootController.cpp (OnButtonEvent, TakeSelected).

-- Session 2026-07-07 (pt11): master HUD visibility (F2 hide key + combat-only mode) + always-on perf --

[FIXED, pending confirm] Feature request: optimize always-on HUD perf, add a hide-HUD keybind (default
  F2, editable), and a "show HUD only in combat" setting.
  - Master-visibility gate added to HUDLayoutManager: effective master = !manualHidden &&
    (!combatOnly || inCombat), on top of per-widget auto-hide + user eye-pref. ApplyEffectiveVisibility
    now ANDs in the master gate. When it flips, all widget views are shown/hidden.
  - Hide key: main.cpp registers the configurable key (VK, default 0x71=F2) via KeyHandler ->
    ToggleMasterHidden. Editable in the HUD editor (new "HUD SETTINGS" section: click the key button,
    press a key to rebind -> setHideKey; C++ re-registers live). Note KeyHandler uses VK codes here
    (GetBSButtonCode returns VK; 0x7A=F11 confirmed), and manualHidden (F2) is SESSION-ONLY (not
    persisted -- HUD never starts mysteriously hidden). combatOnly + hideKey persist in settings.json.
  - Combat-only: PlayerCharacter::IsInCombat() polled (throttled ~200ms via GetTickCount64) from the
    widget data path; UpdateCombatState re-applies master visibility on change. Verified hidden CEF views
    still run JS timers (Abi_HideView only sets a flag + shell iframe CSS, no WasHidden), so the poll
    keeps firing while the HUD is hidden -> combat can re-show it.
  - PERF: the BIND macro (already had a dirty-check skipping unchanged InteropCalls) now also skips the
    JSON BUILD + push entirely when master-hidden -- a hidden HUD does almost no per-poll work (just a
    throttled combat check + atomic load). LoadAndApply now enforces effective visibility at load so a
    combat-only HUD starts hidden out of combat.
  Files: HUDLayoutManager.cpp/.h, HUDDataProvider.cpp/.h, main.cpp, hud_editor.html. Settings file:
  Documents\My Games\Fallout4\PrismaHUD\settings.json.

-- Session 2026-07-07 (pt10): quick-loot rendered at top-left while its editor outline was correct --

[FIXED, pending confirm] With the editor preview (pt9) now rendering widgets, quick loot showed its
  content pinned to the TOP-LEFT (0,0) while its blue editor outline sat at the correct saved position.
  Root cause: quick loot is the ONLY builtin widget created HIDDEN (main.cpp MAKE hideOnCreate=true).
  Its JS applyPos bails when the view has no layout (`window.innerWidth==0`), which is always true while
  hidden -- so setHUDPosition (at DOM-ready) and the default-position timer both silently bailed and
  #root stayed at its CSS default left:0/top:0. Nothing re-applied the position when the editor later
  Showed it. (Latent: it would also mis-render on its real in-game show.) Fix: (1) C++ ShowAllWidgets now
  calls ApplyToView(entry) right after Show(view), re-pushing position to the now-visible view; (2)
  hud_quickloot.html stores the target as fractions and retries applyFrac (bounded ~2s) until the view
  is sized, so the position lands once layout completes instead of leaving it at 0,0. Files:
  HUDLayoutManager.cpp, hud_quickloot.html.

-- Session 2026-07-07 (pt9): in-game evidence pass -- grenade=pistol root cause, editor "open twice", editor preview for auto-hide widgets --

All three below driven by real in-game log/screenshot evidence.

[FIXED, pending confirm] GRENADE WIDGET SHOWED THE EQUIPPED PISTOL -- the REAL root cause (pt8's
  IsThrownWeapon fix was itself wrong). The grenade-scan trace I added logged, for the Kimber Custom
  pistol (formID 0x4822): `gun=true thrown=true` -- impossible under equality. `REX::TEnumSet::any()`
  is a BITWISE-AND OVERLAP test (`(_impl & args) != 0`), but `RE::WEAPON_TYPE` is a SEQUENTIAL-value
  enum (kGun=9=0b1001, kGrenade=0xA=0b1010) whose values share bits -- so `IsThrownWeapon()` =
  `type.any(kGrenade,kMine)` returns true for a gun. CommonLibF4's IsThrownWeapon/IsGunWeapon/
  IsMeleeWeapon are ALL broken for FO4 for this reason. Fix: compare the EXACT type value
  (`type.underlying() == kGrenade || == kMine`) in `BuildGrenadeJSON`. See
  [[reference_tenumset_any_is_bitwise_weapon_type]] + COMMONLIB_FINDINGS. The trace now logs the raw
  type value (9/0xA/0xB) instead of the misleading helper booleans.

[FIXED, pending confirm] EDITOR "HAD TO OPEN PRISMAHUD TWICE" -- on first open the editor view can be
  shown-but-not-yet-laid-out, so `window.innerWidth/Height` (and the VP captured at script load) read 0;
  every widget box then computes `x*0 = 0` -> invisible boxes / 0.0000 coords in Properties. Fix
  (`hud_editor.html` initWidgets): refresh VP live at call time and, if the viewport isn't sized yet,
  `setTimeout` retry until it is. Second-open worked before because a resize had populated VP by then.

[FIXED, pending confirm] AUTO-HIDE WIDGETS RENDERED AS EMPTY BOXES IN THE EDITOR (Toxicity, Radiation,
  Gas Mask Filter, Survival, Interaction Prompt, Quick Loot, + now Grenade). They only render when their
  data is relevant (no rads/mask/loot target -> opacity:0), so in the editor they were positionable but
  invisible. Added an editor-preview hook: `HUDLayoutManager::SetEditorPreview(bool)` InteropCalls
  `window.setEditorPreview("1"/"0")` on every registered view (framework `InteropCall` short-circuits
  `window[fn] && window[fn](arg)`, so widgets without the fn are a safe no-op -- health/AP/compass/ammo
  need no stub). main.cpp calls it in OpenEditor(true)/CloseEditor(false). Each of the 7 data-gated
  widget views implements `setEditorPreview` to force-show sample content and its data handler now
  honors a `_preview` flag so the poll doesn't stomp it. Files: `HUDLayoutManager.cpp/.h`, `main.cpp`,
  hud_rads/survival/interaction/quickloot/grenade.html, Rad Mod hud_toxic/hud_filter.html.

-- Session 2026-07-07 (pt8): three-bug investigation (evidence-first) -- editor drag, grenade name, Rad Mod widget registration --

Investigated with real log/source evidence BEFORE any fix (three parallel agents + manual verification),
per the user's explicit "report findings only, confirm root cause with evidence first" instruction; then
fixes were greenlit and implemented. All three are built + deployed, NOT YET RUNTIME-CONFIRMED.

[FIXED, pending in-game confirm] BUG 2 -- HUD layout editor: dragging a widget only works again after
  Reset. Root cause (confirmed, not listener loss): the drag box `.wdz` lives in `#overlay` (z-index:10,
  its own stacking context), so a moved box slides UNDER the toolbar (z100) / align bar (z99) / props
  panel (z50) or an overlapping sibling `.wdz`, which then swallow the next mousedown. Reset cured it
  only by relocating the box to an uncovered default spot -- it never re-binds listeners (decisive proof
  it's occlusion, not a lost handler). Logs silent (zero [JS ERR] across a full editor session) =
  pure client-side hit-test bug. Fix (`hud_editor.html`): replaced per-element DOM hit-test with a single
  document-level mousedown that hit-tests widget rectangles manually (selected-widget priority, else
  topmost), bypassing DOM stacking entirely; non-interactive chrome BACKGROUND over a widget now falls
  through to the hit-test (real buttons/inputs still handled), and the selected box is raised within the
  overlay. So a widget under the props panel / top bars / another widget stays grabbable.

[FIXED already (evidence gap closed) + trace added] BUG 3 -- grenade widget showed the equipped PISTOL's
  name. Root cause (confirmed in source + header): the old filter used magic weapon-type `9`; in the real
  enum `WEAPON_TYPE::kGun = 0x9`, `kGrenade = 0xA`, so a gun (Kimber Custom) matched as a grenade. Already
  replaced with `weap->IsThrownWeapon()` (kGrenade/kMine only) since commit db483344, present in the
  deployed DLL. The logs could NOT positively confirm resolution (BuildGrenadeJSON logged no formID, and
  no grenade was equipped in the session -> `eqItems=1` pistol-only -> `hasGrenade:false`, widget hidden).
  Added a throttled `PrismaHUD grenade-scan:` diagnostic (formID + IsGunWeapon + IsThrownWeapon + name per
  equipped WEAP) so a test with a grenade equipped produces the real formID evidence. Retest needed.

[FIXED, pending in-game confirm] BUG 1 -- Rad Overhaul's on-screen panel(s) "not picked up" by PrismaHUD.
  TWO root causes, both confirmed:
  (a) UNITS BUG: `PRISMA_HUD_API::WidgetDesc` default X/Y/W/H are FRACTIONS of 1920x1080 (the editor does
      `item.x * VP.w`), but the API header's doc comment wrongly said "pixels" and Rad Mod's toxicity
      widget passed raw pixels (30.0f, 882.0f, ...). So `rad_toxic` registered (logs confirm) but its
      editor drag box landed at 30*1920 -> OFF-SCREEN, i.e. registered-but-unusable. Fix: corrected the
      toxic WidgetDesc to fractions, and fixed the misleading `// pixels` comment in PrismaHUD_F4_API.h
      (both the authoritative `PrismaHUD_F4\src` copy and Rad Mod's copy) so the next modder doesn't
      repeat it.
  (b) The FILTER/MASK bar panel was baked into the fullscreen `radhud.html` view, which is intentionally
      NOT registered (PrismaHUD Show/Hide/position operate per-VIEW; registering the fullscreen visor
      would let the editor toggle/move the whole gas-mask overlay -- `HUDLayoutManager::ShowAllWidgets`
      calls `Show(view)` on every registered view). Fix: extracted the FILTER/MASK bars into a NEW
      standalone view `hud_filter.html` (mirrors `hud_toxic.html`), registered as `rad_filter`
      ("Gas Mask Filter"); the fullscreen `#maskOverlay` + `#visorFog` stay in `radhud.html`.
      `RadHud::PushFilter` now drives BOTH views with the same `updateFilter` payload (visor fog on the
      overlay view, bars on the standalone view). Files: `RadHud.cpp/.h`, new `hud_filter.html`, trimmed
      `radhud.html`/`radhud.js` (updateFilter there keeps only the visor-fog logic).

-- Session 2026-07-07 (pt7): PrismaHUD widgets could render BEHIND another mod's Prisma view (Rad Mod's gas-mask visor) --

[OPEN][MED, fix built + deployed, NOT YET RUNTIME-CONFIRMED] User reported the gas-mask visor overlay
  from their Rad Overhaul mod (FalloutAnomalyRadiation /
  Rad Mod) sometimes painted on top of PrismaHUD's health/ammo/filter widgets, obscuring them (screenshot
  showed HUD bars cut off behind the mask's dark vignette shape).
  Root cause (found via Explore agent, cross-checked against PrismaUI_F4 New Gen source): PrismaUI_F4
  composites every independent view's CEF surface into ONE sprite batch, sorted ascending by a per-view
  `order` int (`ViewRenderer.cpp` `DrawViews()`, `std::sort` by `order`, later order paints on top).
  `order` is auto-assigned as `maxExistingOrder + 1` at `CreateView()` time (`ViewManager.cpp`) --
  meaning whichever mod's view happens to be CREATED LATER in a given session simply wins the stacking
  order, regardless of intent. Rad Mod's `RadHud::CreateView()` and PrismaHUD's `CreateBuiltinViews()`
  both fire off the same `kPostLoadGame`/`kNewGame` messages with no explicit ordering, so which one
  ends up on top was pure session-timing luck -- not a bug in either page's own CSS (Rad Mod's internal
  z-index stack for maskOverlay/visorFog/filterWidget within its OWN view was already correct).
  The framework already exposes `IVPrismaUI7::SetOrder(view, order)` for exactly this; PrismaHUD simply
  never called it.
  Fix: `PrismaHUD_F4\src\main.cpp` `MakeView()` (the single choke point every builtin widget + the
  layout editor view goes through) now calls `g_api->SetOrder(view, kHudViewOrder)` right after
  `CreateView()` succeeds, with `kHudViewOrder = 1'000'000` -- comfortably above any other mod's small
  auto-incremented order value, so PrismaHUD's HUD/editor views always paint on top of any other Prisma
  view (Rad Mod's mask overlay or otherwise), regardless of creation timing.
  Built + deployed (`PrismaHUD_F4.dll`). **Not yet runtime-confirmed** by the user.

-- Session 2026-07-07 (pt6): compass backing shade for readability over bright backgrounds --

[OPEN][LOW, UI only, NOT YET RUNTIME-CONFIRMED] User asked for readable compass on bright backgrounds
  (snow, daytime sky, white walls) -- the tape previously had only text-shadow/box-shadow for contrast,
  which washes out badly against light backdrops.
  Added a static `#shade` layer behind the tape/ticks: a flat top-to-bottom dark gradient
  (`rgba(0,0,0,0.32)` -> `rgba(0,0,0,0.46)`) with a `mask-image` linear-gradient edge fade (transparent
  at the far left/right 10%, opaque in the middle) so the widget doesn't look like a hard rectangle.
  Deliberately NOT `backdrop-filter`/blur -- that resamples the framebuffer behind the element every
  frame (real, avoidable GPU cost, especially on a compositor already juggling multiple Prisma CEF
  views). A flat gradient + mask is painted once and cached like any other static layer since neither
  `#root` nor `#shade` ever move/resize after layout -- only `#strip`'s transform animates, and that's
  already on its own composited layer.
  Files: `hud_compass.html` only (`#shade` div + CSS, `#strip`/`#shade` z-index ordering). No C++
  change, no rebuild -- deployed directly to `mods\PrismaHUD\PrismaUI_F4\views\hud_compass.html`.

-- Session 2026-07-07 (pt5): compass turn-lag tuning + quest-blip redesigned as a hexagon/diamond outline --

[OPEN][LOW, UI + timing tuning, NOT YET RUNTIME-CONFIRMED] After pt4's tracked-quest fix, user reported
  the compass still felt laggy when turning, and asked for a bigger yellow hexagon/diamond outline
  marker instead of the small filled dot.
  Timing: the poll/transition pair (JS `setInterval` calling `window.requestUpdate()` + CSS transition
  on the tape's transform) had a worst-case latency of ~210ms between an actual heading change and the
  tape visually reflecting it (80ms poll wait + 130ms transition). Tightened to 50ms poll / 0.08s
  transition (same "transition slightly > poll" ratio, so motion still stays continuous between
  updates) -- cuts worst-case latency to ~130ms. Pure JS/CSS tuning in `hud_compass.html`, no C++
  change, no rebuild needed.
  Marker: replaced the 6x6px filled circle `.blip` with an inline SVG hexagon (vertical/pointy top-
  bottom, ~15x20px, semi-transparent yellow fill + 2.4px yellow stroke outline + drop-shadow glow) --
  reads as a hexagon/diamond hybrid per the request. Same `#questBlip` id/left-positioning contract,
  so the existing JS placement logic (`place()`) needed no changes beyond CSS.
  Deployed the updated view directly (`assets\views\hud_compass.html` ->
  `mods\PrismaHUD\PrismaUI_F4\views\hud_compass.html`) -- no xmake build involved, HTML/CSS/JS only.

-- Session 2026-07-07 (pt4): compass quest blip was following untracked quests, not the tracked one --

[OPEN][MED, fix built + deployed, NOT YET RUNTIME-CONFIRMED] User reported the compass quest blip felt
  "laggy" and didn't track "real" quest data, despite the diagnostic log showing successful ref
  resolution every poll (`resolved=4`). Root cause: `BuildCompassJSON()` filtered objectives only on
  `enstanceState == kDisplayed`, which is true for every ACTIVE quest's current objective -- not just
  the one(s) the player toggled "Active" (tracked) in the Pip-Boy quest log. With 5 displayed objectives
  across several quests, the blip picked whichever untracked side-quest's target happened to be
  geometrically nearest, re-transitioning every time a different one won -- reading as both wrong-quest
  and jittery/laggy.
  Per the user's ask, searched `ReverseEngineering\Symbols\Fallout4_functions.txt` for compass/quest-target symbols, then
  decompiled the real 1.10.163 equivalents. Vanilla's own compass (`HUDCompassBase::
  ProcessCompassMarkerFunctor` -> `CalculateCompassMarkersFunctor::operator()`, RVA `0xa3c150`) gates
  every quest with `if (TESQuest::GetActive(quest))` before considering any of its targets.
  `TESQuest::GetActive` (RVA `0x5d6aa0`) is bit `0x0800` of `QUEST_DATA::flags` -- already a verified
  field in CommonLibF4's `RE::TESQuest` (`data.flags` at `TESQuest+0xF4`), so the fix is a plain struct
  read, no raw-RVA patch.
  Fix: skip any objective whose `ownerQuest->data.flags & 0x0800 == 0` in `BuildCompassJSON()`. Added a
  `tracked=N` counter to the diagnostic log. Built clean, redeployed to the live modlist.
  Full decompile trail: `ReverseEngineering\03-FINDINGS.md` (2026-07-07 pt4).
  [OPEN] Not yet confirmed in-game by the user -- do not mark `[FIXED]` until they verify (same protocol
  as the ammo/compass suppression fix below).

-- Session 2026-07-07 (pt3): CRITICAL -- pt2's shipped VTable patch had RVAs off by 0x2000000 (32MB) --

[CRITICAL][FIXED SAME SESSION, caught before any save exercised it] The three RVAs deployed in
  `InstallVanillaHUDSuppression()` (pt2 below) were transcribed by hand from `VTableDump.java`'s
  absolute-address output minus image base, and the leading `2` was dropped: `0xd13fb8` instead of
  `0x2d13fb8` (and similarly for the other two). All three wrong addresses landed inside
  `BGSCreatedObjectManager::SaveGame`/`LoadGame` (serializes/deserializes player-crafted magic
  items/enchantments/potions) — NOT any HUD vtable. Since the patch is an 8-byte function-pointer
  write (`write_vfunc`) at a hardcoded offset, this was corrupting live save/load-game machine code,
  applied every session at kGameDataReady. Would very likely have crashed or corrupted saves on the
  next save or load with this DLL active. Caught by independently re-decompiling what was actually at
  the deployed addresses (not by any in-game symptom) BEFORE a save was loaded with the broken build.
  Fixed: corrected RVAs to `0x2d13fb8`/`0x2d14038`/`0x2d14528`. Rebuilt + redeployed immediately,
  overwriting the broken DLL in the live modlist. Full postmortem + the general lesson (hand-computed
  hex-subtraction RVAs need re-verification against the deployed value, not just re-derivation) in
  `ReverseEngineering\03-FINDINGS.md` ("CRITICAL: the pt2 VTable patch shipped with RVAs short by
  0x2000000").
  [OPEN][LOW] Still not yet in-game verified with the corrected build (this fix landed before any
  test pass of pt2's version happened, so there's no separate "did suppression itself work" result
  yet — same pending test as before, now on a safe build).

-- Session 2026-07-07 (pt2): SetVariable hide didn't stick -- real fix is a CanBeVisible VTable patch --

[SUPERSEDES pt1 below] In-game screenshot confirmed the SetVariable("<path>._visible", false) approach
  (pt1, same day) did NOT work — vanilla ammo/AP counters still rendered. Root cause: each
  HUDComponentBase-derived widget's own `UpdateDisplayObject()` re-derives `_visible` from its virtual
  `CanBeVisible()` every engine tick, far faster than PrismaHUD's 100-500ms poll — our SetVariable write
  only ever stuck for a fraction of a frame before the engine overwrote it again. SetVariable was never
  going to work for this class of widget; it needed a native hook.
  User found the real mechanism by writing a new Ghidra tool, `VTableDump.java` (dumps a named vtable's
  raw function-pointer slots — `FW_Query.java` only searches Function symbols, so a plain name search
  never turns up `CanBeVisible` for `HUDAmmoCounter`, which doesn't override it and thus has no
  distinctly-named entry). Confirmed: `HUDComponentBase::CanBeVisible` is a REAL virtual at slot **[7]**
  in every `HUDComponentBase`-derived vtable (RVA `0xa22db0`, shared by `HUDAmmoCounter` and
  `HUDExplosiveAmmoCounter` since neither overrides it; `HUDCompass` has its own override at `0xa1cad0`
  but the same slot index).
  [FIXED] Replaced the SetVariable code with a one-time native VTable patch, `InstallVanillaHUDSuppression()`
  (`HUDDataProvider.cpp`, called once from `main.cpp` kGameDataReady): `REL::Relocation<uintptr_t>{
  REL::Offset{vtableRVA} }.write_vfunc(7, &AlwaysHidden)` against the three target vtables'
  RVAs (`VTABLE_HUDAmmoCounter` 0xd13fb8, `VTABLE_HUDExplosiveAmmoCounter` 0xd14038, `VTABLE_HUDCompass`
  0xd14528 — all 1.10.163 OG-only, gated behind `REX::FModule::IsRuntimeOG()`). This makes the hide
  permanent and one-shot: no per-poll re-assertion, no GFx race, survives every future `UpdateDisplayObject`
  tick because the widget's own visibility check now always returns false. Built + deployed.
  Full decompiled `CanBeVisible` logic + all vtable RVAs: `ReverseEngineering\03-FINDINGS.md`
  ("HUDComponentBase::CanBeVisible — real vtable slot found").
  [OPEN][LOW] Not yet in-game verified (build+deploy only, pending the user's next test pass — same
  session as the compass jitter/quest-blip retest already queued).

-- Session 2026-07-07 (pt1): vanilla compass suppression via Ghidra-recovered GFx path table [SUPERSEDED, see pt2] --

[SUPERSEDED — see pt2 above] Vanilla HUDMenu compass hidden alongside the ammo counters via
  Decompiling `HUDMenu::HUDMenu` (1.10.163 OG, RVA 0x1278970) with the Ghidra pipeline dumped the literal GFx
  member path for EVERY built-in HUD widget straight out of its `AddHUDObject<T>(this, "path")`
  calls — no runtime tree probe needed (the compass path `BottomCenterGroup_mc.CompassWidget_mc`
  cross-validates exactly against the ammo path found by live-probing last session). Full table +
  method in `ReverseEngineering\03-FINDINGS.md` ("Full HUDMenu.swf widget path table") — the path
  table itself is still correct and useful, but the SetVariable-based suppression built on top of it
  did not survive the engine's own per-tick visibility re-derivation; see pt2 for the real fix.
  This closes the old `[OPEN]` note below about needing a "runtime GFx-tree probe" for the compass —
  it was never actually required. Any future PrismaHUD widget duplicating a vanilla one (health, AP,
  rads) can reuse the GFx path table (for other purposes, e.g. moving/recoloring) with zero further RE.

--------------------------------------------------------------------------------
SECTION 4 — PrismaHUD_F4   (a.k.a. "PrismaHud" — the standalone HUD release)
Source folder: E:\F4SE OG\Prisma\UI_Systems\PrismaHUD_F4\   (MOVED from Prisma\PrismaHUD_F4; that
               old path is now a stale empty leftover. The git-TRACKED copy is under UI_Systems\.)
Build:         xmake (from the source folder, run `xmake`). CONVERTED from cmake 2026-06-08 because the
               workspace cmake CommonLibF4 became xmake-only. Output: build\windows\x64\release\PrismaHUD_F4.dll
               cmake/ + CMakeLists.txt are retained but will NOT fresh-configure (no cmake CommonLibF4).
MO2 deploy:    D:\Games\ModlistDownloads\mods\PrismaHUD\   (NEW standalone mod, NOT auto-enabled in profile)
  └── F4SE\Plugins\PrismaHUD_F4.dll
  └── PrismaUI_F4\views\hud_*.html
  └── PrismaUI_F4\icons\*.svg
CommonLib:     E:\F4SE OG\CommonLibF4  (xmake; see COMMONLIB_FINDINGS Section 0a)
Session date:  2026-06-01 (orig); 2026-06-08 (tactical look + compass quest + xmake conversion)
Key source files:
  src/main.cpp             — F4SE entry, view lifecycle, editor open/close
  src/HUDLayoutManager.cpp — widget registry, layout.json load/save, editor payload
  src/HUDDataProvider.cpp  — game data readers (BindUIEvent, game thread)
  src/HUDLayoutManager.h   — WidgetEntry, IVPrismaHUD1 impl
  src/KeyHandler.cpp/h     — BSInputEventUser sink, Shift+F11 toggle
  src/PrismaHUD_F4_API.h   — public API for other plugins (RegisterWidget)
Layout save:   Documents\My Games\Fallout4\PrismaHUD\layout.json
F4SE log:      Documents\My Games\Fallout4\F4SE\PrismaHUD_F4.log
--------------------------------------------------------------------------------

-- Session 2026-06-30 (pt4): in-game functional pass — ammo, survival, F11, container suppress -

[FIXED] Ammo widget showed "-1" for melee. equippingAmmoCount is -1 for melee/unarmed and is
  the ONLY loaded-ammo source CommonLibF4 exposes (no clean magazine-count API). Now: hide the
  widget unless GetCurrentAmmo(BGSEquipIndex{0}) returns a form (ammo-using weapon), and clamp
  loaded to >=0. HUDDataProvider BuildAmmoJSON. [OPEN] true loaded count still approximate.

[FIXED] Survival widget only showed when a need value was >0. Now shows whenever
  player->GetDifficultyLevel() == kSurvival(0x5) || kTrueSurvival(0x6) (user's gamedifficulty=6).
  RE/D/DifficultyLevel.h; PlayerCharacter::GetDifficultyLevel().

[FIXED] Shift+F11 also triggered the chat mod (bound to plain F11). Now LEFT-Shift+F11 only, and
  KeyHandler CONSUMES the key (callbacks return bool -> set event->handled = kStop). Switched
  KeyHandler registration from handlers.push_back to the new MenuControls::RegisterHandler engine
  fn (the MenuControls feat is in our up-to-date CommonLibF4). MenuControls DOES honour handled
  (PlayerControls does NOT), so consuming here reliably blocks the other F11 listener.

[FIXED][CRITICAL INSIGHT] QuickLoot input hijack never suppressed the vanilla ContainerMenu
  because event->handled = HANDLED_RESULT::kStop in a PlayerControls handler is IGNORED by the
  vanilla ActivateHandler (confirmed via research: BSInputEventReceiver does not propagate handled
  to vanilla gameplay handlers). REWORKED: suppress via a BSTEventSink<MenuOpenCloseEvent> on
  RE::UI that posts UIMessageQueue::AddMessage(ContainerMenu, kHide) when opening AND we have a
  live quick-loot target AND Shift isn't held (Shift+Activate = full vanilla menu). Take/TakeAll
  now use the CACHED container handle (m_container), not a live ResolveTarget, so the transiently-
  open-then-hidden menu can't null the take. Input handler now only OBSERVES Activate/wheel (no
  front-rotation, no reliance on kStop). [OPEN] needs in-game test: does kHide cleanly suppress,
  any flicker, does take fire. Log lines: "ContainerMenu suppression sink registered",
  "take/scroll input handler installed".
  GOTCHA for repeat-issues: PlayerControls handlers CANNOT consume/block vanilla gameplay actions
  via handled=kStop. Use MenuOpenCloseEvent+kHide (menus) or a vtable/function hook instead.

[FIXED] Ammo count was -1/garbage. Now reads the TRUE loaded magazine count + ammo form from
  currentProcess->middleHigh->equippedItems[].data (static_cast to EquippedWeaponData) ->
  wd->ammo (TESAmmo*) + wd->ammoCount (uint32 @0x18). RE/E/EquippedWeaponData.h. Melee/no-ammo
  (wd->ammo null) hides the widget. Far better than equippingAmmoCount.

[CORRECTION] Earlier note "no Scaleform in OG CommonLibF4" was WRONG (first research agent erred).
  OG CommonLibF4 HAS Scaleform: Scaleform/G/GFx_Value.h (Value(bool), SetMember/GetMember/Invoke),
  Scaleform/G/GFx_Movie.h (GetVariable/SetVariable). HUDMenu reachable via
  RE::UI::GetSingleton()->GetMenu<RE::HUDMenu>(); IMenu::uiMovie (Scaleform::Ptr<GFx::Movie> @0x40).
  So hiding a vanilla HUD element IS doable in-code:
    menu->uiMovie->SetVariable("<path>._visible", Scaleform::GFx::Value(false));
  [OPEN] The exact GFx member PATH depends on the loaded HUDMenu.swf — this modlist uses FallUI HUD
  (replaces HUDMenu.swf), so the vanilla path (e.g. root.RightMeters_mc.AmmoCount_mc) may differ.
  Cleanest user route: FallUI HUD MCM toggles the vanilla ammo off. In-code route needs a runtime
  GFx-tree probe to discover the real path (IDA exe-dumping does NOT reveal SWF paths).
[OPEN] Editor: scale doesn't scale content (resize only stretches the box) + add per-widget 90°
  rotate (flip) button. User wants uniform aspect-locked scale + rotate-a-single-widget-90.
  Plan: views apply transform:scale(w/designW) at design size; editor aspect-locks resize; flip
  needs a rot field plumbed through WidgetEntry/payload. Not yet implemented.
[NOTE] All 9 editor align buttons ARE wired (verified) — earlier "no function" was the corrupt
  zero-size/off-screen widgets (fixed in pt3).

-- Session 2026-06-30 (pt6): grenade widget showed the PISTOL — WEAPON_TYPE magic-number bug -

[FIXED] In-game (screenshot): the grenade widget displayed "1 KIMBER CUSTOM" (the equipped pistol) and
  the ammo widget "8/101 KIMBER CUSTOM". Diagnostic log confirmed equippedItems=1 (just the pistol),
  weapon=yes, ammoForm=yes, loaded=8. Root cause: BuildGrenadeJSON filtered with magic numbers
  `if (wtype != 8 && wtype != 9) continue;` commented "8=grenade, 9=mine" — but the real CommonLibF4
  WEAPON_TYPE enum is kGun=0x9, kGrenade=0xA, kMine=0xB. So a GUN (type 9) passed the grenade filter and
  the widget showed the pistol + its inventory count (1). This is the "reading the pistol as a grenade"
  AND "ammo that doesn't exist" report. Fixed: use the typed helper `weap->IsThrownWeapon()` (kGrenade/
  kMine). Also guarded BuildAmmoJSON with `weapon->IsGunWeapon()` so only firearms show the ammo widget.
  Built+deployed. NB: equipped grenades do NOT appear in equippedItems (only the held weapon does), so the
  grenade widget now correctly HIDES for a gun; making it show an actual equipped throwable needs a
  different source (player equipped-throwable slot) — revisit if grenades are used.
  [FIXED, later session] vanilla ammo SWF still shows: FallUI-HUD is DISABLED (modlist "-FallUI - HUD"), so
  it's the real vanilla HUDMenu.swf. Fixed via a live GFx tree probe (2026-06-30 later pass) ->
  `HideVanillaAmmo()`; the path was independently CONFIRMED + the full widget table recovered without any
  probing by decompiling `HUDMenu::HUDMenu` with Ghidra (2026-07-07, see top of this file +
  ReverseEngineering\03-FINDINGS.md). uiMovie->SetVisible(false) hides the WHOLE HUDMenu (too much) —
  SetVariable("<path>._visible", false) on the specific `_mc` path is the correct scoped approach.

-- Session 2026-06-30 (pt5): ammo widget — weapon name + show-when-equipped + diagnostics -

[FIXED] Ammo widget never showed the WEAPON name, and hid entirely for a holstered/no-ammo weapon.
  Root causes in BuildAmmoJSON: (1) it emitted the AMMO form's name as "name" (so the weapon was
  never surfaced); (2) hasWeapon/widget visibility was gated on a non-null ammo form, so a holstered
  gun (live EquippedWeaponData not yet instantiated -> data null) hid the whole widget. Rewrote to:
  capture the equipped weapon FORM (form->As<TESObjectWEAP>()) and always output its name + hasWeapon
  whenever a weapon is equipped; fill loaded (EquippedWeaponData::ammoCount @0x18) + ammoForm + reserve
  only when the live data is present. Added "ammoName" as a separate field. JS hud_ammo.html label now
  shows the weapon name (d.name). Built+deployed.
  [DIAGNOSTIC] Added a throttled log (~every 10s): "PrismaHUD ammo: eqItems=N weapon=y/n ammoForm=y/n
  loaded=N dataNull=b" so the in-game log reveals WHY loaded reads 0 (empty array vs weapon-found-but-
  data-null vs drawn-and-populated). Need the user's log line to finish the loaded-count fix if still 0.
  [VERDICT on Papyrus] FO4 Papyrus has GetEquippedWeapon (weapon + name) and reserve via GetItemCount,
  but NO loaded/chambered-ammo function exists anywhere in the API (verified vs CK Wiki). So the loaded
  count MUST stay in the DLL (EquippedWeaponData::ammoCount); Papyrus cannot provide it. Vanilla HUD gets
  it via the engine PlayerAmmoCountEvent -> HUDAmmoCounter (RVA 0xa155a0). AIProcess::GetCurrentAmmo @
  RVA 0xe70530 (REL-only fn; the named Ghidra DB couldn't cleanly decompile it -- auto-analysis never
  disassembled it into a proper function. Header offsets used instead, which are authoritative.)
  [REVIEW HARDENING] code-reviewer flagged the static_cast<EquippedWeaponData*>(eq.data.get()) as an
  unchecked downcast (EquippedItem::data is the polymorphic base EquippedItemData; ammo@0x10/ammoCount@0x18
  lie past the 0x10-byte base, so a non-weapon data subtype on a WEAP form would read garbage, not crash).
  Fixed: guard on runtime vtable match before casting --
  `base && reinterpret_cast<const std::uintptr_t*>(base)[0] == RE::EquippedWeaponData::VTABLE[0].address()`
  (idiom from CommonLibF4 REX/CAST.h). Built+deployed. [MINOR/DEFERRED] reviewer also noted equippedItems
  is iterated without MiddleHighProcessData::equippedItemsLock (BSSpinLock) -- pre-existing pattern (grenade
  + old quick-loot do the same), game-thread-serialized so low risk; left as-is, optional BSAutoLock later.

-- Session 2026-06-30 (pt4): Ghidra-OG verified the compass quest-blip alias bug -

[FIXED] Compass quest blip pointed at the wrong target (or none). BuildCompassJSON resolves each
  active objective's target alias via TESQuest::GetAliasedRef(handle, aliasID), reading the alias
  out of the forward-declared RE::TESQuestTarget through a local QuestTargetLite struct. That struct
  ASSUMED { int32 alias @ 0x00; uint32 flags @ 0x04 } ("well-known FO4 layout") — WRONG. Verified
  against the named 1.10.163 binary (Ghidra) + CommonLibSSE (byte-identical) + FO4 TESCondition==0x8:
  real layout is flags@0x00(uint8), conditions@0x08(TESCondition), alias@0x10, sizeof 0x60. The old
  code read the flags byte (0/1) as the alias -> GetAliasedRef(0/1) -> wrong/missing blip. Fixed
  QuestTargetLite to put alias@0x10 with static_assert(offsetof(alias)==0x10). Built+deployed.
  TOOLING NOTE: this is the first fix produced by the restored Ghidra-OG pipeline — see
  reference_ghidra_og_setup memory (bundled repo Ghidra was jar-stripped + project was never named;
  real install at E:\F4SE OG\ReverseEngineering\Ghidra\ghidra_12.1_PUBLIC, named copy at E:\F4SE OG\ReverseEngineering\Projects\fwgh_proj_OLD_packed_reference_only, pyghidra in Py3.14).
  PATH NOTE: workspace was reorganized this session — Prisma is now under E:\F4SE OG\Projects\Prisma\
  (was E:\F4SE OG\Prisma\), and Knowledge Materials moved under docs\. PrismaHUD xmake.lua's
  includes("../../../CommonLibF4") is now off by one level; build with COMMONLIBF4_PATH=E:\F4SE OG\
  CommonLibF4 (env override the lua already supports), and delete build\ once to clear baked old PCH
  path. Live MO2 deploy is E:\Modlists\Fallen World Alpha 2\mods\PrismaHUD\ (NOT the D:\ path above).

-- Session 2026-06-30 (pt3): in-game bug pass — layout corruption, compass stutter, icons -

User ran it in-game: health/survival misaligned & squished, compass stuttered, editor align
buttons "did nothing", icons ugly. Root causes + fixes (verified by an independent code-review
subagent):

[FIXED] THE root cause of the misalignment — the per-view 50ms `setTimeout(applyPos(DEF),50)`
  timer fired UNCONDITIONALLY and clobbered whatever position C++ had just pushed
  (LoadAndApply/ApplyLayout), so saved/correct positions were overwritten by the hardcoded
  default depending on InteropCall-vs-timer ordering. Fix: `var _posSet` flag — setHUDPosition
  sets it; the timer now `if(!_posSet)applyPos(DEF)`. Added to ALL 9 active views. C++ also now
  calls ApplyLayout(v) inside each view's DOM-ready (DOM_READY macro + OnQuickLootDomReady) so a
  not-yet-ready view doesn't miss its position push. This makes C++ authoritative; no more race.

[FIXED] layout.json was already corrupt (rads/survival/ammo/grenade w:0,h:0; health/ap y>1).
  `MigrateToFractions` treated a corrupt fraction >1.0 as legacy PIXELS and divided by 1080 ->
  teleported health to y~0.001 (the editor showed the health box at the top because of this).
  Fix: migrate only clear pixel values (> 2.0). Added `IsSanePos()` (reject w/h<=0.01, w/h>1.5,
  x/y outside [-0.1, 1.001]) at load/report/save so corruption can't be loaded, reported by a
  hidden 0-viewport view, or persisted. Added a 0-viewport guard to every view's applyPos.
  Backed up the user's corrupt layout.json -> .corrupt.bak (clean defaults).

[FIXED] compass stutter — it repainted only every 250ms with no smoothing, so the tape jumped
  in steps. Rewrote hud_compass.html to ease the DISPLAY heading toward a TARGET every
  requestAnimationFrame (shortest signed 360 wrap), poll 250->100ms. C++ BuildCompassJSON now
  throttles the heavy cell-name + quest scan to every 5th call (heading stays fresh each call),
  so the faster poll stays cheap. (The angle source player->data.angle.z was fine; the stutter
  was purely update-rate + no interpolation.)

[FIXED] icons — copied Rad Mod PNGs (heart-rate/radioactive/running-man/ammo/hand-grenade,
  all RGBA-alpha) into assets\icons\ + deployed to PrismaUI_F4\views\icons\. Wired health ->
  heart-rate, rads -> radioactive, ap -> running-man as -webkit-mask-image (set via JS so the
  severity colour tint via background-color still works). ammo/grenade icons copied, not yet wired.

[CONFIRMED OK] editor align/distribute JS, the moveWidget->LiveMove->ApplyToView path, the
  compass rAF gating, and the QuickLoot RemoveItem transfer are all correct — the align buttons
  only "did nothing" because they were moving zero-size/off-screen (corrupt) widgets.

[OPEN][LOW] QuickLoot: if the crosshair target flickers (e.g. corpse ragdoll IsDeleted for a
  frame) mid-hold, IsActive() goes false for that frame and the Activate isn't consumed -> the
  vanilla menu could open for one frame. Mitigation (cache target at hold-start) deferred.
[OPEN][LOW] QuickLootInput::Install front-rotation of PlayerControls::handlers is correct but
  fragile (depends on push_back-then-shift order); prefer BSTArray insert(begin,...) if revisited.

-- Session 2026-06-30 (pt2): interactive quick-loot (hijack vanilla compact ContainerMenu) -

Goal: suppress the vanilla green compact ContainerMenu quick-loot popup (FallUI re-skins
ContainerMenu.swf) and drive looting through our hud_quickloot.html instead.

Reverse-engineered the reference mod "Jack's Disable Quick Loot.esp" (E:\F4SE OG\Prisma\
Reference_Tools\Mod) with FO4RecordEditor MCP — it is PURE DATA: a StartGameEnabled/RunOnce
QUST whose PlayerRef alias (UniqueActor=Player) carries an ability SPEL in its alias Spells
list (auto-applies, no script) -> MGEF -> hidden PERK with 3 Activate entry points
(ReplaceDefault, ButtonLabel "Search", EMPTY fragments) gated to unlocked containers (excl.
Workbench_General), dead actors (IsActor+GetDead), and ash piles (IsAshpile). ReplaceDefault
+ "Search" makes the engine show a normal Search activate instead of the quick popup.

We did NOT use that ESP. Per user choice the suppression + looting are pure C++ in
PrismaHUD_F4 (new files src/QuickLootController.cpp/.h):
  * QuickLootInput : RE::PlayerInputHandler, FRONT-inserted into PlayerControls::handlers
    (rotate to index 0 so it sees Activate before the vanilla ActivateHandler, which early-
    outs once event->handled is set). Consumes via ev->handled = HANDLED_RESULT::kStop.
      - Activate tap  -> TakeSelected   - Activate hold (>=0.35s) -> TakeAll
      - Shift+Activate-> NOT consumed (vanilla full ContainerMenu still opens = "Transfer")
      - mouse wheel (kWheelUp 0x800 / kWheelDown 0x900) -> MoveSelection
    Cheap pre-filter: only act on QUserEvent()=="Activate" or the wheel codes; live IsActive()
    (GetPickRef + !ContainerMenu open) gates the rest. Consuming Activate is what suppresses
    the vanilla compact menu (it never opens).
  * Transfer API (confirmed, used by PrismaCrafting too): container->RemoveItem(RemoveItemData)
    with reason = ITEM_REMOVE_REASON::kStoreContainer (0x4) and otherContainer = player.
  * Target = RE::Console::GetPickRef(); container = formType kCONT, corpse = kNPC_ && IsDead.
  * QuickLootController owns the ORDERED item snapshot so JS list indices == C++ indices; it
    builds the updateLoot JSON (adds a "sel" field) and InteropCalls on every change. JS
    hud_quickloot.html now honours d.sel as the highlight source of truth.
  * Wiring: SetAPI at kGameDataReady; OnQuickLootDomReady sets the view + binds requestUpdate
    -> controller; QuickLootInput::Install() at kPostLoadGame. Old BuildQuickLootJSON /
    BindQuickLootUpdate removed (wrapped #if 0).

Builds clean (xmake). [OPEN][MED] NOT yet in-game tested — two things to verify on a live run:
  1. Does front-inserting our handler + handled=kStop actually block the vanilla ActivateHandler
     (does it honour event->handled)? If not, fallback = vtable-hook ActivateHandler, or close
     ContainerMenu via MenuOpenCloseEvent sink.
  2. Item transfer correctness (RemoveItem kStoreContainer -> player) for stacks/legendaries.
[OPEN][LOW] "Hold = take all" replaces the option-described "hold = open full menu"; full menu
  is on Shift+Activate instead (cleaner, needs no programmatic ContainerMenu open).
[DEPLOYED 2026-06-30] Created mods\PrismaHUD (was a disabled, empty MO2 entry) and enabled it:
  F4SE\Plugins\PrismaHUD_F4.dll + PrismaUI_F4\views\hud_*.html + PrismaUI_F4\icons\. Flipped
  `-PrismaHUD` -> `+PrismaHUD` (line 2, top priority) in profiles\Fallen World\modlist.txt.
  Mirrors PrismaCrafting's mod layout (root = Data; no Data\ prefix). In-game test pending.

-- Session 2026-06-30: newest-PrismaUI sync + bug sweep (graphical + concurrency) -

[FIXED] PrismaUI API header bumped V4 -> V5 (newest). Synced src/PrismaUI_F4_API.h from
  Prisma\Framework\PrismaUI_F4 New Gen\src\ (10791 bytes, adds IVPrismaUI5 on-mesh API).
  Backward-compatible: HUD still requests IVPrismaUI4. Builds clean.

[FIXED] HUDLayoutManager::SaveLayoutFile() data race -> potential crash. It read
  m_savedPos/m_userVisible with NO lock while invoked from the detached thread in
  SetWidgetVisible, racing the game thread. Added `std::shared_lock lock(m_lock);` to
  match SaveSlotsFile (m_lock is mutable). Repeat-pattern: detached-thread file save
  MUST lock the shared maps it reads.

[FIXED] Editor drag boxes offset 60px below the real HUD (WYSIWYG broken). #overlay was
  position:fixed top:calc(toolbar+align)=60px but widget coords are full-viewport fractions,
  so every drag box rendered 60px low vs the live widget behind it. Set #overlay top:0
  (canvasTop then computes 0, so the existing Math.max(-canvasTop,…) clamp self-corrects).
  Top-strip widgets (compass) now sit behind the bars — edit them via the sidebar list +
  numeric inputs. hud_editor.html.

[FIXED] Compass cardinal labels ~27px off the centre notch. applyHeading centred on the
  ROOT width (600) instead of the strip-wrap content width (root − 24px padding) AND aligned
  tick LEFT edges instead of label centres. Rewrote to use stripWrap.offsetWidth/2 as centre
  and (h+180)*pxPerDeg + TICK_W/2 as the label centre; quest blip uses the same basis.
  hud_compass.html.

[FIXED] Runtime auto-hide leaked into the persisted user-visibility preference. Editor
  eye-toggle and per-frame JS reportVisibility both called SetWidgetVisible (persisted),
  so an auto-hidden widget (e.g. ammo w/ no weapon) looked user-disabled in the editor,
  the user couldn't permanently hide an auto-hide widget, and gameplay spammed layout.json
  writes. Split into m_userVisible (persisted, editor) + m_runtimeVisible (transient, JS);
  added SetRuntimeVisible + ApplyEffectiveVisibility (show iff user AND runtime allow).
  reportVisibility now -> SetRuntimeVisible. RestoreWidgetVisibility re-applies effective
  visibility instead of a pre-editor snapshot (so an in-editor eye-toggle sticks).

[FIXED] Survival widget default height 48 -> 58 /1080 in main.cpp to match the real rendered
  HTML height (3 rows + padding); editor box was shorter than the widget.

[NOTE] Emoji render fine in Ultralight (confirmed across PrismaWorkshop/quickloot) — quickloot's
  🔫🛡💀 icons are NOT a rendering risk; left as-is.
[NOTE] hud_container.html is orphaned (main.cpp loads hud_quickloot.html instead) — harmless dead file.
[NOTE] MO2 deploy path is now E:\Modlists\Fallen World Alpha 2\mods\PrismaHUD\ (D: drive broken);
  that mod folder does not exist yet — PrismaHUD is not installed in the live modlist.

--------------------------------------------------------------------------------

-- Session 2026-06-08: tactical look + compass quest + xmake conversion ---------

[FIXED] Build broken after workspace reorg — converted PrismaHUD_F4 cmake -> xmake.
  - Plugin was moved to UI_Systems\ (its commonlibf4.cmake path was one level too shallow) AND the
    workspace cmake CommonLibF4 was replaced by an xmake-only one, so fresh cmake configure failed.
  - Added xmake.lua (mirrors Rad Mod). Adapted logging: this CommonLibF4 fork has no F4SE::log/stl, so
    PCH `namespace logger = spdlog;` and main.cpp resolves the log dir via SHGetKnownFolderPath.
  - Builds clean (xmake, ~13s incremental). See COMMONLIB_FINDINGS Section 0a.

[FIXED] Tactical look ported into the vitals widgets (hud_health/hud_ap/hud_rads).
  - Sloped parallelogram bars (clip-path), severity-tinted mask icons (heart-rate/running-man/
    radioactive, inlined as sub-1KB base64 data URIs = zero icon-path/deploy risk), centered value
    that only shows at <=75% of "good". Kept each widget's existing JS contract (updateStats/
    requestUpdate/setHUDPosition/reportPosition/reportVisibility) so the editor + data feed still work.

[FIXED] Compass quest direction (questDir) implemented in HUDDataProvider::BuildCompassJSON.
  - Was a hardcoded -1 stub. Now iterates player->objectives (kDisplayed), resolves each target via
    ownerQuest->GetAliasedRef(alias), picks nearest, outputs absolute bearing atan2(dx,dy) the JS expects.
  - Heading + location already worked. See COMMONLIB_FINDINGS quest-marker entry for the type recipe.
  [OPEN][LOW] questDir relies on a locally-defined TESQuestTarget{int32 alias@0} — UNVERIFIED in-game.
    Low crash risk (offset-0 read of a valid pointer; bad value = no blip). Validate the blip points
    at the active quest target in-game; if wrong, the alias offset is the thing to check.

[OPEN][MED] Tissue widget + ghoul still pending. Rad overhaul should register a tissue widget via
  PRISMA_HUD_API::RegisterWidget (tissue data/code STAYS in the rad overhaul); ghoul overlay stays
  entirely in the rad overhaul. Not yet started.

-- C++ (src/) ------------------------------------------------------------------

[FIXED] DATA RACE -- HUDLayoutManager LiveMove used shared_lock while mutating
  File: src/HUDLayoutManager.cpp (LiveMove)
  Problem: std::shared_lock taken, then entry.x/y/w/h written through the lock.
           Any concurrent reader on another thread sees torn writes. Could cause
           widget positions to corrupt silently during editor drag operations.
  Fix: Changed to std::unique_lock.

[FIXED] DATA RACE -- HUDLayoutManager LoadAndApply used shared_lock while mutating
  File: src/HUDLayoutManager.cpp (LoadAndApply)
  Problem: Same pattern -- shared_lock taken, then entry.x/y/w/h modified when
           applying saved positions from the layout JSON file on kPostLoadGame.
  Fix: Changed to std::unique_lock.

[FIXED] STUB -- BuildSurvivalJSON hunger/thirst now resolved via EditorID lookup
  File: src/HUDDataProvider.cpp (BuildSurvivalJSON)
  Fix: Lazy-init on first call using TESForm::GetFormByEditorID<RE::ActorValueInfo>
       with EditorIDs "Hunger" and "Dehydration". Cached as static file-level pointers.
       Falls back to 0 on non-survival games (lookup returns nullptr gracefully).

[FIXED] STUB -- BuildPromptJSON now reads Console::GetPickRef + GetActivateText
  File: src/HUDDataProvider.cpp (BuildPromptJSON)
  Fix: RE::Console::GetPickRef() (REL::ID 170742) returns the current crosshair
       ObjectRefHandle. GetActivateText(ref, result) called on the base TESBoundObject
       (vtable 0x60). Widget now shows activation text for any interactive object.

[FIXED] PERF -- GPU contention causing second-monitor video lag (2026-06-02)
  Files: src/HUDDataProvider.cpp, assets/views/hud_*.html (9 files)
  Problem: 9 separate Ultralight D3D11 render targets firing ~66 JS→C++→JS
           round trips/sec. Each InteropCall marks the view dirty and triggers
           a full GPU texture re-render even when data was unchanged. The 9
           concurrent D3D11 passes starved DWM/hardware video decode on the
           second monitor even in borderless windowed mode.
  Fix 1 — dirty-check in BIND macro (HUDDataProvider.cpp):
           Each BindUIEvent callback now stores the last-sent JSON in a static
           per-lambda string. If the value is identical, InteropCall is skipped
           entirely — no Ultralight dirty-mark, no GPU re-render.
  Fix 2 — throttled setInterval values in all 9 HTML views:
           interaction: 80→100ms  |  health: 100→250ms  |  ap: 100→200ms
           compass: 100→250ms     |  ammo: 100→200ms     |  rads: 150→500ms
           grenade: 200→500ms     |  container: 500→1500ms | survival: 500→2000ms
  Fix 3 — BuildContainerJSON: replaced std::sqrt with squared-distance comparison.
  Result: ~66 C++ callbacks/sec → ~17. GPU re-renders drop to near-zero at rest.

[OPEN][LOW] VERIFY -- Grenade anim type enum values 8 and 9
  File: src/HUDDataProvider.cpp (BuildGrenadeJSON)
  Problem: Code filters equipped weapons by WeaponAnimationType 8 (grenade throw)
           and 9 (mine). Values assumed from FO4 engine data, not verified in-game.
           If wrong, grenade counter will always show 0.
  Action: Equip a grenade in-game, read log. Add temporary logger::info for
          wtype in the equipped items loop to confirm.
  Priority: LOW -- verify during next in-game session.

[FIXED] BUILD WARNING -- [[nodiscard]] return of KeyHandler::Register discarded
  File: src/main.cpp (kGameDataReady handler)
  Problem: KeyHandler::Register is marked [[nodiscard]] but the return value
           (KeyHandlerEvent handle) was discarded. MSVC C4834 warning on every build.
           The handle is not needed here (registration is permanent for the session)
           but the warning clutters build output and could mask real warnings.
  Fix: Stored as [[maybe_unused]] auto _editorKey = KeyHandler::GetSingleton()->Register(...)


-- JS/HTML (assets/views/) -----------------------------------------------------

[FIXED] SYNTAX ERROR -- stray )); after applyPos closing brace killed entire script
  Files: hud_ap.html, hud_compass.html, hud_container.html, hud_interaction.html,
         hud_ammo.html, hud_grenade.html, hud_rads.html, hud_survival.html
  Problem: applyPos function in compressed form ended with }));}  instead of }).
           JS engine parses each <script> block atomically -- one syntax error
           anywhere and NOTHING in the block runs. window.requestUpdate never set
           up, no setInterval, no data polling, no setHUDPosition -- views appear
           stuck at CSS defaults.
  Fix: Removed the stray )); in each file.

[FIXED] MISSING ID FIELD -- hud_health.html reportPosition JSON had no id key
  File: assets/views/hud_health.html
  Problem: window.reportPosition called with {x,y,w,h} — no "id" field.
           C++ OnPositionReport does j.at("id") which throws json::out_of_range.
           Caught by catch(...){} silently. UpdatePositionFromReport never called.
           Layout manager kept MAKE-default pixel coords instead of viewport-
           fraction-computed positions. Editor boxes showed wrong locations.
  Fix: Added var WIDGET_ID = 'prisma_health' and id:WIDGET_ID to the JSON.

[FIXED] TIMING -- applyPos fired before BindUIEvent registered window.reportPosition
  Files: ALL 9 HUD HTML views
  Problem: Script body called applyPos(...) immediately, then set the fallback:
             window.reportPosition = window.reportPosition || function(){};
           At that point BindUIEvent hadn't fired yet (DOM ready fires after the
           script body completes). window.reportPosition was undefined → TypeError
           silently swallowed. C++ never received the initial position report.
           Editor showed stale MAKE-default coords instead of actual positions.
  Fix: Moved fallback assignments BEFORE any applyPos call. Wrapped initial
       applyPos in setTimeout(fn, 50) to give C++ BindUIEvent time to register
       after the DOM ready callback fires.

[FIXED] MISSING FALLBACK -- window.reportPosition fallback absent in many views
  Files: hud_ap, hud_compass, hud_container, hud_interaction, hud_ammo,
         hud_grenade, hud_rads, hud_survival
  Problem: window.requestUpdate had a || fallback but window.reportPosition did
           not. After the syntax error was fixed (above), calling reportPosition
           before BindUIEvent registered it would still throw TypeError.
  Fix: Added window.reportPosition = window.reportPosition || function(){};
       before the deferred applyPos call in each file.

[FIXED] ICONS -- CSS mask-image relative URL not resolving in Ultralight 1.4.0
  Files: hud_ammo.html, hud_grenade.html, hud_rads.html, hud_survival.html
  Problem: CSS mask-image: url('../icons/filename.svg') silently failed.
           The <div> elements rendered as zero-size invisible boxes.
           Ultralight 1.4.0 appears not to resolve relative CSS mask-image URLs
           the same way it resolves <img src> URLs.
  Fix: Replaced <div class="icon"> with <img src="../icons/filename.svg">.
       Colorization via CSS filter chain:
         brightness(0) invert(1) sepia(1) saturate(N) hue-rotate(Xdeg) brightness(M)
       This converts any SVG to the target color regardless of original SVG color.
       Filter values per view:
         ammo bullet  : saturate(5) hue-rotate(5deg)   → gold
         grenade      : saturate(3) hue-rotate(30deg)  → olive green
         rads nuclear : saturate(8) hue-rotate(345deg) → orange-red
         survival food: saturate(5) hue-rotate(10deg)  → orange
         survival water:saturate(10) hue-rotate(160deg)→ sky blue
         survival sleep:saturate(10) hue-rotate(220deg)→ purple

[FIXED] EDITOR OVERLAY -- drag zone boxes offset by toolbar height (36px)
  File: assets/views/hud_editor.html
  Problem: #overlay had top: var(--toolbar-h) instead of inset:0.
           .wdz children with absolute positioning were offset by 36px from
           their stored pixel coordinates. Drag boxes never aligned with views.
  Fix: #overlay { position:fixed; inset:0; }

[FIXED] EDITOR DRAG -- clamping used overlay.getBoundingClientRect() for bounds
  File: assets/views/hud_editor.html
  Problem: Drag clamping used overlay.getBoundingClientRect().width/height which
           returns the overlay's clipped rect (0 on transparent views), not the
           viewport size. Widgets could be dragged off-screen.
  Fix: Clamping now uses window.innerWidth and window.innerHeight.


-- Performance audit (Section 2 checklist applied to PrismaHUD_F4) --------------
-- Checklist status:
--   hud_health/ap/rads/ammo/grenade/container/survival/interaction/compass:
--     [PASS] No drag handlers (HUD views are display-only, no interaction)
--     [PASS] No auto-scroll
--     [PASS] No layout reads in high-freq callbacks
--     [WARN] setInterval 80-500ms (see OPEN PERF below)
--   hud_editor.html:
--     [FAIL] Layout reads in mousemove (see OPEN PERF below)
--     [FAIL] style.left/top written in mousemove without rAF
--     [FAIL] C++ moveWidget called every mousemove without debounce
--     [PASS] window.innerWidth cached at drag start (mousedown), not per-event
--     [PASS] No auto-scroll

[OPEN] PERF -- hud_editor.html drag handler has layout reads inside mousemove
  File: assets/views/hud_editor.html (mousemove handler)
  Problem: window.innerWidth/Height read inside mousemove for clamp bounds.
           These are layout-triggering reads (ISSUE 1 from Section 2).
           updateDZEl() writes el.style.left/top/width/height on every mousemove
           — layout writes at mouse polling rate (ISSUE 2 from Section 2).
           moveWidget() calls C++ InteropCall on every mousemove — no debounce
           (ISSUE 5 from Section 2 — may queue AddTask faster than processing).
           No rAF throttling in place.
  Fix plan: Cache window.innerWidth/Height at mousedown (not mousemove).
            Use style.transform = 'translate(x,y)' on .wdz during drag
            (write DIRECTLY in mousemove — transform has no layout cost per
            ISSUE 7 from Section 2). Commit to left/top on mouseup only.
            Debounce moveWidget C++ call with rAF (one call per render frame).
  Priority: MEDIUM — editor only; affects feel but not gameplay FPS.

-- Full-Editor Redesign (2026-06-02 brainstorm session) -------------------------
-- Spec: docs/superpowers/specs/2026-06-02-prismaHUD-full-editor-design.md
-- Status: FULLY IMPLEMENTED (2026-06-02) — all 7 bugs fixed, HTML redesign complete

[FIXED] TYPE MISMATCH -- g_api and SetAPI mismatch in main.cpp + HUDLayoutManager.h
  Files: src/main.cpp, src/HUDLayoutManager.h
  Problem: g_api is declared as IVPrismaUI4* (correct for BindUIEvent) but
           HUDLayoutManager::SetAPI() accepts IVPrismaUI2*. m_api member is also
           IVPrismaUI2*. Means BindUIEvent calls will silently fail or crash
           because the API pointer is cast to the wrong interface version.
  Fix: Change m_api and SetAPI parameter to IVPrismaUI4* in HUDLayoutManager.h/cpp.

[FIXED] THREADING -- 3 editor JS→C++ listeners use RegisterJSListener (Ultralight thread)
  File: src/main.cpp (OpenEditor, DOM_READY macro usage)
  Problem: moveWidget, saveLayout, requestEditorData registered via RegisterJSListener.
           These callbacks touch m_savedPos, call SaveLayoutFile(), invoke BuildEditorPayload()
           — all of which read/write game-thread data. Race condition on every drag/save.
  Fix: Replace all three with BindUIEvent. BindUIEvent fires on the game thread.

[FIXED] THREADING -- editor view reuses handle, re-registers listeners each open
  File: src/main.cpp (CreateEditorView) + src/HUDLayoutManager.cpp (OnEditorDomReady)
  Problem: When editor is closed and re-opened without a game reload, the PrismaView
           handle is reused but DOM_READY fires again, re-registering all listeners.
           Duplicate BindUIEvent registrations cause double-fire on every event.
  Fix: Add bool m_editorListenersRegistered to HUDLayoutManager. Guard registration
       in OnEditorDomReady — only register if !m_editorListenersRegistered.

[FIXED] FILE I/O UNDER LOCK -- SaveFromJS calls SaveLayoutFile while holding m_lock
  File: src/HUDLayoutManager.cpp (SaveFromJS / saveLayout handler)
  Problem: m_lock (shared_mutex) is held across the full SaveLayoutFile() call, which
           does synchronous disk I/O. Any concurrent game-thread reader that tries to
           acquire shared_lock will stall for disk duration (potentially 50-200ms).
  Fix: Snapshot m_savedPos into a local map under unique_lock, release lock, then
       write the snapshot to disk outside the lock.

[FIXED] MISSING DATA -- BuildEditorPayload does not emit defaultX/Y/W/H
  File: src/HUDLayoutManager.cpp (BuildEditorPayload)
  Problem: Editor JS needs default positions to implement "Reset to defaults" and to
           show deltas. Currently payload only sends current positions. entry.desc
           contains the default values but they are not serialized.
  Fix: Add "defaultX", "defaultY", "defaultW", "defaultH" from entry.desc to the
       per-widget JSON object in BuildEditorPayload.

[FIXED] MISSING DATA -- BuildHealthJSON omits radiation data
  File: src/HUDDataProvider.cpp (BuildHealthJSON)
  Problem: Returns {hp, hpMax} only. Health widget needs rads/radsMax to render the
           red radiation overlay bar. Without these the overlay never appears.
  Fix: Merge rads and radsMax (from ActorValue "Rads") into BuildHealthJSON return.
       Remove standalone BuildRadsJSON or keep it for the separate rads widget.

[FIXED] COORDINATE MIGRATION -- layout.json may contain absolute pixel positions
  File: src/HUDLayoutManager.cpp (LoadLayoutFile)
  Problem: Old sessions saved absolute pixel coords (e.g. x=960, y=540). New system
           stores fractions (0.0–1.0). A user upgrading from an old save will see
           all widgets snap to top-left (fractions interpreted as near-zero pixels).
  Fix: In LoadLayoutFile, detect values > 1.0 for x/y (or > 1.0 for w/h) and
       divide by 1920.0/1080.0 respectively before storing.

[OPEN] PERF -- HUD view polling intervals may be too fast (Section 2 ISSUE 5)
  Files: ALL 9 HUD HTML views
  Current intervals:
    hud_health:      100ms  (10fps polls)
    hud_ap:          100ms
    hud_compass:     100ms
    hud_rads:        150ms
    hud_ammo:        100ms
    hud_grenade:     200ms
    hud_interaction:  80ms  ← fastest
    hud_container:   500ms
    hud_survival:    500ms
  Section 2 ISSUE 5: "Auto-refresh >= 1000ms. Interval < processing time queues
  AddTask → freeze." Risk depends on how fast BindUIEvent callbacks execute.
  For HUD callbacks (single GetActorValue call, one InteropCall) processing
  should take <1ms — well within 100ms intervals. Likely safe in practice.
  The 80ms interval on interaction prompt is the highest risk since it polls
  most aggressively while returning empty data every time.
  Action: Monitor for frame drops during combat (health/ammo at 100ms).
          If drops occur, increase to 200ms for health/AP, 500ms for rads.
  Priority: LOW — assess in-game before changing.

[FIXED] RELIABILITY -- Editor RegisterJSListener bindings not re-registered on view reuse
  File: src/main.cpp (CreateEditorView + kPostLoadGame handler)
  Section 3 rule + Section 7 fix: "When IsValid() is true on reload, CreateView
  is skipped and DOM ready does NOT re-fire. RegisterJSListener is not guaranteed
  to persist on reused views."
  Problem: OnEditorDomReady registers moveWidget, saveLayout, closeEditor via
           RegisterJSListener. If the editor view survives a game reload (stays
           valid), CreateEditorView early-returns and OnEditorDomReady never re-fires.
           These three bindings may be dead on reload — editor drag/save silently
           stops working.
  Fix: In kPostLoadGame handler, after CreateEditorView, add:
         else { OnEditorDomReady(g_editorView); }  // re-register defensively
       Same defensive pattern applied to PrismaDebugger (Section 7) and
       PrismaJournal (Section 1).
  Note: BindUIEvent bindings (requestUpdate, reportPosition) persist — only
        RegisterJSListener bindings need re-registration. Editor uses
        RegisterJSListener for all three callbacks.
  Priority: HIGH — silent failure, hard to diagnose.


-- Architecture / Rules specific to PrismaHUD_F4 --------------------------------

WIDGET POSITION SYNC FLOW (understand before modifying):
  1. CreateView → DOM ready → BindUIEvent registers requestUpdate + reportPosition
  2. setTimeout(50ms) fires in JS → applyPos(viewport-fraction coords) runs
  3. window.reportPosition({id, x, y, w, h}) → OnPositionReport → UpdatePositionFromReport
  4. From this point layout manager has the real viewport-relative position
  5. Editor uses BuildEditorPayload() which reads these live positions

[FIXED 2026-06-19] FW_PipboyHUD suppressed in all interiors — wrong worldspace gate
  Problem: IsPlayerInWorld() only returns true when the player's cell worldspace is the
    Commonwealth (0x3C). This excludes ALL interiors, Vault 111, Far Harbor, Nuka-World,
    and any mod worldspace — and any save loaded while indoors.
  Fix: Gate on Pip-Boy POSSESSION instead. Check FillsBipedSlot(kPipboy=30) on player
    inventory. Player receives Pip-Boy on leaving Vault 111 and carries it everywhere
    afterward. This skips prewar/Vault 111/loading without excluding interiors.
  See COMMONLIB_FINDINGS.md Section [CONFIRMED 2026-06-18] for the full possession-
    check code pattern (ForEachStack + FillsBipedSlot(30)).
  Note: Pip-Boy possession check fires WHILE the Pip-Boy menu is open (player receives
    it with the menu open in the intro), so guard against opening chat/HUD views while
    a Scaleform menu is up.
  6. Drag in editor → moveWidget JS → LiveMove C++ → ApplyToView → InteropCall
     → setHUDPosition JS → applyPos → reportPosition (round-trips to sync)
  RULE: The 50ms deferred setTimeout is load-bearing. If you reduce it to 0 the
        BindUIEvent may not have completed registration on the Ultralight thread.
        If you increase it significantly views flash at CSS-default positions.

BIND MACRO vs RegisterJSListener:
  BIND macro in HUDDataProvider uses BindUIEvent (IVPrismaUI4) for requestUpdate.
  This fires on the GAME THREAD — RE:: access is safe inside those callbacks.
  RegisterJSListener (Ultralight thread) is used for editor callbacks only
  (moveWidget, saveLayout, closeEditor) which do not touch RE::.
  NEVER swap these without updating threading rules above.

VIEW HANDLE LIFECYCLE:
  g_builtin stores PrismaView handles. On kPostLoadGame:
    1. If handle still valid (IsValid), view is reused. DOM ready does NOT re-fire.
       BindUIEvent bindings persist on the view. LoadAndApply re-pushes saved pos.
    2. If handle invalid, g_builtin is zeroed, CreateBuiltinViews recreates everything.
  CONSEQUENCE: If you add a new BindUIEvent in a DOM ready callback it will NOT
               fire on subsequent reloads unless the view is actually recreated.
               One-time setup in DOM ready is fine. Dynamic data stays in BindUIEvent.


-- Quest direction / compass blip RE investigation (2026-06-02) ----------------

[OPEN][LOW] COMPASS questDir — BGSQuestObjective not in CommonLibF4
  Researched: BGSQuestObjective forward-declared only (no body). QuestTargetArray
  forward-declared only. BGSRefAlias::GenericFillData is uint64_t data[3] with no
  field names. Cannot navigate quest→objective→target→position without RE.

  IDA PATH TO IMPLEMENT (when time available):
  1. Unpack Fallout4.exe (1.11.191) via Steamless (E:\F4SE OG\Reference\Steamless-master)
  2. Open in IDA Pro. When prompted for symbols, say No.
  3. Alt+F7 → run E:\F4SE OG\Reference\BethesdaGhidraScripts\extras\IDAImportNames_1.11.191.0.py
  4. G → go to 0x140D60880 (PlayerCharacter::CheckForQuestTargetUpdate)
  5. F5 for pseudocode. Paste output here — I extract struct offsets and implement.

  ADDRESS LIBRARY CROSS-REFERENCE (1.11.191):
    PlayerCharacter::CheckForQuestTargetUpdate → RVA 0x00D60880, REL::ID 2232998
    PlayerCharacter::RemoveQuestObjectives     → RVA 0x00D60C10, REL::ID 2233000
    HUDCompassBase::HUDCompassBase (ctor)      → RVA 0x009B7EF0, REL::ID 2220154
  NOTE: These REL::IDs are 1.11.191 NG-specific. Not present in 1.10.163 OG address bin.
  For OG offsets: use IDADebugger structural hash script (E:\F4SE OG\Prisma\IDADebugger)
  with the above RVA as OLD target, running on 1.10.163 to find equivalent.

  IDADebugger script role: finds where functions MOVED between versions (not struct layouts).
  Use it AFTER IDA analysis to get OG offsets from NG confirmed function.


-- Quick-loot workshop-bench transfer-only view (2026-07-13) ------------------

[FIXED] Duplicate "R Transfer" hint in transfer-only view
  hud_quickloot's transfer-only render showed "R Transfer" twice: the centered
  .ql-transfer-msg AND the footer's R key-hint (the only footer hint not wrapped
  in a class hidden by #root.transfer-only). Fixed: footer R hint tagged
  .ql-hint-transfer, hidden under #root.transfer-only in hud_quickloot.css.

[FIXED] Workshop Framework's "Manage Workbench" perk choice never appeared/fired
  QuickLootController::BuildSnapshot()'s workshop-bench branch early-returned a
  minimal transferOnly payload BEFORE the activateChoices computation block ran,
  so any perk-added activate choice on the bench (e.g. Workshop Framework's
  "Manage Workbench", confirmed via its own MCM config.json
  ManageWorkbench:FrameworkOptions / WorkshopFramework.esm|15132) was silently
  dropped from the JSON. Separately, QuickLootInput::OnButtonEvent's workshop-
  bench input gate only let isOpenFull (R) through -- isChoice1/2/3 (number keys
  1/2/3, the real engine buttonIndex) were blocked, and hud_quickloot.js's
  transferOnly branch unconditionally blanked qlChoices.innerHTML instead of
  calling renderChoices(). All three fixed together: activateChoices hoisted
  above the workshop-bench branch in BuildSnapshot, OnButtonEvent gate widened
  to allow isChoice1/2/3, JS renders choices in transfer-only mode too.

[CORRECTED] Mod identification: "press spacebar to manage the settlement"
  Previously misattributed to Settlement Menu Manager (based on reading its
  MainScript.psc, which only operates inside vanilla WorkshopMenu via FormList
  injection). User corrected: it's Workshop Framework (kinggath,
  github.com/kinggath/WorkshopFramework), confirmed via WSFW's own MCM
  config.json "Manage Workbench" alternate-activation switcher. WSFW adds this
  through the same vanilla ActivateChoiceListener/perk-driven activate-choice
  mechanism as WH Butchering's "Field Dress" -- PrismaHUD's existing
  activateChoices plumbing is the correct integration point, no dedicated WSFW
  patch needed beyond the fixes above.

-- Interaction prompt "keeps moving" -- vanilla HUDRollover left unsuppressed (2026-07-13) ------

[FIXED] "The E button keeps moving to different places" -- it wasn't PrismaHUD's own
  hud_interaction widget moving (its position is static, only changes via the layout
  editor/persisted layout.json, verified by inspection of ApplyToView call sites -- no
  per-poll reposition exists). It was TWO independent prompts rendering at once: vanilla's
  own HUDRollover widget (CenterGroup_mc.RolloverWidget_mc, FO4's built-in centered
  "E) <activate text>" prompt for every activatable target) was never in kSuppressedWidgets,
  so it kept rendering alongside PrismaHUD's own hud_interaction prompt. The two overlapping/
  alternating (different text, different exact layout per target type) read as "the prompt
  moves around."
  Left unsuppressed for a long time on an OUTDATED assumption that hiding it would break
  perk-driven activate choices (WH Butchering's Field Dress, Workshop Framework's Manage
  Workbench). Verified via RVA that this assumption no longer holds (if it ever did): the
  activate-CHOICE capture is a detour on ActivateChoiceListener::PopulateData at vtable RVA
  0x2d21120 (VanillaUISuppressor.h part D) -- a COMPLETELY SEPARATE object/vtable from
  HUDRollover's own CanBeVisible-suppression vtable at RVA 0x2d192c8 (kKnownWidgets table,
  VanillaUISuppressor.cpp). Hiding the widget (slot 7, CanBeVisible) cannot affect the
  listener's own data-population detour (slot 2, different vtable entirely) -- PrismaHUD's
  own capture+render (QuickLootController's activateChoices / GetActivateChoiceLabel/
  TriggerActivateChoice, see the 2026-07-13 workshop-bench entry above) is unaffected.
  Fix: added "HUDRollover" to kSuppressedWidgets in HUDDataProvider.cpp. Now only
  PrismaHUD's own hud_interaction widget renders the generic single-activate prompt (fixed,
  centered position, matching vanilla's visual intent) and only QuickLootController's own UI
  renders container/corpse/workshop-bench prompts and perk choices.

-- Workshop-bench false-negative + PauseMenu leak + component-tag name artifact (2026-07-13 pt2) --

[FIXED] Workshop bench showed the FULL item list instead of Transfer-only (screenshot evidence:
  a real settlement RobCo toolbox with 9 items rendered header+list+carry-bar+full footer, not the
  minimal "R Transfer" view). Root cause: the detection --
  ref->extraList->HasType<Workshop::ExtraData>() -- was checking the WRONG thing. Workshop::ExtraData
  (RE/W/Workshop.h: currentPowerGrid/powerGrid/deletedItems/powerRating) is the SETTLEMENT's own
  power-grid/tracking state, attached to the site's single canonical workshop object -- not to every
  physical storage container that feeds the shared workshop inventory pool. A container can be a
  fully valid shared-pool workshop container with zero ExtraData of its own.
  Fix: added RE::Workshop::FindNearestValidWorkshop(*ref) as a second detection path (kept the
  ExtraData check too, OR'd together) -- a real, addressable native function, already proven in this
  exact codebase (PrismaCrafting's GetWorkshopSourceRefs() uses the same function as its own
  "resolve the settlement's workshop" fallback when not in build mode). Compare the result's FormID
  against the target ref's own FormID: true means the ref IS, by the engine's own reckoning, its own
  location's canonical workshop reference.
  This almost certainly also explains "quick container is slow to react when rolling over a new
  item, and so is the activate view" for workshop targets specifically: the old false-negative meant
  every poll (200ms) ran the FULL inventory walk + per-item value/weight/model-path computation +
  mesh-preview show() calls against the shared settlement pool (easily hundreds of components in a
  developed settlement, not the ~9 shown in the one screenshot), on the main thread, every poll --
  now that detection correctly takes the fast transferOnly path (BuildSnapshot's own comment: "skip
  the potentially huge -- hundreds of items -- inventory walk entirely"), that per-poll cost drops to
  near zero for workshop targets, which should also fix the perceived reaction lag for BOTH quick-loot
  and the interaction prompt (both run as BindUIEvent callbacks off the same per-tick queue).

[FIXED] HUD (quick-loot, interaction prompt) rendered on top of the ESC/pause menu. Neither is gated
  by the vanilla HUDMenu open/close signal (by design -- see the crosshair-polling exemption noted in
  HUDLayoutManager), and "PauseMenu" (RE::PauseMenu::MENU_NAME, verified via CommonLibF4) was never in
  MenuSuppressSink's watched-menu list. Added it alongside WorkshopMenu/TerminalMenu/ChemistryMenu/
  CookingMenu/LockpickingMenu.

[FIXED] Item names showing a raw "{{{Adhesive, Cloth}}}" component-tag artifact (e.g. "Electrical
  Tape{{{Adhesive, Cloth}}}"). NOT a PrismaHUD bug in the data sense -- verified via FO4RecordEditor
  search_all: the record's actual FullName field literally is "[Scrap] Electrical
  Tape{{{Adhesive, Cloth}}}" (DuctTape01, MiscItem, Fallout4.esm, overridden by "Modern Replacer -
  AIO.esp"). QLStripBracketTag already stripped the leading "[Scrap] " tag but had no handling for
  this mod's trailing "{{{...}}}" component-tag convention -- extended it to also strip any
  "{{{...}}}" run wherever it occurs in the name.

-- Unified interaction prompt (workshop Transfer/choices merged into hud_interaction) (2026-07-13 pt3) --

[FIXED] "The Transfer is still showing in the quick rollover menu when it should show next to the
  E button" / "the activator button should be centered... they are still off to all different
  sides" (user report + screenshots, post-pt2 fix). Confirmed via the live persisted
  Documents\My Games\Fallout4\PrismaHUD\layout.json: prisma_prompt was saved at a stale off-center
  position (x=0.195,y=0.722, bottom-left-ish) from earlier dragging/testing, while
  prisma_quickloot's transferOnly mini-panel rendered as a SEPARATE floating widget at its own
  independently-saved position -- two different CEF views, two different saved coordinates, for
  what vanilla shows as ONE centered row ("E) Take  R) Transfer"). That structural split -- not a
  positioning-math bug -- is what read as "keeps moving."
  Fix: QuickLootController's workshop-bench branch no longer shows its own panel at all (pushes
  {"visible":false}); the cached activateChoices are exposed via WorkshopActivateChoicesJSON() and
  merged into HUDDataProvider::BuildPromptJSON's own payload as an "extras" array
  ([{key,label}, ...]) whenever IsWorkshopBenchTarget() is true (always includes R/Transfer, plus
  any perk choice like WSFW's Manage Workbench). hud_interaction.html/css/js renders extras inline
  next to the main "E) <action>" text, in the SAME widget/position. main.cpp/HUDDataProvider.cpp/
  QuickLootController.h/.cpp/hud_interaction.* all touched.
  NOTE: the STALE saved position for prisma_prompt in the user's live layout.json was NOT reset by
  this fix (that's the user's own out-of-repo save data, not something to edit unilaterally) --
  needs the in-game HUD editor's "Reset Sel"/"Reset All" on the interaction prompt widget, or
  explicit user go-ahead to edit the file directly.

[INVESTIGATED, NOT RESOLVED] "The E button is still too slow to show up" for a plain loose-item
  target (a pistol on a shelf, NOT a container/workshop -- rules out the pt2 heavy-inventory-walk
  theory entirely, since QuickLootController::ResolveTarget() returns null immediately for a
  non-CONT/non-dead-NPC ref, doing zero work). Traced the full pipeline: PickRefUpdateEvent (engine-
  driven) -> QuickLootController::SetCachedPick -> hud_interaction's 100ms JS poll ->
  BuildPromptJSON (GetActivateText + GetFullName, O(1), no loops) -> InteropCall -> JS textContent
  update. Found no inefficiency anywhere in this path -- worst-case latency should be bounded by the
  100ms poll interval, not perceptible as "slow." No further lead without live profiling/tracing;
  not claiming this fixed. Worth re-testing after the pt2/pt3 fixes above (the workshop-heavy-walk
  bug may have accounted for most of what felt slow) before chasing this further.

-- Prompt/quick-loot 1-2s show latency = Chromium hidden-frame timer throttling (2026-07-13 pt4) --

[FIXED] "It takes like 1-2 seconds for it to show Take. It should be instant." (Follow-up to pt3's
  [INVESTIGATED, NOT RESOLVED] note -- that pass traced the pipeline and correctly found no
  inefficiency in the WORK being done, but missed WHY the work started late.)
  ROOT CAUSE (not a slow poll -- a throttled one): both target-driven views (hud_interaction and
  hud_quickloot) are HIDDEN whenever there's no crosshair target. The framework's Hide() bottoms out
  in the CEF shell setting `display:none` on that view's iframe (PrismaUI_F4 New Gen\shell\src\
  main.ts, hideView()). Chromium heavily throttles setInterval in a hidden / non-rendered frame
  (~1Hz) -- so each view's own JS poll (hud_interaction.js: 100ms; hud_quickloot.js: 200ms), which
  was the ONLY thing detecting "a target appeared, wake up", effectively ran about once per second
  while hidden. The view was being asked to notice its own wake-up condition using a timer the
  browser had already throttled precisely BECAUSE the view was asleep. Lowering the poll interval
  cannot fix this -- the interval is not what's slow.
  This is a general trap for ANY Prisma plugin using the hidden-view + self-poll-to-wake pattern
  (see repeat-issues.md): a hidden CEF view cannot be relied on to poll itself back into visibility.
  FIX: make the show path event-driven, not poll-driven. PickRefUpdateEvent (PlayerCharacter's own
  event source) is the engine telling us the exact instant the crosshair target changed -- the
  PickRefSink now, on any ACTUAL change, calls HUDDataProvider::PushPromptNow() (new, shares the
  prompt's dirty-check state -- see below) and QuickLootController::OnRequestUpdate() directly.
  InteropCall executes script immediately rather than scheduling a timer, so it lands even on a
  display:none view; the JS then calls reportVisibility, which un-hides it. The JS polls stay as the
  steady-state refresh for an already-visible (hence unthrottled) view and as a backstop.
  NOTE: BindPromptUpdate no longer uses the BIND macro -- BIND traps its dirty-check `s_last` inside
  its own lambda, unreachable from PushPromptNow. Two independent dirty-checks would let the poll's
  stale state suppress a push the event path needed. The prompt now has an explicit binding sharing
  one g_lastPrompt.

-- Rollover/prompt double-hint, Pipboy leak, and the Manage-Workbench deep dive (2026-07-13 pt5) --

[FIXED] "If rollover window is open then the E/R to transfer should not show." BuildPromptJSON now
  returns hasTarget=false whenever the quick-loot panel genuinely owns the screen (qlc->Active() &&
  !IsWorkshopBenchTarget() && !UserHidden()) -- the panel already renders its own E/T/R/Select hints
  in its footer, so the centered prompt was a second, differently-placed copy. The workshop bench is
  the deliberate exception: the panel does not render there at all, precisely so Transfer/Manage can
  live in the centered prompt (see pt3).

[FIXED] "HUD should not show when pipboy is open." Added "PipboyMenu" (RE::PipboyMenu::MENU_NAME,
  verified against CommonLibF4) to MenuSuppressSink's watched list, alongside PauseMenu et al.

[FIXED] Ordering bug found while in there: the PickRefUpdateEvent sink called PushPromptNow() BEFORE
  QuickLootController::OnRequestUpdate(). OnRequestUpdate is what re-resolves the target and
  refreshes m_isWorkshopBench / m_workshopChoicesJson / m_active -- all of which BuildPromptJSON
  reads. So on the very frame you looked at a new target, the prompt published the PREVIOUS target's
  state (e.g. the bench's Transfer/Manage extras missing). Swapped the order.

[OPEN][HIGH] Workshop Framework's "Manage" activate choice still never appears. STATIC TRAIL IS
  EXHAUSTED -- everything checkable without the game running has now been checked, and all of it says
  this SHOULD work:
    - The record is correct and IS the mechanism we hook. WSFW_ActivationsPerk
      [015131:WorkshopFramework.esm], dumped via FO4RecordEditor: EntryPoint=Activate,
      ButtonLabel="Manage", PerkConditionTabCount=2, conditions
      GetGlobalValue(WSFW_AlternateActivation_Workshop [015132]) == 1 (tab 0) AND target
      HasKeyword(WorkshopKeyword [054BA7:Fallout4.esm]) == 1 (tab 1). That is exactly a
      BGSEntryPointFunctionDataActivateChoice, i.e. the ActivateChoiceListener path our detour hooks.
    - My own HUDRollover suppression (pt1) is RULED OUT as the cause, contradicting the old warning
      comment in kSuppressedWidgets. Decompiled HUDRollover::UpdateDisplayObject (RVA 0xab1820): its
      ShowRollover data path is gated only on the widget's own dirty/show flags (this[0x540],
      this[0x548]), NOT on CanBeVisible -- CanBeVisible is only consulted by the base
      HUDComponentBase::UpdateDisplayObject call at the END, which just sets Scaleform _visible. And
      ShowRollover itself (RVA 0xab19d0) only pushes ButtonHintData into the SWF; it never calls
      PopulateData. So hiding the widget cannot stop the capture detour from being called.
    - The detour installs cleanly at runtime (PrismaUI_F4.log: "installed
      ActivateChoiceListener::PopulateData detour (vtable RVA 0x2d21120, slot 2)").
  WHAT REMAINS IS RUNTIME-ONLY, and I will not guess it. The live unknowns are (a) whether the engine
  ever calls PopulateData with buttonIndex>=1 at a workshop bench, (b) what it writes if it does, and
  (c) whether WSFW_ActivationsPerk is actually ON the player (WSFW adds it from WSFW_Main at startup;
  on an existing save a dormant WSFW quest would mean no perk, hence no choice -- compare
  [[reference_quest_setstage_dormant_esl]]).
  Added a throttled diagnostic to Detour_PopulateData (framework) logging every call's
  btn/valid/listener/label, deduped per (index,label). NEXT IN-GAME PASS: look at a settlement
  workbench, then send the "VanillaUISuppressor: PopulateData btn=..." lines from PrismaUI_F4.log.
  Those lines distinguish the three cases outright: no btn>=1 line at all = the engine never offers
  it (perk not on player / condition failing) -> the fix is on the WSFW/perk side, not ours; a btn>=1
  line WITH label='Manage' = the engine offers it and our capture/plumbing is dropping it -> the fix
  is ours.

-- Manage Workbench: SOLVED by log evidence -- HUDRollover suppression DID kill it (2026-07-13 pt6) --

[CORRECTION of pt5] pt5 claimed HUDRollover suppression was "RULED OUT" as the cause of the missing
  activate choices, based on decompiling HUDRollover::UpdateDisplayObject (0xab1820) and ShowRollover
  (0xab19d0) and finding that neither calls PopulateData. That reasoning was WRONG -- it checked the
  CONSUMERS of the rollover data and concluded the PRODUCER was innocent. The old warning comment in
  kSuppressedWidgets ("DO NOT also add HUDRollover here -- suppressing it kills Field Dress") was
  right all along, and I overrode it.

[EVIDENCE -- the diagnostic added in pt5 settled it in one pass] Session 20:53-21:00 with the
  diagnostic build live (PrismaUI_F4.dll deployed 20:50; log confirms capture enabled 20:53:56 and
  'HUDRollover' suppressed at 20:53:56), player reached a settlement workbench (WorkshopMenu
  open/close logged at 21:00): the log contains ZERO "VanillaUISuppressor: PopulateData btn=" lines.
  Not one -- not even buttonIndex 0. Detour_PopulateData was never called at all. Since the detour is
  confirmed installed, that can only mean the engine never ran ActivateChoiceListener::PopulateData:
  it runs UPSTREAM as part of BUILDING the rollover's data, and that producer is gated on the widget
  being able to be visible. A CanBeVisible=false HUDRollover therefore never populates, and there is
  nothing for the capture to see. This is also why "Transfer" still showed (PrismaHUD hardcodes that
  hint for the bench) while "Manage" never did (it can only come from the capture).

[FIXED] HUDRollover is no longer suppressed via the CanBeVisible vtable patch. VanillaUISuppressor::
  SuppressHUDWidget now special-cases "HUDRollover" and routes it to a new visuals-only path
  (SuppressRolloverVisuals): HUDRollover::UpdateDisplayObject contains exactly one `call ShowRollover`
  (call site RVA 0xab18b0 -> 0xab19d0, located with DisasmAt), and that single call is retargeted, via
  REL trampoline write_call<5>, to a detour that calls HideRollover (0xab1c30) instead. The widget's
  entire data pass -- including the PopulateData the activate-choice capture depends on -- runs
  exactly as vanilla; only the drawing is suppressed. Guarded by a pre-patch verification that the
  bytes at the call site really are an E8 rel32 CALL whose target is ShowRollover (same discipline as
  the vtable patches, after the incident where a hand-copied RVA patched live save/load-game code).
  Callers keep the identical "SuppressHUDWidget('HUDRollover')" API and semantics -- PrismaHUD needed
  no change. OG-only, like every other raw RVA in this file.
  GENERAL LESSON (-> repeat-issues.md): a CanBeVisible suppression does not merely hide a widget, it
  can switch off whatever engine work is done to FEED that widget. Before suppressing a widget whose
  data we also consume, check what stops being produced -- and prefer a visuals-only hook (retarget
  the draw call) over disabling the component.

-- Widget position lost while hidden (the "activator moves all over the screen") (2026-07-13 pt7) --

[FIXED] "The activator keeps moving all over the screen -- I have to keep opening the editor to get it
  to stay in place." ROOT CAUSE, and it is a clean one: applyPos() read window.innerWidth and, if it
  was 0, simply RETURNED -- silently discarding the position. But every auto-hiding widget is HIDDEN
  most of the time, and the framework hides a view by setting display:none on its iframe (CEF shell
  hideView()); a display:none iframe reports innerWidth/innerHeight of 0. So the position C++ pushes
  via setHUDPosition at load arrived while the view was hidden, was thrown on the floor, and #root
  stayed at its CSS default of left:0/top:0 -- the top-left corner (exactly what the screenshot
  shows). Opening the HUD editor "fixed" it only because the editor shows the view and re-pushes a
  position while it happens to be laid out -- which is precisely the workaround the user had found.
  A retry timer cannot fix this: a view that stays hidden never gains dimensions (and its timers are
  throttled while hidden anyway -- see pt4). hud_quickloot's existing ~2s retry loop had the same
  hole, it just expired instead of bailing immediately.
  FIX: remember the wanted position (_want) instead of discarding it, and apply it the moment the view
  actually gains a layout -- a ResizeObserver on document.documentElement fires exactly on that 0 -> N
  transition. Applied to all six position-bearing auto-hiding views: hud_interaction, hud_ammo,
  hud_grenade, hud_rads, hud_survival, hud_quickloot.
  RELATED: this is the same family as pt4 (Chromium throttles a hidden frame's timers). Both come from
  the same wrong assumption -- that a hidden CEF view can still measure/schedule like a visible one.
  It cannot. -> repeat-issues.md.

-- Manage Workbench: my pt6 conclusion was WRONG; still unresolved (2026-07-13 pt7) --

[CORRECTION of pt6] pt6 concluded "HUDRollover suppression DID kill the activate-choice capture",
  based on a session with the suppression ON showing zero PopulateData calls. That conclusion does not
  hold. In the NEXT session the HUDRollover patch was REFUSED at startup (the new call-site guard
  rejected it, see below), so HUDRollover was NOT suppressed at all -- and PopulateData still logged
  ZERO calls. Two sessions, suppression on and off, zero calls both times. Suppression is therefore
  NOT the differentiator, and the real reason the engine never calls PopulateData is still unknown.
  I have now been wrong about this twice by reasoning ahead of the evidence; not doing it a third time.

[VERIFIED, so it is NOT the cause] The hook target itself is correct. Dumped
  VTABLE_ActivateChoiceListener @0x2d21120: [1] UpdatePickRef @0x832870, [2] PopulateData @0x8329a0.
  The detour is on the right vtable and the right slot, and the log confirms it installs.

[FIXED, my own bug] The pt6 ShowRollover call-site guard refused to patch, reporting "the instruction
  at RVA 0xab18b0 is not a CALL to ShowRollover". The guard was right to refuse, but the RVA was fine
  -- the CHECK was broken: it used REL::Relocation<std::uint8_t*>::get(), and get() DEREFERENCES (for
  T = uint8_t* it reads 8 bytes AT the address and returns them as a pointer), so it inspected garbage
  instead of the instruction bytes. Disassembly confirms 0x140ab18b0 really is `CALL 0x140ab19d0`
  (5-byte E8). Fixed to read from site.address() directly. (The safety guard doing its job and
  catching my own mistake is exactly why it exists.)

[NEXT STEP -- one discriminating signal, no more theories] Added a diagnostic detour on slot 1
  (ActivateChoiceListener::UpdatePickRef). It splits the remaining possibilities cleanly:
    - UpdatePickRef fires, PopulateData does not => the listener IS live but has no choices to
      populate => the perk is not applying (not on the player, or a condition failing). WSFW adds
      WSFW_ActivationsPerk from WSFW_Main at startup; on an existing save a dormant WSFW quest means
      no perk, hence no choice. That would be a WSFW/save-state issue, NOT a PrismaHUD one.
    - Neither fires => this listener is never used for our targets at all => the whole capture
      approach is aimed at the wrong mechanism and needs rethinking.
  Next in-game pass: look at the workbench, then send the "UpdatePickRef call #" / "PopulateData btn="
  lines from PrismaUI_F4.log.

---

## [FIXED 2026-07-14] BindUIEvent does NOT pin to the game thread -- the whole HUD data path was racing

`HUDDataProvider.cpp` stated, and relied on, "Data builders (all called on game thread via BindUIEvent)".
**That contract is false**, and the framework's own log proves it.

`PluginAPI::PrismaUIInterface::BindUIEvent` (API.cpp:780) wraps the callback in
`F4SE::GetTaskInterface()->AddTask`, and its "BindUIEvent fired" log line is the first statement INSIDE
that task. In one session that line printed six distinct thread ids (24712, 7008, 25736, 26196, 21368,
25608) across 7,467 fires.

**These are engine threads, not CEF threads** -- each of them also ran F4SE message dispatch, co-save
REVERT/LOAD serialization, and `MenuOpenClose` sink callbacks, and none of them ever appears in a CEF/JS
context. So the per-thread allocator (`MemoryManager`/`ScrapHeap`) is valid on them; this is NOT the
"foreign thread corrupts the allocator" bug class.

**The proof that it is a POOL and not one thread:** co-save REVERT ran on tid 7008 at 11:41:49 and on tid
24712 at 11:45:23 in the same process. An OS thread id never changes, so F4SE's task/serialization
callbacks are dispatched by more than one thread. `AddTask` buys you allocator safety, NOT mutual
exclusion.

What was racing, every single widget poll:
- The `BIND` macro's `static std::string s_last` -- an unsynchronized read-modify-write.
- `g_lastPrompt` -- shared between the poll path AND the `PickRefUpdateEvent` sink (`PushPromptNow`).
- The builders themselves (`BuildCompassJSON` walking quest objectives, `BuildAmmoJSON` reading
  `EquippedItemData`) running concurrently with each other.
- `QuickLootController::OnRequestUpdate` mutating its snapshot from the same threads.

**Fix:** one serialization point, `HUDDataProvider::DataLock()` (recursive -- `OnRequestUpdate` calls
`PushPromptNow`). `InteropCall` is issued outside the lock so the lock is never held across a CEF hop.

**This same false assumption is load-bearing elsewhere.** PrismaCrafting's JS listeners were "fixed" by
marshalling them through `AddTask` on the belief that this pinned them to the main thread. It does not.
That code is allocator-safe but still unserialized -- see `prismacraft.md`.

Whether this race is THE cause of the XAudio2 heap-corruption crash is NOT yet established; it is a real
bug independent of that, and the crash repro is the confirmation run.

---

## [CORRECTION 2026-07-17] "HUDPlusPlus sigscan conflict" was never a real finding -- HUDPlusPlus is not installed

A later conversation (this one) carried forward a claim that HUDRollover's `ShowRollover` call site was
externally hooked by HUDPlusPlus, and that this forced the fallback to CanBeVisible vtable suppression.
**That is not in this file anywhere, and it is not true.** Checked directly: no `HUDPlusPlus` mod folder
under `E:\Modlists\Fallen World Alpha 2\mods\`, no such DLL anywhere under any mod's `F4SE\Plugins\`, no
`HUDPlusPlus.log` in the F4SE log directory. It was never installed in this modlist. Do not cite it again
without re-verifying against the actual modlist first.

The REAL, already-documented history of the Manage-button investigation is the pt1-pt7 saga above
(2026-07-13, "Manage Workbench" sections): a genuinely wrong hardcoded RVA (0xab18b0 mid-instruction) was
first blamed, then a buggy verification guard (`REL::Relocation<std::uint8_t*>::get()` dereferences
instead of reading the instruction bytes) was found and fixed, disassembly then CONFIRMED the call site
really is `CALL 0x140ab19d0`, and a working call-site-only fix (`SuppressRolloverVisuals`, retargeting
just the one `call ShowRollover` to `HideRollover` so 100% of the engine's own data pass runs untouched)
was implemented and marked `[FIXED]` at the time. **That function does not exist in the current
`VanillaUISuppressor.cpp`** -- it was reverted at some point without a KB entry explaining why, and the
comment now in its place (`SuppressHUDWidget`, "the vtable slot-7 CanBeVisible patch is preferred... Do
not reintroduce the detour") repeats the pt6 finding pt7 already superseded. Currently deployed:
plain CanBeVisible vtable suppression on `HUDRollover`, confirmed via runtime log
(`SuppressHUDWidget('HUDRollover') -> true`) and via a passive diagnostic that the bench's
`QuickContainerStateData` reads `NOT POPULATED` while it's active.

Also newly clarified: there appear to be TWO separate engine mechanisms in play, not one --
`ActivateChoiceListener::PopulateData` (indexed choice slots, e.g. WH Field Dress) vs.
`FavoritesManager::quickContainerMode` (`QuickContainerStateData.perkButtonText`, a single slot, read by
`QuickLootController::EnginePerkButtonLabel()`). Which of the two actually produces WSFW's "Manage" text
is still not nailed down with certainty from static analysis alone -- the KB's own pt5/pt6/pt7 corrections
show two earlier passes reasoning ahead of the runtime evidence and getting it wrong both times ("I have
now been wrong about this twice... not doing it a third time" -- pt7). Re-running the same live Shift+F9
diagnostic (`main.cpp::RegisterRolloverDiagToggle`, still present and deployed) at a real bench, this
session, instead of re-guessing. See CHECKLIST.md for the pending result.

--------------------------------------------------------------------------------
## [2026-08-07] Issue tracking moved to GitHub; two long-standing items fixed
--------------------------------------------------------------------------------

**Tracker.** `github.com/PRISMA-USER-INTERFACE-FRAMEWORK/PrismaHUD` had zero issues. The full backlog
from `CHECKLIST.md` + the plugin's `docs/STATUS.md` is now filed there as 21 open issues plus 2 closed
record issues (#22 shipped baseline, #23 the 2026-07-17 session's completed work). This KB stays the
place for root-cause writeups; GitHub is now the place for *what is open and who is waiting on what*.
Numbers referenced below are GitHub issue numbers.

**[FIXED] #19 — `IsSanePos` bounded the origin only, so a scaled edge widget could vanish.**
`HUDLayoutManager.cpp`'s `IsSanePos` clamped `x`/`y` to roughly `[-0.1, 1.001]` and `s` to `[0.2, 3.0]`,
but never considered `origin + intrinsic * s`. A widget at a legal edge origin with a large scale ended
up almost entirely off-screen, and nothing recovered it -- the only fix was deleting `layout.json`.

New `ClampExtent` bounds the projected extent so at least 25% of the widget stays on screen per axis,
using `WidgetDesc::defaultW/H` (a viewport fraction; `0` = unknown, axis skipped). Applied at two depths
deliberately: `ApplyToView` clamps into LOCALS (it runs under a *shared* lock from `ApplyLayout`, so it
must not mutate, and it is also the `LiveMove` drag path where rewriting the stored value would fight the
editor's drag box); `RecoverOffscreen` is the load-time path (`RegisterWidget`, `LoadAndApply`) under the
exclusive lock, and mutates both `WidgetEntry::x/y` and the widget's `m_savedPos` JSON so the recovery
persists past the next `SaveLayoutFile`. An extent wider than the viewport pins to origin `0` rather than
solving an empty range. Commit 5936755.

**[FIXED] #12 — `s_diag` ammo diagnostic removed.** Added 2026-06-30 "until ammo is confirmed", still
spamming `PrismaHUD_F4.log` every ~10s five weeks later. Gone, along with `eqCount` and `dataNull`, which
it was the sole consumer of; the `if (!base)` branch (body: `dataNull = true`) folded into an
`else if (base)` guard. The neighbouring `s_vt` warning was KEPT on purpose -- it is a regression detector,
not a diagnostic: it fires only when `EquippedItemData`'s runtime vtable stops matching
`EquippedWeaponData::VTABLE[0]` (CommonLibF4 offset drift), and without it the ammo path would silently
read garbage from `ammo@0x10` / `ammoCount@0x18`. Same commit.

**[OPEN][LOW] Both fixes are UNBUILT.** Written on a Linux machine with no MSVC toolchain (`xmake f -p
windows` fails at "checking for Microsoft Visual Studio ... no"). They need an `xmake` build on Windows,
a deploy, and an in-game check that a scaled edge widget is recoverable, before either is trusted.
Tracked as GitHub #12 / #19, both relabelled `status: needs testing`.

**[2026-08-07 pt2] Backlog pass: #6, #13, #17 (all UNBUILT, see pt1 for why).**

- **[FIXED] #6 quick-loot value/weight.** View-only (`hud_quickloot.js`); the row JSON already carried
  both fields. Per-UNIT ratio so it is count-independent; omitted rather than `inf` when weight or value
  is 0. Commit bbdb5fa - ships with a file copy, no DLL needed.
- **[FIXED] #13 quick-loot legendary.** Decision was "keep value bands AND add a real legendary check",
  so they are now two independent JSON fields instead of `QLRarity` collapsing legendary over the band.
  Detection is the engine's own `ExtraDataList::GetLegendaryMod` (VariantID 178075/2190180, declared in
  CommonLibF4 `RE/E/ExtraDataList.h`) - **not** a keyword guess. **Key structural fact:** the legendary
  mod lives on `BGSInventoryItem::Stack::extra` (@ 0x18), never on the base form, so every existing
  read here (all passing `nullptr` for the instance) was incapable of seeing one. Hence the stack walk
  in `QLIsLegendary`. Rows aggregate stacks, so any legendary stack marks the row. Commit f2a6e01.
- **[FIXED] #17 slot rename.** Recorded as "Task 3 not started"; it was actually **implemented and
  broken three ways**. (a) The only path to the rename input was the Save button, and `SaveSlot`
  unconditionally does `m_slots[i].layout = m_savedPos` - so relabelling silently destroyed the slot's
  saved layout. New `RenameSlot` sets the label only. (b) Escape called `blur()` and the blur handler
  *was* the commit path, so Escape saved. (c) `{once:true}` on the keydown listener removed it after the
  first keystroke, so Enter/Escape only worked as the very first key pressed. Commit 79aaa87.

**[LESSON] A negative symbol-grep is a hint, not a finding.** STATUS.md listed v1-spec Tasks 2/2B/3/4 as
unstarted on the strength of grepping for `setWidgetIcon` / `ImportLayout` / rotate / etc. Task 3 was in
fact built - the grep just looked for names that implementation never used. Tasks 2, 2B and 4 were
re-checked properly on 2026-08-07 and ARE genuinely absent (GitHub #15/#16/#18), but the method that
produced the original claim was unsound and produced one false negative out of four.

**[2026-08-07 pt3] More backlog: #8 (partial), #10, #16. All need a Windows build.**

- **[FIXED] #10 junk scrap components.** `TESObjectMISC::componentData` @0x158 is
  `BSTArray<BSTTuple<TESForm*, SharedVal>>*`; `.second.i` is the count and `BGSComponent` inherits
  `TESFullName`. Header-verified, no guessed offsets. Own row (`ql-row3`), capped at 4. Commit 8262b77.
- **[PARTIAL] #8 weapon detail.** Ammo type shipped (`InstanceData::ammo` @0x068). **Constraint:** this
  is the base-form read (`nullptr` instance, as everywhere in `BuildSnapshot`), so a receiver mod that
  changes caliber will not show. Fire rate and accuracy deliberately NOT shipped -- `accuracyBonus`
  (int8 @0x136) is a bonus, not the Pip-Boy's aim-model-derived accuracy, and fire rate has three
  plausible sources (`rangedData->fireSeconds`, `attackSeconds` @0x0C0, `speed` @0x0C8) with nothing in
  the headers to choose between them. Vanilla's display is a derived stat.
- **[FIXED] #16 layout export/import.** Files in `PrismaHUD\exports\`. **Security constraint worth
  remembering across plugins:** the name is untrusted editor text reaching the filesystem, so it is
  whitelisted to `[A-Za-z0-9 _-]`, trimmed, capped -- AND reserved DOS device names are broken, because
  `exports\CON.json` still resolves to the console device on Windows. Character filtering alone is not
  enough. Import drops corrupt entries individually via `IsSanePos` rather than failing the whole file,
  and runs each through `RecoverOffscreen`. Commit 3e5c37f.

**[OPEN][MED] #7 special icons are NOT header-solvable -- do not re-derive this.** All three signals
need Ghidra or an in-game probe: (a) `BGSInventoryItem::IsQuestObject` takes a stackID and
`BGSInventoryItem::Stack` has no id member in this fork; (b) `OBJ_BOOK::teaches` is a union selected by
`OBJ_BOOK::flags`, and CommonLibF4 has NO FO4 BOOK flag enum -- the only one in the workspace is
CommonLibSSE's (Skyrim), and reusing it is the same cross-game assumption that forced Ghidra
verification on the compass; a misread dereferences the wrong pointer type; (c) "already read" has no
getter -- only `AddChange`/`RemoveChange` setters and `CHANGE_TYPES::kBookRead = 1u << 6`.
