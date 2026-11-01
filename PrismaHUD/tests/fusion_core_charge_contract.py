from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
header = (ROOT / "src/QuickLootBatteryInfo.h").read_text(encoding="utf-8")
source = (ROOT / "src/QuickLootBatteryInfo.cpp").read_text(encoding="utf-8")
controller = (ROOT / "src/QuickLootController.cpp").read_text(encoding="utf-8")
view = (ROOT / "assets/views/hud_quickloot.js").read_text(encoding="utf-8")

checks = {
    "backend has explicit battery result state": "isPowerArmorBattery" in header,
    "backend distinguishes known charge from unknown charge": "hasChargePercent" in header,
    "backend distinguishes mixed-charge aggregate rows": "mixedCharge" in header,
    "battery identity uses Bethesda PowerArmor predicate": "RE::PowerArmor::IsPowerArmorBattery(object)" in source,
    "full capacity uses Bethesda fNewBatteryCapacity accessor": "RE::PowerArmor::GetNewBatteryCapacity()" in source,
    "per-stack charge uses engine extra-data health accessor": "extra->GetHealthPerc()" in source,
    "missing stack extra data falls back to full charge": "float charge = 1.0f" in source,
    "charge is clamped to a valid engine fraction": "std::clamp(charge, 0.0f, 1.0f)" in source,
    "mixed stack charge refuses a fake aggregate percentage": "std::fabs(charge - firstCharge) > 0.001f" in source
        and "result.mixedCharge = true" in source
        and "result.hasChargePercent = false" in source,
    "display percentage is derived from engine fraction": "result.chargePercent = firstCharge * 100.0f" in source,
    "inspection is fail-closed for presentation": "catch (...)" in source,
    "quick-loot snapshot calls the typed inspector": "prismhud::quickloot::InspectBattery(obj, item)" in controller,
    "quick-loot payload exposes battery identity": 'extras["powerArmorBattery"] = true' in controller,
    "quick-loot payload exposes mixed state": 'extras["batteryChargeMixed"] = battery.mixedCharge' in controller,
    "quick-loot payload exposes charge only when known": "if (battery.hasChargePercent)" in controller
        and 'extras["batteryChargePercent"] = battery.chargePercent' in controller,
    "quick-loot view renders a concrete charge percent": "CHARGE ${Math.round" in view
        and "batteryChargePercent" in view,
    "quick-loot view renders mixed charge explicitly": "CHARGE MIXED" in view
        and "batteryChargeMixed" in view,
    "editor preview exercises fusion-core charge rendering": "name: 'Fusion Core'" in view
        and "batteryChargePercent: 63" in view,
}

failed = [name for name, ok in checks.items() if not ok]
if failed:
    for name in failed:
        print(f"FAIL: {name}")
    raise SystemExit(1)

print("fusion_core_charge_contract: PASS")
