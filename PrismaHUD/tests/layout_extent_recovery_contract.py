from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / "src/HUDLayoutManager.cpp").read_text(encoding="utf-8")

checks = {
    "extent clamp exists": "static void ClampExtent" in source,
    "extent clamp keeps a visible fraction": "constexpr float kMinVisible = 0.25f" in source
        and "const float keep = extent * kMinVisible" in source,
    "scaled width participates in clamp": "clampAxis(x, w * s);" in source,
    "scaled height participates in clamp": "clampAxis(y, h * s);" in source,
    "scale is bounded before saved layout acceptance": "if (!(s > 0.19f) || s > 3.01f)" in source,
    "rotated widgets swap intrinsic extents": "const bool quarter = (e.r == 90.f || e.r == 270.f);" in source
        and "quarter ? e.defaultH : e.defaultW" in source
        and "quarter ? e.defaultW : e.defaultH" in source,
    "saved positions recover when a widget registers": "RecoverOffscreen(we, p);" in source,
    "loaded positions recover before application": "RecoverOffscreen(entry, p);" in source,
    "imported positions recover before application": "RecoverOffscreen(entry, m_savedPos[id]);" in source,
    "render path clamps projected extents": "ClampExtent(x, y, e.s, ew, eh);" in source,
}

failed = [name for name, ok in checks.items() if not ok]
if failed:
    for name in failed:
        print(f"FAIL: {name}")
    raise SystemExit(1)

print("layout_extent_recovery_contract: PASS")
