from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
REPO = ROOT.parents[1]
SRC = ROOT / "src"

main = (SRC / "main.cpp").read_text(encoding="utf-8")
layout_h = (SRC / "HUDLayoutManager.h").read_text(encoding="utf-8")
layout_cpp = (SRC / "HUDLayoutManager.cpp").read_text(encoding="utf-8")
quick_h = (SRC / "QuickLootController.h").read_text(encoding="utf-8")
quick_cpp = (SRC / "QuickLootController.cpp").read_text(encoding="utf-8")

for name in ("PrismaUI_F4_API.h", "PrismaUI_F4_API_V11.h"):
    local = (SRC / name).read_bytes().replace(b"\r\n", b"\n")
    canonical = (REPO / "framework" / "prismaui-core" / "src" / name).read_bytes().replace(b"\r\n", b"\n")
    assert local == canonical, f"PrismaHUD vendored {name} drifted from the framework provider"

assert '#include "PrismaUI_F4_API_V11.h"' in main
assert "static PRISMA_UI_API::IVPrismaUI11* g_api = nullptr;" in main
assert "RequestPluginAPI<PRISMA_UI_API::IVPrismaUI11>()" in main

handler = main.split("static void F4SEMessageHandler", 1)[1].split(
    'extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Query(', 1
)[0]
post = handler.split("case F4SE::MessagingInterface::kPostLoadGame:", 1)[1]
assert "DispatchToGameThread" in post
for forbidden in ("CreateBuiltinViews();", "LoadAndApply();", "QuickLootInput::Install();"):
    assert forbidden not in post, f"raw F4SE post-load branch still performs {forbidden}"

verified = main.split("static void HandleLoadedGameOnVerifiedThread()", 1)[1].split(
    "static void RunVerifiedLoadInit", 1
)[0]
assert "g_api->IsGameThread()" in verified
assert "ResetTransientForLoad();" in verified
assert "CreateBuiltinViews();" in verified
assert "LoadAndApply();" in verified
assert "QuickLootInput::Install();" in verified
assert "g_openSuppressingMenus.clear();" in verified
assert "SetCaptureMode(false);" in verified

assert "kPreLoadGame" in handler
assert "g_loadGeneration.fetch_add" in handler
assert "discarded stale load initialization generation" in main

assert "void ResetTransientForLoad();" in layout_h
layout_reset = layout_cpp.split("void HUDLayoutManager::ResetTransientForLoad()", 1)[1].split(
    "void HUDLayoutManager::LoadAndApply()", 1
)[0]
for token in (
    "m_runtimeVisible.clear();",
    "m_manualHidden = false;",
    "m_inCombat = false;",
    "m_lastCombatTick = 0;",
    "m_menuSuppressed.store(false",
    "m_masterVisible.store(!m_combatOnly",
):
    assert token in layout_reset, f"HUD load reset lost {token}"

load_apply = layout_cpp.split("void HUDLayoutManager::LoadAndApply()", 1)[1].split(
    "void HUDLayoutManager::LoadLayoutFile()", 1
)[0]
assert "ApplyEffectiveVisibility(entry.id, true);" in load_apply

assert "void ResetTransientForLoad();" in quick_h
quick_reset = quick_cpp.split("void QuickLootController::ResetTransientForLoad()", 1)[1].split(
    "void QuickLootController::ToggleUserHidden()", 1
)[0]
for token in (
    "SetExclusive(false);",
    "m_cachedPick = {};",
    "m_container = {};",
    "m_items.clear();",
    "m_active = false;",
    "m_isWorkshopBench = false;",
    "m_allowFullOpen = false;",
):
    assert token in quick_reset, f"QuickLoot load reset lost {token}"

assert "BindUIEvent dispatches from a pool" not in quick_cpp

print("PrismaHUD V11 load-thread contract passed")
