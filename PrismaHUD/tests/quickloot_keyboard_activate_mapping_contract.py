from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
header = (ROOT / "src/ControlLabels.h").read_text(encoding="utf-8")
labels = (ROOT / "src/ControlLabels.cpp").read_text(encoding="utf-8")
quickloot = (ROOT / "src/QuickLootController.cpp").read_text(encoding="utf-8")

checks = {
    "keyboard user-event code helper exists": "KeyboardCodeForUserEvent" in header and "KeyboardCodeForUserEvent" in labels,
    "keyboard Activate reads MainGameplay ControlMap": 'GetMappedKey(userEvent, RE::INPUT_DEVICE::kKeyboard' in labels
        and "kMainGameplay" in labels,
    "Take prompt follows Fallout Activate on every active device": 'if (id == "take") return ForUserEvent("Activate")' in labels,
    "QuickLoot input reads live keyboard Take code": 'ControlLabels::KeyboardCodeForUserEvent("Activate")' in quickloot,
    "mapped keyboard code is compared to ButtonEvent idCode": "ev->idCode == keyboardTakeCode" in quickloot,
    "raw E is fallback only when live mapping is unavailable": "keyboardTakeCode == 0u && ev->idCode == 0x45" in quickloot,
    "user-event Activate fallback remains available": 'ev->QUserEvent() == "Activate"' in quickloot,
    "keyboard footer does not unconditionally fall back to E": 'if (takeKey.empty())     takeKey     = "E";' not in quickloot,
}

failed = [name for name, ok in checks.items() if not ok]
if failed:
    for name in failed:
        print(f"FAIL: {name}")
    raise SystemExit(1)

print("quickloot_keyboard_activate_mapping_contract: PASS")
