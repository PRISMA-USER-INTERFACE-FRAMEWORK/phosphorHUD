#!/usr/bin/env python3
"""Replay Phosphor HUD messages against hud.html, with no game running.

Checks, in order:
  1. every line is valid JSON and matches schema/hud-messages.schema.json (if `jsonschema` is installed)
  2. the schema and hud.js agree on the top-level fields (catches spec/HUD drift)
  3. every message runs through hudUpdate() in a headless browser without a JavaScript error

Usage:
  pip install playwright jsonschema && python -m playwright install chromium
  python tools/replay.py                                   # examples/messages.jsonl against mod/.../hud.html
  python tools/replay.py my_log.jsonl --shots out_dir      # replay lines copied from PhosphorHUD.log, screenshot each
  python tools/replay.py --hud path/to/hud.html --delay 400

Exit code 0 when everything passes, 1 otherwise.
"""
import argparse, asyncio, json, pathlib, re, sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
DEF_HUD = ROOT / "mod" / "Phosphor HUD" / "PrismaUI_F4" / "views" / "PhosphorHUD" / "hud.html"
DEF_MSGS = ROOT / "examples" / "messages.jsonl"
SCHEMA = ROOT / "schema" / "hud-messages.schema.json"


def hud_fields(js):
    """Top-level fields hudUpdate() reads, straight from its source."""
    m = re.search(r"function hudUpdate\(data\)\s*\{(.*?)\n  \}\n  window\.hudUpdate", js, re.S)
    if not m:
        sys.exit("could not find hudUpdate() in hud.js")
    body = m.group(1)
    fields = set(re.findall(r"\bd\.([A-Za-z0-9_]+)", body))
    for lst in re.findall(r"\[((?:\"[A-Za-z0-9_]+\",?\s*)+)\]\.forEach", body):
        fields |= set(re.findall(r"\"([A-Za-z0-9_]+)\"", lst))
    return fields


def load_lines(path):
    out = []
    for n, raw in enumerate(pathlib.Path(path).read_text(encoding="utf-8").splitlines(), 1):
        raw = raw.strip()
        if not raw or raw.startswith("//"):
            continue
        # accept raw log lines like `... hudUpdate {"hp":1}` by taking the first {...}
        start = raw.find("{")
        out.append((n, raw[start:] if start >= 0 else raw))
    return out


async def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("messages", nargs="?", default=str(DEF_MSGS))
    ap.add_argument("--hud", default=str(DEF_HUD))
    ap.add_argument("--delay", type=int, default=120, help="ms between messages")
    ap.add_argument("--shots", help="folder to save a screenshot after each message")
    a = ap.parse_args()

    js = pathlib.Path(a.hud).with_name("hud.js").read_text(encoding="utf-8")
    schema = json.loads(SCHEMA.read_text(encoding="utf-8"))
    problems = []

    # 2. drift between the spec's schema and the HUD
    in_hud, in_schema = hud_fields(js), set(schema["properties"])
    for f in sorted(in_hud - in_schema):
        problems.append(f"schema is missing a field hud.html reads: {f}")
    for f in sorted(in_schema - in_hud):
        problems.append(f"schema documents a field hud.html ignores: {f}")

    # 1. JSON + schema
    try:
        import jsonschema
        validator = jsonschema.Draft202012Validator(schema)
    except ImportError:
        validator = None
        print("note: jsonschema not installed, skipping schema validation")
    lines = load_lines(a.messages)
    parsed = []
    for n, raw in lines:
        try:
            obj = json.loads(raw)
        except json.JSONDecodeError as e:
            problems.append(f"line {n}: not JSON ({e})")
            continue
        if validator:
            for err in validator.iter_errors(obj):
                where = "/".join(str(p) for p in err.absolute_path) or "(top)"
                problems.append(f"line {n}: {where}: {err.message}")
        parsed.append((n, raw))

    # 3. replay in the page, in-game mode (file:// with no #preview)
    from playwright.async_api import async_playwright
    async with async_playwright() as p:
        browser = await p.chromium.launch()
        page = await browser.new_page(viewport={"width": 1920, "height": 1080})
        js_errors = []
        page.on("pageerror", lambda e: js_errors.append(str(e)))
        page.on("console", lambda m: js_errors.append(m.text) if m.type == "error" and m.text.startswith("hudUpdate") else None)
        await page.goto(pathlib.Path(a.hud).resolve().as_uri())
        await page.wait_for_timeout(400)
        await page.evaluate("document.documentElement.style.background = '#3d3a30'")
        shots = pathlib.Path(a.shots) if a.shots else None
        if shots:
            shots.mkdir(parents=True, exist_ok=True)
        for i, (n, raw) in enumerate(parsed, 1):
            before = len(js_errors)
            try:
                await page.evaluate("s => window.hudUpdate(s)", raw)
            except Exception as e:  # an exception thrown inside hudUpdate lands here
                js_errors.append(str(e).splitlines()[0])
            await page.wait_for_timeout(a.delay)
            for e in js_errors[before:]:
                problems.append(f"line {n}: JavaScript error: {e}")
            if shots:
                await page.screenshot(path=str(shots / f"{i:03d}_line{n}.png"))
        visible = await page.evaluate("!document.getElementById('stage').classList.contains('awaiting')")
        if not visible and any('"snapshot"' in r or '"hp"' in r for _, r in parsed):
            problems.append("the HUD never became visible (no snapshot/hp message took effect)")
        await browser.close()

    print(f"{len(parsed)} messages replayed against {a.hud}")
    if problems:
        print(f"{len(problems)} problem(s):")
        for pr in problems:
            print("  - " + pr)
        return 1
    print("all good")
    return 0


if __name__ == "__main__":
    sys.exit(asyncio.run(main()))
