from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / "src/QuickLootController.cpp").read_text(encoding="utf-8")
view = (ROOT / "assets/views/hud_quickloot.js").read_text(encoding="utf-8")

checks = {
    "CombatFormulas header is used": '#include "RE/C/CombatFormulas.h"' in source,
    "vanilla fire-rate display formula is used": "RE::CombatFormulas::GetWeaponDisplayRateOfFire" in source,
    "vanilla accuracy display formula is used": "RE::CombatFormulas::GetWeaponDisplayAccuracy" in source,
    "accuracy uses a weapon object instance": "RE::BGSObjectInstanceT<RE::TESObjectWEAP>" in source,
    "base-form display stats stay explicit": "weap, nullptr" in source,
    "non-finite engine results are rejected": "std::isfinite" in source,
    "fire rate is sent to the view": 'extras["fireRate"]' in source,
    "accuracy is sent to the view": 'extras["accuracy"]' in source,
    "quickloot renders fire rate": "item.fireRate" in view and "ROF" in view,
    "quickloot renders accuracy": "item.accuracy" in view and "ACC" in view,
}

failed = [name for name, ok in checks.items() if not ok]
if failed:
    for name in failed:
        print(f"FAIL: {name}")
    raise SystemExit(1)

print("weapon_display_stats_contract: PASS")
