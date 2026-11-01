from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / "src/SleepWaitMenuController.cpp").read_text(encoding="utf-8")
header = (ROOT / "src/SleepWaitMenuController.h").read_text(encoding="utf-8")
html = (ROOT / "assets/views/hud_sleepwait.html").read_text(encoding="utf-8")
suite = (ROOT / "src/MenuReplacementSuite.cpp").read_text(encoding="utf-8")

checks = {
    "HUD suite owns SleepWait": "SleepWaitMenuController" in suite,
    "requires V11": "IVPrismaUI11" in header,
    "raw callbacks defer": "DispatchToGameThread" in source,
    "typed native state": all(x in source for x in ("menu->waitMode", "menu->isActive", "menu->appPaused", "menu->sleepTimer")),
    "live iHours read": '"root1.Menu_mc.iHours"' in source,
    "live iMaxHours read": '"root1.Menu_mc.iMaxHours"' in source,
    "writes vanilla iHours": "SetVariable(path, value" in source,
    "accept forwarded through vanilla ProcessUserEvent": "ProcessUserEvent" in source and 'InvokeVanillaUserEvent("Accept")' in source,
    "cancel uses engine menu queue": "UIMessageQueue::GetSingleton" in source and "UI_MESSAGE_TYPE::kHide" in source,
    "focus before vanilla hide": source.find("if (!m_api->FocusOverlay") < source.find("menu->uiMovie->SetVisible(false)"),
    "failure restores vanilla": "restoring vanilla menu" in source and "RestoreVanilla();" in source,
    "never advances time directly": all(x not in source for x in ("PassTime", "AdvanceTime", "SetHoursPassed", "SetGameHour")),
    "direct target UI": "data-hour=\"6\"" in html and "data-hour=\"22\"" in html,
    "coarse and fine stepping": 'data-step="-12"' in html and 'data-step="1"' in html,
    "keyboard accept/cancel": "e.key==='Escape'" in html and "e.key==='Enter'" in html,
    "Bethesda backend clearly declared": "BETHESDA SLEEPWAITMENU BACKEND" in html,
}

failed = [name for name, ok in checks.items() if not ok]
if failed:
    for name in failed:
        print(f"FAIL: {name}")
    raise SystemExit(1)

print("sleepwait_menu_contract: PASS")
