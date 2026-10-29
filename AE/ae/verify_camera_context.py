"""Execute the actual ReadSlow camera block with counted camera stubs, in AE and OG modes."""
import argparse
from pathlib import Path
import subprocess

p = argparse.ArgumentParser()
p.add_argument('--vcvars', required=True)
args = p.parse_args()
root = Path(__file__).resolve().parents[1]
out = root / '.build/ae-camera-context'
out.mkdir(parents=True, exist_ok=True)
live = (root / 'src/Live.cpp').read_text(encoding='utf-8')
block = live.split('    out["context"] =', 1)[0].rsplit('photo = true;', 1)[1]
source = r'''
#include "AeOptions.h"
#include <sstream>
#include <iostream>
namespace Phosphor::AE { inline Options options; inline bool Enabled(std::string_view key) { return options.Enabled(key); } }
namespace AE = Phosphor::AE;
namespace RE {
struct CameraStates { enum { k3rdPerson = 8 }; };
struct State { struct Id { int value = 8; int get() const { return value; } } id; };
struct PlayerCamera {
    State* currentState = nullptr;
    static inline PlayerCamera* instance = nullptr;
    static inline int calls = 0;
    static PlayerCamera* GetSingleton() { ++calls; return instance; }
};
}
bool readContext() {
''' + block + r'''
    return thirdPerson;
}
int main() {
    RE::State state;
    RE::PlayerCamera camera;
    camera.currentState = &state;
    RE::PlayerCamera::instance = &camera;
    for (int enabled : {0, 1}) {
        std::istringstream ini("[Experimental]\nbCrosshair=" + std::to_string(enabled) + "\n");
        AE::options.Read(ini);
        RE::PlayerCamera::calls = 0;
#if defined(PHOSPHOR_AE)
        const bool expected = enabled != 0;
#else
        const bool expected = true;
#endif
        if (readContext() != expected || RE::PlayerCamera::calls != int(expected)) {
            std::cerr << "Crosshair camera isolation failed: enabled=" << enabled << " calls=" << RE::PlayerCamera::calls << '\n';
            return 1;
        }
    }
    state.id.value = 0;
    if (readContext()) return 2;
    camera.currentState = nullptr;
    if (readContext()) return 3;
    RE::PlayerCamera::instance = nullptr;
    if (readContext()) return 4;
    std::cout << "Camera context isolation and null/first/third-person checks passed\n";
}
'''
(out / 'probe.cpp').write_text(source, encoding='utf-8')
for mode in ('ae', 'og'):
    define = '/DPHOSPHOR_AE=1' if mode == 'ae' else ''
    cmd = out / (mode + '.cmd')
    cmd.write_text(f'@echo off\ncall "{args.vcvars}" >nul\ncl /nologo /std:c++latest /EHsc {define} /I"{root}/ae" "{out}/probe.cpp" /Fe"{out}/{mode}.exe" /Fo"{out}/{mode}.obj"\nif errorlevel 1 exit /b %errorlevel%\n"{out}/{mode}.exe"\nexit /b %errorlevel%\n')
    result = subprocess.run(['cmd', '/d', '/c', str(cmd)], cwd=root)
    if result.returncode: raise SystemExit(result.returncode)
