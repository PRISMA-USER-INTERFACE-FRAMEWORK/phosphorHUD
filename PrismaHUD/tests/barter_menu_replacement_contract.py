from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / "src/BarterMenuController.cpp").read_text(encoding="utf-8")
header = (ROOT / "src/BarterMenuController.h").read_text(encoding="utf-8")
html = (ROOT / "assets/views/hud_barter.html").read_text(encoding="utf-8")
suite = (ROOT / "src/MenuReplacementSuite.cpp").read_text(encoding="utf-8")

checks = {
    "HUD suite owns barter": "BarterMenuController" in suite,
    "no grid namespace in controller": "PrismaInventory" not in source and "PrismaInventory" not in header,
    "no grid project reference": "PrismaInventory_F4" not in source and "PrismaInventory_F4" not in header,
    "V11 controller API": "IVPrismaUI11" in header,
    "open and close are V11 deferred": "DispatchToGameThread" in source and "DispatchOpen" in source and "DispatchClose" in source,
    "HUD barter view": 'CreateView("hud_barter.html"' in source,
    "V11 barter action": 'view, "barterAction"' in source,
    "close recovery tick": '"barterTick"' in source and "barterTick" in html,
    "typed Bethesda transfer": "menu->DoItemTransfer" in source,
    "native reset": "menu->ClearTradingData();" in source,
    "native accept": "menu->CompleteTrade();" in source,
    "stale handle validation": "liveEntry.invHandle.id != expectedHandle" in source,
    "stale stack validation": "StackKey(liveEntry) != expectedStackKey" in source,
    "live quantity validation": "ReadVanillaQuantity(*menu" in source,
    "unknown quantity only permits one": "else if (requestedCount != 1)" in source,
    "focus is checked": "if (!m_api->FocusOverlay(m_view, false, false))" in source,
    "vanilla hidden after focus": source.find("if (!m_api->FocusOverlay") < source.find("uiMovie->SetVisible(false)"),
    "close race before vanilla hide": source.find("if (!m_openRequested.load() || !menu)") < source.find("uiMovie->SetVisible(false)"),
    "category metadata": '"filterFlag"' in source and '"category"' in source,
    "native category queue": "QueueCategory" in source and "ResolveStableBarterRow" in source,
    "bulk re-resolves each mutation": "ResolveStableBarterRow(*menu" in source,
    "bulk sell skips favorites": "!fromContainer && display.isFavorite" in source,
    "UI category tabs": "var cats=[[\'ALL\'" in html,
    "UI category bulk actions": "bulk|player|" in html and "bulk|vendor|" in html,
}

failed = [name for name, ok in checks.items() if not ok]
if failed:
    for name in failed:
        print(f"FAIL: {name}")
    raise SystemExit(1)

print("barter_menu_replacement_contract: PASS")
