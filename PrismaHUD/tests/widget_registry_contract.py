import re
from pathlib import Path

root = Path(__file__).resolve().parents[1]
hdr = (root / 'src' / 'HUDLayoutManager.h').read_text(encoding='utf-8')
cpp = (root / 'src' / 'HUDLayoutManager.cpp').read_text(encoding='utf-8')


def code_only(text):
    """Source lines with // comments stripped.

    A substring check treats commented-out code as present: `// ApplyToView(we);` still contains
    "ApplyToView(we);", so disabling the call would pass this guard.
    """
    out = []
    for line in text.splitlines():
        if line.strip().startswith("//"):
            continue
        out.append(line.split("//", 1)[0] if "//" in line else line)
    return "\n".join(out)


entry = hdr.split('struct WidgetEntry {', 1)[1].split('};', 1)[0]
assert 'PRISMA_HUD_API::WidgetDesc' not in entry, 'WidgetEntry must not retain caller-owned WidgetDesc pointers'
assert 'std::string id;' in entry, 'WidgetEntry must own the stable widget id'
assert 'std::string displayName;' in entry, 'WidgetEntry must own the display name'

register = code_only(cpp).split('void HUDLayoutManager::RegisterWidget(', 1)[1].split('\n\nvoid HUDLayoutManager::UnregisterWidget', 1)[0]
assert re.search(r'\bview == 0\b|!view\b', register), \
    'RegisterWidget must reject an invalid PrismaView'
assert '!desc.id' in register and "desc.id[0] == '\\0'" in register, 'RegisterWidget must reject null/empty ids'
assert 'const std::string id(desc.id);' in register, 'RegisterWidget must copy the id before storing it'
assert 'std::string(desc.displayName)' in register, 'RegisterWidget must copy the display name'
assert 'ApplyToView(we);' in register, 'RegisterWidget must deliver defaults even without a saved layout'
assert 'PushIconToView(we);' in register, 'RegisterWidget must restore saved icon state at registration'
assert re.search(r'ApplyEffectiveVisibility\(id(?:, true)?\);', register), \
    'RegisterWidget must apply current visibility gates at registration'
assert 'BindUIEvent(view, "reportPosition"' in register, 'external widgets must get position feedback binding'
assert 'BindUIEvent(view, "reportVisibility"' in register, 'external widgets must get runtime visibility binding'
assert '.desc.id' not in cpp and '.desc.displayName' not in cpp, 'manager must use owned descriptor strings'

print('PrismaHUD widget registry ownership contract passed')
