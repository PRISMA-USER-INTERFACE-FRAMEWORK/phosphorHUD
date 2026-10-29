"""Run the real reader/probe entry gates with counted engine access, plus source wiring checks."""
import argparse, subprocess
from pathlib import Path
p = argparse.ArgumentParser()
p.add_argument('--vcvars', required=True)
a = p.parse_args()
root = Path(__file__).resolve().parents[1]
out = root / '.build/ae-engine-quests'
out.mkdir(parents=True, exist_ok=True)
pieces = []
for file, signature, name, tail in (
    ('EngineQuests.cpp', 'Pipboy::Snapshot Read() {', 'read', 'return {true};'),
    ('EngineProbe.cpp', 'void Run(const Pipboy::Snapshot& pip, const char* why) {', 'probe', ''),
):
    text = (root / 'src' / file).read_text(encoding='utf-8')
    body = text.split(signature, 1)[1].split('    auto* player = RE::PlayerCharacter::GetSingleton();', 1)[0]
    assert 'bEngineQuests' in body and 'bCompass' in body, file + ': missing pre-access gates'
    pieces.append(('Snapshot ' if name == 'read' else 'void ') + name + '() {\n' + body + '\n++engineReads;\n' + tail + '\n}\n')
source = r'''
#include "AeOptions.h"
#include <cassert>
#include <iostream>
#include <sstream>
namespace Phosphor::AE { Options options; bool Enabled(std::string_view k) { return options.Enabled(k); } }
namespace Pipboy { struct Snapshot { bool connected = false; }; }
using Pipboy::Snapshot;
int engineReads = 0, readNo = 0;
void Step(const char*) {}
#define AE_GATE(key, ...) if (!Phosphor::AE::Enabled(key)) return __VA_ARGS__
''' + '\n'.join(pieces) + r'''
int main() {
    int cases = 0;
    for (int compass : {0,1}) for (int quests : {0,1}) {
        std::istringstream ini("[Experimental]\nbCompass=" + std::to_string(compass) + "\nbEngineQuests=" + std::to_string(quests));
        Phosphor::AE::options.Read(ini);
        engineReads = 0;
        assert(read().connected == bool(compass && quests));
        probe();
        assert(engineReads == 2 * int(compass && quests));
        ++cases;
    }
    std::cout << cases << " reader/probe gate combinations passed; disabled paths make zero engine accesses\n";
}
'''
(out / 'probe.cpp').write_text(source, encoding='utf-8')
cmd = out / 'run.cmd'
cmd.write_text(f'@echo off\ncall "{a.vcvars}" >nul\ncl /nologo /std:c++latest /EHsc /DPHOSPHOR_AE=1 /I"{root}/ae" "{out}/probe.cpp" /Fe"{out}/probe.exe" /Fo"{out}/probe.obj"\nif errorlevel 1 exit /b %errorlevel%\n"{out}/probe.exe"\nexit /b %errorlevel%\n')
subprocess.run(['cmd', '/d', '/c', str(cmd)], cwd=root, check=True)
