from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CPP = (ROOT / "src" / "SleepWaitMirrorController.cpp").read_text(encoding="utf-8")
HDR = (ROOT / "src" / "SleepWaitMirrorController.h").read_text(encoding="utf-8")
MAIN = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")
HTML = (ROOT / "assets" / "views" / "hud_sleepwait_mirror.html").read_text(encoding="utf-8")

for forbidden in (
    "PassTime(",
    "WaitGameTime(",
    'Invoke("Accept"',
    'Invoke("Cancel"',
    'SuppressVanillaMenu("SleepWaitMenu"',
    'CloseVanillaMenu("SleepWaitMenu"',
):
    assert forbidden not in CPP + MAIN, f"read-only SleepWait mirror crossed action boundary: {forbidden}"

assert "RE::SleepWaitMenu::MENU_NAME" in CPP
assert "ui->GetMenu<RE::SleepWaitMenu>()" in CPP
assert "menu->waitMode" in CPP
assert "menu->isActive" in CPP
assert "menu->appPaused" in CPP

assert '"root1.Menu_mc.iHours"' in CPP
assert '"root1.Menu_mc.iMaxHours"' in CPP
assert "ReadFirstNumeric" in CPP
assert 'nlohmann::json(nullptr)' in CPP

assert "DispatchToGameThread(callback, nullptr)" in CPP
assert "OnMenuOpenVerified" in CPP and "OnMenuCloseVerified" in CPP
assert "m_api->IsGameThread()" in CPP
assert "BindGameThreadUIEvent" in MAIN

assert "readOnly" in CPP
assert "VANILLA MENU STILL CONTROLS ACCEPT / CANCEL" in HTML
assert "pointer-events:none" in HTML
assert "requestSleepWaitMirrorUpdate" in HTML
assert "updateSleepWaitMirror" in HTML

assert "void ResetForLoad();" in HDR
assert "SleepWaitMirrorController::GetSingleton()->ResetForLoad();" in MAIN

assert "void OnDomReady(PrismaView view);" in HDR
assert "bool m_domReady{ false };" in HDR
assert "SleepWaitMirrorController::GetSingleton()->OnDomReady(v);" in MAIN
assert "!m_domReady" in CPP
dom_ready = CPP.split("void SleepWaitMirrorController::OnDomReady(PrismaView view)", 1)[1]
assert "m_domReady = true;" in dom_ready
assert "m_last.clear();" in dom_ready
assert "if (!m_open) return;" in dom_ready
assert "m_api->Show(m_view);" in dom_ready
assert "OnRequestUpdate();" in dom_ready

dom_callback = MAIN.split("static void OnSleepWaitMirrorDomReady(PrismaView v)", 1)[1].split(
    "static void OnMessageBoxDomReady(PrismaView v)", 1
)[0]
binding_failure = dom_callback.index('logger::error("SleepWaitMirror: verified requestUpdate binding failed')
dom_ready_call = dom_callback.index("SleepWaitMirrorController::GetSingleton()->OnDomReady(v);")
assert dom_callback.index("return;", binding_failure) < dom_ready_call

suppressor = MAIN.split("bool IsSuppressingMenuName", 1)[1].split("class MenuSuppressSink", 1)[0]
assert '"SleepWaitMenu"' not in suppressor
assert "SleepWaitMirrorController::GetSingleton()->InstallMenuSink();" in MAIN

print("PrismaHUD SleepWait read-only mirror contract passed")
