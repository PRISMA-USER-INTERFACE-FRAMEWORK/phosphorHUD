from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
header = (ROOT / "src/ControlLabels.h").read_text(encoding="utf-8")
source = (ROOT / "src/ControlLabels.cpp").read_text(encoding="utf-8")
quickloot = (ROOT / "src/QuickLootController.cpp").read_text(encoding="utf-8")

checks = {
    "perk is not a hardcoded HUD gamepad default": '{ "perk", "Y" }' not in source
        and '{"perk", "Y"}' not in source,
    "perk mapping uses Fallout dedicated input context": "kQuickContainerMenuPerk" in source,
    "perk mapping reads the live ControlMap singleton": "RE::ControlMap::GetSingleton()" in source,
    "perk mapping reads gamepad device mappings": "deviceMappings[deviceIndex]" in source
        and "RE::INPUT_DEVICE::kGamepad" in source,
    "perk mapping uses the engine mapping inputKey": "mapping.inputKey" in source,
    "gamepad mapping rejects unmapped sentinel values": "0xFFFFFFFFu" in source,
    "gamepad mapping validates through framework canonical resolver": "GetGamepadButtonName" in source,
    "raw gamepad code is normalized for QuickLoot input comparison": "raw | 0x10000u" in source,
    "live perk code is exposed to HUD input ownership": "GamepadCodeForHUDAction" in header
        and 'if (id == "perk") return LiveQuickContainerPerkGamepadCode()' in source,
    "perk prompt is built from the live mapping": 'if (id == "perk") return QuickContainerPerkGamepadPrompt()' in source,
    "live user-event code helper is exposed": "GamepadCodeForUserEvent" in header and source,
    "Activate code uses ControlMap GetMappedKey": "GetMappedKey(userEvent, RE::INPUT_DEVICE::kGamepad" in source,
    "Take code follows live Activate mapping": 'if (id == "take") return GamepadCodeForUserEvent("Activate")' in source,
    "Take prompt follows live Activate mapping": 'if (id == "take") return ForUserEvent("Activate")' in source,
    "QuickLoot input reads the live perk code": 'ControlLabels::GamepadCodeForHUDAction("perk")' in quickloot,
    "QuickLoot input reads the live Take code": 'ControlLabels::GamepadCodeForHUDAction("take")' in quickloot,
    "QuickLoot input identifies the exact perk-owned button": "const bool padEnginePerk" in quickloot
        and "gp == padPerkCode" in quickloot,
    "QuickLoot input does not reserve Y just because a perk exists": "gp == 0x18000u && !padPerkPresent" not in quickloot,
    "QuickLoot gamepad actions yield to the engine perk button": all(token in quickloot for token in (
        "const bool padTake = isPad && !padEnginePerk",
        "const bool padTakeAll    = isPad && !padEnginePerk",
        "const bool padOpenFull   = isPad && !padEnginePerk",
        "const bool padSelUp      = isPad && !padEnginePerk",
        "const bool padSelDown    = isPad && !padEnginePerk",
    )),
}

failed = [name for name, ok in checks.items() if not ok]
if failed:
    for name in failed:
        print(f"FAIL: {name}")
    raise SystemExit(1)

print("quickcontainer_perk_mapping_contract: PASS")
