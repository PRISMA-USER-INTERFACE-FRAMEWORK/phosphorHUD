"""Compile actual native-HUD switch/early-exit code with counted UI access stubs."""
import argparse
from pathlib import Path
import subprocess

p = argparse.ArgumentParser()
p.add_argument('--vcvars', required=True)
args = p.parse_args()
root = Path(__file__).resolve().parents[1]
out = root / '.build/ae-native-hud'
out.mkdir(parents=True, exist_ok=True)
live = (root / 'src/Live.cpp').read_text(encoding='utf-8')
gate = live.split('void RequestNativeHud(NativeHudRequest request) {', 1)[1].split('    if (nativeHud.Submit', 1)[0]
early = live.split('    NativeHudSnapshot snapshot;', 1)[1].split('    auto* ui = RE::UI::GetSingleton();', 1)[0]
source = r'''
#include "AeOptions.h"
#include <sstream>
#include <iostream>
namespace AE = Phosphor::AE;
namespace Phosphor::AE { inline Options options; bool Enabled(std::string_view key) { return options.Enabled(key); } }
struct NativeHudRequest { bool capture=false, gamepad=false, hideText=false, hideCrosshair=false, hideFlashlight=false, hideCompass=false; };
struct NativeHudSnapshot { bool visibilityApplied=false; };
int uiReads=0;
NativeHudRequest gated(NativeHudRequest request) {
''' + gate + r'''
    return request;
}
NativeHudSnapshot capture(const NativeHudRequest& request, const NativeHudRequest& appliedHiding) {
    NativeHudSnapshot snapshot;
''' + early + r'''
    ++uiReads; // First RE::UI access in the actual function.
    return snapshot;
}
int main() {
    const char* keys[]={"bHideVanilla","bNativeHudText","bCrosshair","bFlashlight","bCompass"};
    int cases=0;
    for(int switches=0;switches<32;++switches) {
        std::ostringstream ini; ini << "[Experimental]\n";
        for(int i=0;i<5;++i) ini << keys[i] << '=' << ((switches>>i)&1) << '\n';
        std::istringstream input(ini.str()); AE::options.Read(input);
        for(int bits=0;bits<32;++bits) for(int restoring=0;restoring<2;++restoring) {
            NativeHudRequest r{bool(bits&1),false,bool(bits&2),bool(bits&4),bool(bits&8),bool(bits&16)};
            NativeHudRequest expected=r;
#if defined(PHOSPHOR_AE)
            expected.hideText = r.hideText && (switches&1) && (switches&2);
            expected.hideCrosshair = r.hideCrosshair && (switches&1) && (switches&4);
            expected.hideFlashlight = r.hideFlashlight && (switches&1) && (switches&8);
            expected.hideCompass = r.hideCompass && (switches&1) && (switches&16);
            expected.capture = r.capture && (switches&14);
            bool shouldRead=expected.capture || expected.hideCompass || restoring;
#else
            bool shouldRead=expected.capture || restoring;
#endif
            auto actual=gated(r);
            if(actual.capture!=expected.capture || actual.hideText!=expected.hideText ||
               actual.hideCrosshair!=expected.hideCrosshair || actual.hideFlashlight!=expected.hideFlashlight ||
               actual.hideCompass!=expected.hideCompass) return 1;
            NativeHudRequest applied; applied.hideCompass=restoring;
            uiReads=0; capture(actual,applied);
            if(uiReads!=int(shouldRead)) {
                std::cerr << "UI boundary failed switches=" << switches << " request=" << bits << " restoring=" << restoring << '\n';
                return 2;
            }
            ++cases;
        }
    }
    std::cout << cases << " native-HUD switch/request/restore cases passed\n";
}
'''
(out / 'probe.cpp').write_text(source, encoding='utf-8')
for mode in ('ae', 'og'):
    define = '/DPHOSPHOR_AE=1' if mode == 'ae' else ''
    cmd = out / (mode + '.cmd')
    cmd.write_text(f'@echo off\ncall "{args.vcvars}" >nul\ncl /nologo /std:c++latest /EHsc {define} /I"{root}/ae" "{out}/probe.cpp" /Fe"{out}/{mode}.exe" /Fo"{out}/{mode}.obj"\nif errorlevel 1 exit /b %errorlevel%\n"{out}/{mode}.exe"\nexit /b %errorlevel%\n')
    result = subprocess.run(['cmd', '/d', '/c', str(cmd)], cwd=root)
    if result.returncode:
        raise SystemExit(result.returncode)
