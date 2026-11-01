from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / "src/QuickLootController.cpp").read_text(encoding="utf-8")

label_start = source.index("std::string QuickLootController::EnginePerkButtonLabel()")
button_start = source.index("bool QuickLootController::HasEnginePerkButton()")
next_section = source.index("bool QuickLootController::ShouldSuppressContainer() const", button_start)
label_body = source[label_start:button_start]
button_body = source[button_start:next_section]

checks = {
    "perk label requires a latched current target": "CurrentContainer()" in label_body,
    "perk label matches cached containerRef to target": "qc.containerRef != expected" in label_body,
    "gamepad perk ownership requires a latched current target": "CurrentContainer()" in button_body,
    "gamepad perk ownership matches cached containerRef to target": "qc.containerRef != expected" in button_body,
    "both paths read Bethesda quick-container cache": "quickContainerMode.eventDataStruct" in label_body
        and "quickContainerMode.eventDataStruct" in button_body,
    "both paths require enabled non-empty engine perk data": "qc.perkButtonEnabled" in label_body
        and "qc.perkButtonText.empty()" in label_body
        and "qc.perkButtonEnabled" in button_body
        and "qc.perkButtonText.empty()" in button_body,
}

failed = [name for name, ok in checks.items() if not ok]
if failed:
    for name in failed:
        print(f"FAIL: {name}")
    raise SystemExit(1)

print("quickcontainer_state_contract: PASS")
