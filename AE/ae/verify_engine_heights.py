"""Execute actual engine marker-height reads with counted Z fields."""
import argparse, re, subprocess
from pathlib import Path
p = argparse.ArgumentParser(); p.add_argument('--vcvars', required=True); a = p.parse_args()
root = Path(__file__).resolve().parents[1]; out = root / '.build/ae-engine-heights'; out.mkdir(exist_ok=True)
text = (root / 'src/EngineQuests.cpp').read_text(encoding='utf-8')
blocks = re.findall(r'(?:#if defined\(PHOSPHOR_AE\)\n\s*if \(Phosphor::AE::Enabled\("bMarkerHeights"\)\) \{\n#endif\n)?\s*marker.z = (?:door|ref)->data.location.z;\n\s*marker.hasZ = true;(?:\n#if defined\(PHOSPHOR_AE\)\n\s*}\n#endif)?', text)
assert len(blocks) == 2
source = r'''
#include "AeOptions.h"
#include <sstream>
#include <cassert>
#include <iostream>
namespace Phosphor::AE { Options options; bool Enabled(std::string_view k) { return options.Enabled(k); } }
int reads;
struct Z { operator double() const { ++reads; return 123; } };
struct Ref { struct { struct { Z z; } location; } data; };
struct Marker { double z=0; bool hasZ=false; };
''' + '\n'.join(f'Marker read{i}(Ref* door, Ref* ref) {{ Marker marker;\n{b}\nreturn marker; }}' for i,b in enumerate(blocks)) + r'''
int main() {
    Ref ref; int cases=0;
    for(int enabled : {0,1}) {
        std::istringstream ini("[Experimental]\nbMarkerHeights=" + std::to_string(enabled));
        Phosphor::AE::options.Read(ini);
        for(auto read : {read0, read1}) {
            reads=0; auto marker=read(&ref,&ref);
            assert(marker.hasZ == bool(enabled) && reads == enabled);
            if(enabled) assert(marker.z == 123);
            ++cases;
        }
    }
    std::cout << cases << " engine target/door Z-read isolation cases passed\n";
}
'''
(out/'probe.cpp').write_text(source)
cmd=out/'run.cmd';cmd.write_text(f'@echo off\ncall "{a.vcvars}" >nul\ncl /nologo /std:c++latest /EHsc /DPHOSPHOR_AE=1 /I"{root}/ae" "{out}/probe.cpp" /Fe"{out}/probe.exe" /Fo"{out}/probe.obj"\nif errorlevel 1 exit /b %errorlevel%\n"{out}/probe.exe"\nexit /b %errorlevel%\n')
subprocess.run(['cmd','/d','/c',str(cmd)],cwd=root,check=True)
