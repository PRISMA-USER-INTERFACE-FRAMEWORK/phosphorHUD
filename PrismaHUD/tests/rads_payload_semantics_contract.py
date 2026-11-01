import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / "src/HUDDataProvider.cpp").read_text(encoding="utf-8")
view = (ROOT / "assets/views/hud_rads.js").read_text(encoding="utf-8")

checks = {
    "radiation dose keeps Fallout 1000 scale": len(re.findall(r'\{"radsMax",\s*1000\.f\}', source)) >= 2,
    "rads view consumes dose scale": "_rads / _radsMax" in view,
    "rad health ceiling is named accurately": '"radHealthMax"' in source,
    "legacy hpLost payload name is gone": '"hpLost"' not in source,
    "radHealthMax is not used as the rads denominator": "radHealthMax" not in view,
}

failed = [name for name, ok in checks.items() if not ok]
if failed:
    for name in failed:
        print(f"FAIL: {name}")
    raise SystemExit(1)

print("rads_payload_semantics_contract: PASS")
