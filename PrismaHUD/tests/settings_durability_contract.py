from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CPP = (ROOT / "src" / "HUDLayoutManager.cpp").read_text(encoding="utf-8")


def code_only(text: str) -> str:
    """Strip // comments so prose cannot satisfy or fail source invariants."""
    out = []
    for line in text.splitlines():
        if line.strip().startswith("//"):
            continue
        out.append(line.split("//", 1)[0] if "//" in line else line)
    return "\n".join(out)


CODE = code_only(CPP)

save = CODE.split("void HUDLayoutManager::SaveSettings() const", 1)[1].split(
    "void HUDLayoutManager::RecomputeMaster()", 1
)[0]

assert "static std::mutex s_settingsFileMx;" in save
assert "std::scoped_lock fileLock(s_settingsFileMx);" in save
assert save.index("fileLock(s_settingsFileMx)") < save.index("std::shared_lock lock(m_lock)"), (
    "settings writer must serialize before snapshotting so an older snapshot cannot overwrite newer state"
)
assert 'tmp += ".tmp";' in save
assert "std::ios::binary | std::ios::trunc" in save
assert "f.flush();" in save
assert "std::filesystem::rename(tmp, path, ec);" in save
assert "std::filesystem::remove(path" in save, "Windows replace retry must remove the existing destination"
assert "catch (...)" in save, "detached settings writer must never let an exception terminate the process"

toggle = CODE.split("void HUDLayoutManager::ToggleMasterHidden()", 1)[1].split(
    "void HUDLayoutManager::SetCombatOnly", 1
)[0]
assert "m_manualHidden = !m_manualHidden" in toggle
assert "SaveSettings" not in toggle
assert "std::thread" not in toggle

combat = CODE.split("void HUDLayoutManager::SetCombatOnly", 1)[1].split(
    "void HUDLayoutManager::SetHideKey", 1
)[0]
hide_key = CODE.split("void HUDLayoutManager::SetHideKey", 1)[1].split(
    "void HUDLayoutManager::UpdateCombatState", 1
)[0]
for body in (combat, hide_key):
    assert "std::thread([this]() { SaveSettings(); }).detach();" in body

print("PrismaHUD settings durability contract passed")
