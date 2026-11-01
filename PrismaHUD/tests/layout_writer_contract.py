from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CPP = (ROOT / "src" / "HUDLayoutManager.cpp").read_text(encoding="utf-8")

save_from_js = CPP.split("void HUDLayoutManager::SaveFromJS", 1)[1].split(
    "void HUDLayoutManager::UpdatePositionFromReport", 1
)[0]
assert "SaveLayoutFile();" in save_from_js
assert "std::ofstream" not in save_from_js, (
    "SaveFromJS must not bypass the serialized atomic SaveLayoutFile writer"
)
assert "LayoutPath()" not in save_from_js
assert "snapshot = m_savedPos" not in save_from_js
assert "catch (...)" in save_from_js

save_layout = CPP.split("void HUDLayoutManager::SaveLayoutFile() const", 1)[1].split(
    "void HUDLayoutManager::SaveFromJS", 1
)[0]
assert "static std::mutex s_fileMx;" in save_layout
assert 'tmp += ".tmp";' in save_layout
assert "std::filesystem::rename(tmp, path" in save_layout

print("PrismaHUD layout writer contract passed")
