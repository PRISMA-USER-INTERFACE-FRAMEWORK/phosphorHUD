import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = (ROOT / "src" / "HUDLayoutManager.h").read_text(encoding="utf-8")
CPP = (ROOT / "src" / "HUDLayoutManager.cpp").read_text(encoding="utf-8")
MAIN = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")


def code_only(text):
    """Source lines with // comments stripped.

    A plain substring check treats commented-out code as present: `// ApplyToView(we);` still
    contains "ApplyToView(we);", so disabling the call passes the guard. Match code, not prose.
    """
    out = []
    for line in text.splitlines():
        stripped = line.strip()
        if stripped.startswith("//"):
            continue
        out.append(line.split("//", 1)[0] if "//" in line else line)
    return "\n".join(out)

assert "std::string id;" in HEADER
assert "std::string displayName;" in HEADER
assert "PRISMA_HUD_API::WidgetDesc desc;" not in HEADER

register = code_only(CPP).split("void HUDLayoutManager::RegisterWidget", 1)[1].split(
    "void HUDLayoutManager::UnregisterWidget", 1
)[0]
assert re.search(r"(?:!view|view == 0)\s*\|\|\s*!desc\.id\s*\|\|\s*desc\.id\[0\] == '\\0'", register), \
    'RegisterWidget must reject a zero view and a null/empty id at the ABI boundary'
assert "e.id = id;" in register and "e.displayName = displayName;" in register
assert "ApplyToView(we);" in register, "RegisterWidget must push saved-or-default layout"
assert re.search(r"ApplyEffectiveVisibility\((?:we\.)?id, true\);", register), (
    "late third-party registration must honor loaded hide gates without flashing unknown auto-hide state"
)

load_apply = code_only(CPP).split("void HUDLayoutManager::LoadAndApply()", 1)[1].split(
    "void HUDLayoutManager::LoadLayoutFile()", 1
)[0]
assert "if (auto it = m_savedPos.find(entry.id); it != m_savedPos.end())" in load_apply
assert "ApplyToView(entry);" in load_apply
assert re.search(r"ApplyEffectiveVisibility\(entry\.id(?:, true)?\);", load_apply)
assert "if (it == m_savedPos.end()) continue;" not in load_apply, (
    "first-install/default-position widgets must still receive layout and visibility"
)

visibility = code_only(CPP).split("void HUDLayoutManager::ApplyEffectiveVisibility", 1)[1].split(
    "void HUDLayoutManager::SetWidgetVisible", 1
)[0]
assert "preserveIfRuntimeUnknown" in visibility
assert "runtimeKnown" in visibility
assert "if (show && preserveIfRuntimeUnknown && !runtimeKnown)" in visibility

assert "if (!g_quickLootView || !g_api->IsValid(g_quickLootView))" in MAIN
assert "g_quickLootView = MakeView(\"hud_quickloot.html\", OnQuickLootDomReady, true);" in MAIN

assert ".desc." not in CPP

print("PrismaHUD widget registration contract guard passed")
