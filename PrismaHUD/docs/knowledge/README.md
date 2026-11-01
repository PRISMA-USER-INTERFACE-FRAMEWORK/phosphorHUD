# Knowledge base

Imported into this repo on **2026-08-07** from the workspace-level knowledge base that previously
lived outside version control. Everything here is reference material: root-cause history, verified
engine facts, and the reasoning behind non-obvious code.

Until this import, these documents existed only on one machine. Anyone cloning the repo got the
source and nothing explaining *why* it looks the way it does.

## Contents

| File | What it is | Scope |
|------|-----------|-------|
| [findings-prismahud.md](findings-prismahud.md) | Day-to-day bug history with full root-cause writeups. The authoritative record of what broke, why, and what the fix actually was. | PrismaHUD |
| [code-notes.md](code-notes.md) | Reasoning behind non-obvious code, anchored to file + symbol. Source files stay clean; explanation lives here. | PrismaHUD |
| [changelog.md](changelog.md) | Dated record of what was built or fixed. | PrismaHUD (extracted) |
| [open-items.md](open-items.md) | Workspace-side roll-up of open issues. | PrismaHUD (extracted) |
| [commonlibf4-findings.md](commonlibf4-findings.md) | Confirmed struct layouts, field offsets and API availability for CommonLibF4. | Shared engine reference |
| [repeat-issues.md](repeat-issues.md) | Failure patterns that recurred across two or more Prisma plugins. | Cross-plugin |

## What was and wasn't imported

The workspace knowledge base covers every Prisma plugin, not just this one. Files that are
PrismaHUD-specific were copied whole. Two are cross-plugin roll-ups and were **extracted** rather
than copied, so this repo does not carry unrelated projects' bug history:

- `changelog.md` — 79 of 412 workspace entries mention PrismaHUD.
- `open-items.md` — 4 of 67 entries concern PrismaHUD.

`commonlibf4-findings.md` and `repeat-issues.md` are cross-plugin but were kept whole: the first
documents engine facts this plugin's code directly depends on, and the second is a list of lessons
that apply here regardless of which plugin first hit them.

## Caveats

- **The live backlog is this repo's [GitHub issue tracker](https://github.com/PRISMA-USER-INTERFACE-FRAMEWORK/PrismaHUD/issues), not `open-items.md`.**
  That file is the workspace-side view and can lag.
- These are **snapshots**. The workspace copies remain the working originals for now, so the two can
  drift. Whichever becomes authoritative, the other should be deleted rather than left to rot.
- Absolute paths throughout refer to the original Windows workspace layout (`E:\F4SE OG\...`), which
  has since moved. Treat paths as historical context, not as instructions.
- Entries are dated and were accurate when written. Several describe code that has since been
  rewritten; the findings file flags its own corrections inline where a later session disproved an
  earlier conclusion.
