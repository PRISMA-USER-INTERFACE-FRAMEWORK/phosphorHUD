from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / "src/HUDDataProvider.cpp").read_text(encoding="utf-8")

checks = {
    "temporary ammo diagnostic counter removed": "static int s_diag" not in source,
    "temporary ammo diagnostic symbol removed": "s_diag" not in source,
}

failed = [name for name, ok in checks.items() if not ok]
if failed:
    for name in failed:
        print(f"FAIL: {name}")
    raise SystemExit(1)

print("ammo_diag_noise_contract: PASS")
