from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / "src/ContainerMenuController.cpp").read_text(encoding="utf-8")
header = (ROOT / "src/ContainerMenuController.h").read_text(encoding="utf-8")
html = (ROOT / "assets/views/hud_container.html").read_text(encoding="utf-8")
suite = (ROOT / "src/MenuReplacementSuite.cpp").read_text(encoding="utf-8")

checks = {
    "HUD suite owns container replacement": "ContainerMenuController" in suite,
    "separate from grid project": "PrismaInventory_F4" not in source and "PrismaInventory" not in source,
    "V11 lifecycle": "IVPrismaUI11" in header and "DispatchToGameThread" in source,
    "HUD container view": 'CreateView("hud_container.html"' in source,
    "typed Bethesda transfer": "menu->DoItemTransfer" in source,
    "typed Bethesda take-all": "menu->TakeAllItems();" in source,
    "engine filter flags exposed": "display.filterFlag" in source,
    "stable stack identity": "StackKey(entry)" in source and "ResolveStableRow" in source,
    "bulk preflight": "std::vector<StableTransferRow> plan" in source,
    "bulk re-resolves every mutation": "ResolveStableRow(*menu" in source,
    "safe store skips favorites": "display.isFavorite" in source,
    "safe store skips equipped": "display.equipState != 0" in source,
    "focus checked before vanilla hide": source.find("if (!m_api->FocusOverlay") < source.find("menu->uiMovie->SetVisible(false)"),
    "close race checked before vanilla hide": source.find("if (!m_openRequested.load() || !menu)") < source.find("menu->uiMovie->SetVisible(false)"),
    "UI category bulk transfer": "bulk|player|" in html and "bulk|container|" in html,
    "UI recovery tick": "containerTick" in html and '"containerTick"' in source,
}

failed = [name for name, ok in checks.items() if not ok]
if failed:
    for name in failed:
        print(f"FAIL: {name}")
    raise SystemExit(1)

print("container_menu_replacement_contract: PASS")
