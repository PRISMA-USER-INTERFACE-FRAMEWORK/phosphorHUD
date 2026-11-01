# PrismaHUD open items

> Imported into this repo on 2026-08-07 from the workspace knowledge base
> (`Prisma/findings/open-items.md`). Extracted: 4 of 67 entries concern
> PrismaHUD; the rest belong to other Prisma plugins and were deliberately left out.
>
> NOTE: the live backlog is this repo's GitHub issue tracker. This file is the workspace-side
> roll-up and can lag it.

---

  [OPEN][LOW] 2026-08-07 — PrismaHUD: the #19 off-screen-widget extent clamp (ClampExtent /
          RecoverOffscreen in HUDLayoutManager) and the #12 s_diag ammo-logger removal are committed
          (5936755) but UNBUILT — written on a machine with no MSVC toolchain. Needs an xmake build on
          Windows, a deploy, and an in-game check that a large-scale edge widget is pulled back into
          view (watch for the "was off-screen ... pulled back to" warning in PrismaHUD_F4.log).
          See findings/prismahud.md.

  [NOTE] 2026-08-07 — PrismaHUD's open backlog now lives on GitHub:
          github.com/PRISMA-USER-INTERFACE-FRAMEWORK/PrismaHUD/issues (21 open, filed from CHECKLIST.md
          + the plugin's docs/STATUS.md). This file stays the cross-plugin roll-up; PrismaHUD items are
          tracked by issue number there.

  [OPEN][LOW] 2026-07-18 — PrismaHUD: every widget hardcodes a CSS pixel box; only the anchor
          position scales with resolution, never the widget's own size, no device-scale-factor
          anywhere (bd f4se_og-0pv). 4K renders too small, 1366x768 too large, ultrawide skews
          anchor X/Y disproportionately. Needs a real DPI/resolution-scaling design.

  [FIXED] 2026-07-13 pt9 — 25 of 47 buildable projects had NO version control (untracked AND not
          gitignored), as did 285 of 354 docs markdown files — including 25 of the 29 findings-KB
          files, i.e. the "master brain" had no backup. All now committed (670 files / 90 MB,
          audited: no DLLs/symbol files/objs/build/node_modules). Tools/FO4AnimForge/Mods/ gitignored
          (226 MB of third-party Blender rigs). brain/build_brain.py now reports per-project VCS
          state and docs coverage, so this cannot silently regress.
          NOTE: 13 projects live in NESTED repos (CommonLibF4, PrismaUI_F4 New Gen, PrismaHUD_F4,
          FalloutChat, F4MP-Revival, ...) — root `git status` NEVER shows their changes.
          brain/DRIFT.md lists each with its uncommitted count. See findings/environment-setup.md.
