# PrismaHUD — Working Checklist

> **Superseded 2026-08-07.** Everything below now lives on the issue tracker:
> https://github.com/PRISMA-USER-INTERFACE-FRAMEWORK/PrismaHUD/issues
> Track work there. This file is kept as the snapshot the tracker was built from, and should not be
> updated further — the mapping is: in-game verification pass → #1, Manage button → #2, gamepad perk
> button → #3, SPECIAL re-skin → #4, item-detail cards → #5-#10, diagnostics cleanup → #11-#12,
> design decisions → #13-#14, editor tasks → #15-#18, off-screen bound → #19, GetAsyncKeyState → #20,
> uniform-scale verification → #21. #22/#23 are closed records of shipped work.

Living to-do list. Check items off as they're confirmed. Grouped by what's blocking what.

_Last updated: 2026-07-17_

---

## 🔴 Awaiting your in-game confirmation (deployed, needs a play session)

These are built + deployed; they just need a look in-game to close out.

- [ ] Controller glyphs show **during gameplay** (not only when a menu is open)
- [ ] Controller drives **focused Prisma views** — D-pad navigates, A confirms, B cancels (message box, WH field-dressing menu)
- [ ] Message-box **body glyphs render as neutral keycaps on keyboard** (workshop tutorial is the reliable test) — _screenshot wanted_
- [ ] Activate prompt shows **verb only** ("Read", not "Grognak the Barbarian Read")
- [ ] **Clicking/hitch is gone** (CEF texture-probe + device-thrash fixes)
- [ ] **No crash** walking up to a toolbox / activating a container (trampoline fix)
- [ ] **No crash** opening the wheel with modded weapons in inventory (instance-extra null guard)
- [ ] **Take grabs one item, the highlighted one** (vanilla double-take call-site fix)
- [ ] **Field Dress still works** on a corpse (must survive all the quick-loot changes)

## 🟠 Bug O — real gamepad perk button (needs you, controller in hand)

- [ ] At a corpse/bench **on controller**, report which face button actually fires **Field Dress / Manage**
- [ ] Then read that button live from ControlMap instead of the current hardcoded `"Y"` assumption

## 🟡 HUDRollover / the missing "Manage" button

**CORRECTED 2026-07-17: the HUDPlusPlus theory was wrong** — checked the live modlist directly, it isn't
installed anywhere (no mod folder, no DLL, no log). See `docs/Prisma/findings/prismahud.md` correction
entry. Confirmed still missing in-game (toolbox screenshot, 2026-07-17) with the vtable-suppressed
HUDRollover build. The framework once had a verified call-site-only fix (`SuppressRolloverVisuals` —
retargets just the one `call ShowRollover` to `HideRollover`, leaving the engine's whole data pass
untouched) but it's gone from the current `VanillaUISuppressor.cpp`, reverted at some unknown point.

- [ ] **Pending: Shift+F9 at a bench, live in this session** — flips HUDRollover suppression and dumps
      the raw quick-container state before/after to `PrismaHUD_F4.log`. This is the authoritative test;
      two earlier sessions reasoned ahead of this exact evidence and were wrong both times (see KB).
- [ ] Based on that result: either re-implement `SuppressRolloverVisuals` (if suppression is confirmed
      to kill the perk data) or look elsewhere (if it doesn't) — no more theories before the log is read.
- [ ] Confirm **Manage returns** AND the vanilla "E) …" prompt **stays hidden**

## 🟢 SPECIAL menu re-skin (skin-over-vanilla — Step 1 done & verified)

- [ ] Provide the `[SPECIAL SWF]` dump lines (open the REGISTRATION FORM once) → locate **POINTS** + **SELECTED row**
- [ ] Wire points + selection into the mirror; verify **all 5 fields** match vanilla
- [ ] Suppress the vanilla SWF + move input over (arrows → `SetActorValue`; Accept/Reset → vanilla's own `TryAccept`/`ConfirmReset`, not reimplemented)
- [ ] Name field: **review FWKeyboard reusability** before building anything new
- [ ] _(Later, separate task)_ Pip-Boy SPECIAL view — out of scope for the intro version

## 🔵 Item-detail cards — absorb from HUDPlusPlus (permission granted; C++ data reads only, our own UI)

One scoped pass each, in rough value order. PrismaHUD's quick-loot already shows name/count/value/weight/DR/damage/range + carry bar.

- [ ] **Value/Wt.** (one division — trivial)
- [ ] **Special icons** — quest item / skill book / book-already-read (the one thing that actually disappears with HUDPlusPlus gone)
- [ ] **Weapon detail** — ammo type, fire rate, accuracy
- [ ] **Fusion core charge** (verify the charge source first)
- [ ] **Junk scrap components** (+ tagged-for-search icon)

## ⚪ Cleanup / housekeeping

- [ ] Remove temporary diagnostics once their questions are answered: `[SPECIAL SWF]` movie dump, bench "no perk button" DIAG log
- [ ] Rename `PrismaWheelMenuTest` → `PrismaWheelMenu` (the "Test" name is misleading for a shipped plugin) — _separate plugin, optional_

---

## ✅ Completed this session (for reference)

- [x] All vanilla notifications render as Prisma toasts (burst-safe; replaces NotificationFilter)
- [x] Vanilla activate button (HUDRollover) suppression path fixed — but see Manage caveat above
- [x] Message box: `$`-token localization resolved (Confirm character / OK / Cancel)
- [x] Message box: grey-panel-behind-dialog fixed (self-correcting vanilla-movie hide)
- [x] Message box: dropped bespoke gamepad nav (framework supersedes it; was a double-fire bug)
- [x] Message box: body glyphs → neutral keycaps on keyboard (engine `bGamepadEnable` limitation)
- [x] Compass reads real map markers (`currentMapMarkers`, all markers, grave included) + perf fix
- [x] Framework: controllers drive Prisma views (gamepad → synthetic keys into CEF)
- [x] Framework: device tracking on the gameplay chain, buttons-only (no stick-drift false positives)
- [x] Crash fixes: one shared trampoline per DLL; wheel-menu instance-extra null guard
- [x] Perf: killed per-poll `info` logging + the CEF texture probe (the real hitch)
