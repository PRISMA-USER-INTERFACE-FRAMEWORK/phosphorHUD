from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
header = (ROOT / "src/MessageBoxController.h").read_text(encoding="utf-8")
source = (ROOT / "src/MessageBoxController.cpp").read_text(encoding="utf-8")

checks = {
    "controller requires PrismaUI V11": "IVPrismaUI11" in header,
    "raw open is deferred through V11": "DispatchToGameThread(&MessageBoxController::DispatchOpen" in source,
    "raw close is deferred through V11": "DispatchToGameThread(&MessageBoxController::DispatchClose" in source,
    "explicit DOM-ready probe exists": "__PRISMA_MESSAGEBOX_READY__:" in source and "m_domReady" in header,
    "mbTick uses V11 binding": 'BindGameThreadUIEvent(\n        view, "mbTick"' in source,
    "mbAnswer uses V11 binding": 'BindGameThreadUIEvent(\n        view, "mbAnswer"' in source,
    "focus failure rolls back": "if (!api->Focus(view, false, false))" in source and "rolled back to vanilla" in source,
    "vanilla hide happens after successful focus": source.find("if (!api->Focus(view, false, false))") < source.find("SetVanillaVisible(false);"),
    "close race is checked before vanilla hide": source.find("if (!m_openRequested.load())", source.find("if (!api->Focus")) < source.find("SetVanillaVisible(false);"),
}

failed = [name for name, ok in checks.items() if not ok]
if failed:
    for name in failed:
        print(f"FAIL: {name}")
    raise SystemExit(1)

print("messagebox_failsafe_v11_contract: PASS")
