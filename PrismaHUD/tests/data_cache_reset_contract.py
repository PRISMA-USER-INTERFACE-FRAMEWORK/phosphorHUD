from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
H = (ROOT / "src" / "HUDDataProvider.h").read_text(encoding="utf-8")
CPP = (ROOT / "src" / "HUDDataProvider.cpp").read_text(encoding="utf-8")
MAIN = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")

assert "void ResetTransientForLoad();" in H
assert "m_lastPayload" in H and "m_lastPrompt" in H
set_views = H.split("void SetViews(BuiltinViews views)", 1)[1].split(
    "void ResetTransientForLoad();", 1
)[0]
assert "m_lastPayload.clear();" in set_views
assert "m_lastPrompt.clear();" in set_views

bind = CPP.split("#define BIND", 1)[1].split("#undef BIND", 1)[0]
assert "static std::string s_last" not in bind
assert "s->m_lastPayload[s->m_views.ViewField]" in bind

assert "g_lastPrompt" not in CPP
assert "m_lastPrompt" in CPP

reset = CPP.split("void HUDDataProvider::ResetTransientForLoad()", 1)[1].split(
    "std::recursive_mutex& HUDDataProvider::DataLock", 1
)[0]
for token in ("m_lastPayload.clear();", "m_lastPrompt.clear();", "ResetDataBuilderCaches();"):
    assert token in reset

assert "struct CompassCache" in CPP
assert "CompassCache g_compassCache" in CPP
compass = CPP.split("static std::string BuildCompassJSON()", 1)[1].split(
    "static std::string BuildRadsJSON()", 1
)[0]
for old in ("static int            s_tick", "static std::string    s_location", "s_markersSent"):
    assert old not in compass
assert "auto& cache = g_compassCache;" in compass
assert "cache.markersSent" in compass

verified = MAIN.split("static void HandleLoadedGameOnVerifiedThread()", 1)[1].split(
    "static void RunVerifiedLoadInit", 1
)[0]
assert "HUDDataProvider::GetSingleton()->ResetTransientForLoad();" in verified

assert "BindUIEvent does NOT serialize onto one thread" not in H

print("PrismaHUD data cache reset contract passed")
