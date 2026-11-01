from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CPP = (ROOT / "src" / "HUDLayoutManager.cpp").read_text(encoding="utf-8")
MAIN = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")
API = (ROOT / "src" / "PrismaHUD_F4_API.h").read_text(encoding="utf-8")


def code_only(text):
    """Source lines with // comments stripped -- a substring check counts commented-out code."""
    out = []
    for line in text.splitlines():
        if line.strip().startswith("//"):
            continue
        out.append(line.split("//", 1)[0] if "//" in line else line)
    return "\n".join(out)


load = code_only(CPP).split("void HUDLayoutManager::LoadLayoutFile()", 1)[1].split(
    "void HUDLayoutManager::SaveLayoutFile()", 1
)[0]
assert "m_savedPos.clear();" in load
assert "m_userVisible.clear();" in load
assert load.index("m_savedPos.clear();") < load.index("std::filesystem::exists(path)"), (
    "a deleted layout file must clear old in-memory positions rather than retaining them"
)

api = code_only(MAIN).split('extern "C" DLLEXPORT void* RequestPrismaHUDAPI', 1)[1].split(
    "static void LogJS(PrismaView", 1
)[0]
assert "case PRISMA_HUD_API::InterfaceVersion::V1:" in api
assert "case PRISMA_HUD_API::InterfaceVersion::V2:" in api
assert "case PRISMA_HUD_API::InterfaceVersion::V3:" in api
assert "default:" in api and "return nullptr;" in api, (
    "unsupported future PrismaHUD API versions must not receive a shorter V1 vtable"
)
assert api.index("default:") > api.index("InterfaceVersion::V1"), (
    "V1 must be an explicit supported case, not share the unsupported-version default"
)

assert '{"x":0.0156,"y":0.8167,"s":1.0,"r":0.0}' in API
assert '"w":400' not in API and '"h":300' not in API, (
    "the copied public header must document the actual x/y/scale/rotation payload"
)

print("PrismaHUD reload/API contract guard passed")
