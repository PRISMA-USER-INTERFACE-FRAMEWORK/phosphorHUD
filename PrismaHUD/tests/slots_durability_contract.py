from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CPP = (ROOT / "src" / "HUDLayoutManager.cpp").read_text(encoding="utf-8")
EDITOR = (ROOT / "assets" / "views" / "hud_editor.js").read_text(encoding="utf-8")

save = CPP.split("void HUDLayoutManager::SaveSlotsFile() const", 1)[1].split(
    "void HUDLayoutManager::LoadSlotsFile()", 1
)[0]
assert "static std::mutex s_slotsFileMx;" in save
assert "std::scoped_lock fileLock(s_slotsFileMx);" in save
assert save.index("fileLock(s_slotsFileMx)") < save.index("std::shared_lock lock(m_lock)"), (
    "slot writers must serialize before snapshotting so old state cannot overwrite newer state"
)
assert 'tmp += ".tmp";' in save
assert "std::ios::binary | std::ios::trunc" in save
assert "f.flush();" in save
assert "std::filesystem::rename(tmp, path, ec);" in save
assert "catch (const std::exception& e)" in save and "catch (...)" in save, (
    "detached slot writers must contain every exception instead of terminating the process"
)

load = CPP.split("void HUDLayoutManager::LoadSlotsFile()", 1)[1]
assert "for (auto& slot : m_slots)" in load
assert "slot.name.clear();" in load
assert "slot.layout.clear();" in load
assert 'm_slots[0].name = "Default";' in load
assert load.index("slot.layout.clear();") < load.index("std::filesystem::exists(path)"), (
    "slot reload must clear old in-memory entries even when slots.json was deleted"
)

save_slot = CPP.split("void HUDLayoutManager::SaveSlot", 1)[1].split("void HUDLayoutManager::RenameSlot", 1)[0]
rename = CPP.split("void HUDLayoutManager::RenameSlot", 1)[1].split("void HUDLayoutManager::SaveSlotsFile", 1)[0]
for body in (save_slot, rename):
    assert "std::thread([this]() { SaveSlotsFile(); }).detach();" in body

assert "m_slots[index].layout = m_savedPos;" in save_slot, (
    "SaveSlot must snapshot the current layout"
)
assert "m_slots[index].name" in rename, "RenameSlot must update the label"
assert ".layout" not in rename and "m_savedPos" not in rename, (
    "RenameSlot must never overwrite or resnapshot the stored slot layout"
)

assert 'title="click to rename"' in EDITOR
assert "window.renameSlot(JSON.stringify({index: idx, name}))" in EDITOR
assert "if (e.key === 'Enter')" in EDITOR and "e.key === 'Escape'" in EDITOR
assert "const onBlur = () => finish(true);" in EDITOR, (
    "slot names must commit through the dedicated rename path without requiring SaveSlot"
)

print("PrismaHUD slots durability contract passed")
