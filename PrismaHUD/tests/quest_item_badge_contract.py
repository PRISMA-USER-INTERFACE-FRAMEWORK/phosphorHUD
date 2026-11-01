from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / "src/QuickLootController.cpp").read_text(encoding="utf-8")
view = (ROOT / "assets/views/hud_quickloot.js").read_text(encoding="utf-8")
css = (ROOT / "assets/views/hud_quickloot.css").read_text(encoding="utf-8")

checks = {
    "quest state uses engine inventory API": "item.IsQuestObject(stackId)" in source,
    "quest state is evaluated per stack": "std::uint32_t stackId" in source,
    "aggregated row preserves any quest stack": "questItem = true" in source,
    "quest state is sent to the view": '{"questItem", questItem}' in source,
    "view renders quest state": "item.questItem" in view and "quest-badge" in view,
    "quest badge is identified for hover/accessibility": 'title="quest item"' in view,
    "quest badge has dedicated styling": ".quest-badge" in css,
}

failed = [name for name, ok in checks.items() if not ok]
if failed:
    for name in failed:
        print(f"FAIL: {name}")
    raise SystemExit(1)

print("quest_item_badge_contract: PASS")
