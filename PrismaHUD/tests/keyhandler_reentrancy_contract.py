from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CPP = (ROOT / "src" / "KeyHandler.cpp").read_text(encoding="utf-8")

body = CPP.split("void KeyHandler::OnButtonEvent", 1)[1]
assert "std::vector<KeyCallback> callbacks;" in body
assert "std::shared_lock lock(_mutex);" in body
assert "callbacks.push_back(cb);" in body

lock_pos = body.index("std::shared_lock lock(_mutex);")
invoke_pos = body.index("consume |= cb();")
lock_scope_end = body.index("    }\n\n    bool consume", lock_pos)
assert lock_pos < lock_scope_end < invoke_pos

assert "catch (const std::exception& e)" in body
assert "catch (...)" in body
assert body.index("for (auto& cb : callbacks)") < body.index("catch (...)")

assert "consume |= cb();" in body
assert "if (consume)" in body

print("PrismaHUD KeyHandler reentrancy contract passed")
