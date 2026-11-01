from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / "src/HUDIconCatalog.cpp").read_text(encoding="utf-8")
header = (ROOT / "src/HUDIconCatalog.h").read_text(encoding="utf-8")
manager = (ROOT / "src/HUDLayoutManager.cpp").read_text(encoding="utf-8")
editor = (ROOT / "assets/views/hud_editor.js").read_text(encoding="utf-8")
editor_css = (ROOT / "assets/views/hud_editor.css").read_text(encoding="utf-8")
cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
release_spec = (ROOT / "release.spec").read_text(encoding="utf-8")
manifest = (ROOT / "assets/icons/icons.json").read_text(encoding="utf-8")

save_from_js = manager[
    manager.index("void HUDLayoutManager::SaveFromJS"):
    manager.index("void HUDLayoutManager::UpdatePositionFromReport")
]
apply_preset = manager[
    manager.index("void HUDLayoutManager::ApplyPreset"):
    manager.index("void HUDLayoutManager::LoadSlot")
]

checks = {
    "runtime catalog interface exists": "std::vector<Choice> Enumerate()" in header,
    "deployed icon directory is source of truth": '"Data" / "PrismaUI_F4" / "views" / "icons"' in source,
    "folder is scanned": "std::filesystem::directory_iterator(root)" in source,
    "only png/svg accepted": 'ext == ".png" || ext == ".svg"' in source,
    "path separators rejected": "file.find('/')" in source and "file.find('\\\\')" in source,
    "ADS colon rejected": "file.find(':')" in source,
    "subpaths rejected": "p.has_parent_path()" in source,
    "windows reserved device names rejected": "ReservedDosDeviceName" in source
        and 'base == "con"' in source
        and 'base == "prn"' in source
        and 'base == "aux"' in source
        and 'base == "nul"' in source
        and 'prefix == "com"' in source
        and 'prefix == "lpt"' in source,
    "trailing dot or space rejected": "file.back() == ' ' || file.back() == '.'" in source,
    "control characters rejected": "if (c < 0x20) return false" in source,
    "file must exist at selection time": "std::filesystem::is_regular_file(candidate)" in source,
    "catalog has bounded size": "kMaxIcons" in source,
    "manifest is optional labels only": 'root / "icons.json"' in source and 'doc.contains("icons")' in source,
    "default icon remains explicit": 'out.push_back({ "", "Default" })' in source,
    "shipped manifest exists": '"icons"' in manifest,
    "manager enumerates runtime catalog": "prismhud::icons::Enumerate()" in manager,
    "manager validates explicit selections": "prismhud::icons::IsAllowed(want)" in manager,
    "manager validates saved selections": "prismhud::icons::IsAllowed(saved)" in manager,
    "fixed icon table is retired": "HUDLayoutManager::kIcons" not in manager,
    "normal save preserves widget metadata": "m_savedPos[id] = pos;" not in save_from_js
        and 'saved["x"] = pos["x"]' in save_from_js
        and 'saved["r"] = pos["r"]' in save_from_js,
    "preset preserves widget metadata": "saved = nlohmann::json" not in apply_preset
        and 'saved["x"] = entry.x' in apply_preset
        and 'saved["r"] = entry.r' in apply_preset,
    "editor save includes rotation": "s: w.s, r: w.r || 0" in editor,
    "large catalog picker is independently scrollable": ".icon-pick" in editor_css
        and "max-height:116px" in editor_css
        and "overflow-y:auto" in editor_css
        and "overscroll-behavior:contain" in editor_css,
    "icon buttons keep a fixed grid footprint": "flex:0 0 26px" in editor_css,
    "cmake packages icons beside views": 'Data/PrismaUI_F4/views/icons' in cmake,
    "release package agrees with runtime path": "assets/icons => PrismaUI_F4/views/icons" in release_spec,
}

failed = [name for name, ok in checks.items() if not ok]
if failed:
    for name in failed:
        print(f"FAIL: {name}")
    raise SystemExit(1)

print("icon_catalog_contract: PASS")
