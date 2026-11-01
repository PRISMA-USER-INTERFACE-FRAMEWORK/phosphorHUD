from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / "src/QuickLootController.cpp").read_text(encoding="utf-8")
view = (ROOT / "assets/views/hud_quickloot.js").read_text(encoding="utf-8")

checks = {
    "caps bands are named value tiers in C++": "QLValueTier" in source and "QLRarity" not in source,
    "payload uses valueTier": '{"valueTier",' in source,
    "legacy rarity payload key is gone": '{"rarity",' not in source,
    "view reads valueTier": "item.valueTier" in view and "item.rarity" not in view,
    "value-tier fallback remains common": "item.valueTier || 'common'" in view,
    "legendary remains independent": '{"legendary",' in source and "item.legendary" in view,
}

failed = [name for name, ok in checks.items() if not ok]
if failed:
    for name in failed:
        print(f"FAIL: {name}")
    raise SystemExit(1)

print("value_tier_semantics_contract: PASS")
