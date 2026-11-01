from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
labels = (ROOT / "src/ControlLabels.cpp").read_text(encoding="utf-8")
quickloot = (ROOT / "src/QuickLootController.cpp").read_text(encoding="utf-8")
view = (ROOT / "assets/views/hud_quickloot.js").read_text(encoding="utf-8")
html = (ROOT / "assets/views/hud_quickloot.html").read_text(encoding="utf-8")

checks = {
    "HUD action helper exposes live Take button code": 'GamepadCodeForHUDAction("take")' in quickloot,
    "HUD action helper exposes Take All button code": 'GamepadCodeForHUDAction("take_all")' in quickloot,
    "HUD action helper exposes transfer button code": 'GamepadCodeForHUDAction("transfer")' in quickloot,
    "HUD action helper exposes selection direction codes": all(token in quickloot for token in (
        'GamepadCodeForHUDAction("select_up")',
        'GamepadCodeForHUDAction("select_down")',
    )),
    "HUD action helper exposes bench transfer button code": 'GamepadCodeForHUDAction("bench_transfer")' in quickloot,
    "default transfer code is normalized Y": '"transfer"' in labels and "0x18000u" in labels,
    "default bench transfer code is normalized X": '"bench_transfer"' in labels and "0x14000u" in labels,
    "footer clears Take only on exact perk collision": 'clearIfCollision(takeKey, "take")' in quickloot,
    "footer clears Take All only on exact perk collision": 'clearIfCollision(takeAllKey, "take_all")' in quickloot,
    "footer clears Transfer only on exact perk collision": 'clearIfCollision(transferKey, "transfer")' in quickloot,
    "footer clears selection when either D-pad direction collides":
        "padPerkCode == selectUpCode || padPerkCode == selectDownCode" in quickloot,
    "collision helper compares exact normalized codes": "padPerkCode == actionCode" in quickloot,
    "collidable footer hints have stable elements": all(token in html for token in (
        'id="takeHint"', 'id="takeAllHint"', 'id="transferHint"', 'id="selectHint"',
    )),
    "view clears stale keycap markup when an action yields":
        "cap.innerHTML = key ? CG.renderKey(key, ps) : ''" in view,
    "view hides the whole yielded action hint": "hint.style.display = key ? '' : 'none'" in view,
    "Take TakeAll Transfer and Select all use collision-aware renderer": all(token in view for token in (
        "renderFooterAction(takeHint, takeCap, d.takeKey, _ps)",
        "renderFooterAction(takeAllHint, takeAllCap, d.takeAllKey, _ps)",
        "renderFooterAction(transferHint, transferCap, d.transferKey, _ps)",
        "renderFooterAction(selectHint, selectCap, d.selectKey, _ps)",
    )),
}

failed = [name for name, ok in checks.items() if not ok]
if failed:
    for name in failed:
        print(f"FAIL: {name}")
    raise SystemExit(1)

print("quickcontainer_perk_prompt_collision_contract: PASS")
