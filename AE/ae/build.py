"""Offline AE target. Uses installed MSVC and explicitly supplied cached dependencies; never installs."""
import argparse, os, subprocess
from pathlib import Path
p=argparse.ArgumentParser()
p.add_argument('--commonlib',required=True);p.add_argument('--spdlog',required=True);p.add_argument('--json',required=True);p.add_argument('--valijson',required=True)
p.add_argument('--vcvars',required=True);p.add_argument('--plugin-only',action='store_true')
a=p.parse_args();root=Path(__file__).resolve().parents[1];lib=Path(a.commonlib).resolve();shared=lib/'lib/commonlib-shared'
out=root/'build/ae-experimental';out.mkdir(parents=True,exist_ok=True)
includes=[lib/'include',shared/'include',Path(a.spdlog)/'include',Path(a.json)/'include',Path(a.valijson)/'include',root/'src',root/'third_party',root/'ae']
flags=['/nologo','/std:c++latest','/EHsc','/permissive-','/bigobj','/Zc:enumTypes','/Zc:preprocessor','/O2','/MD','/utf-8','/DNOMINMAX','/DWIN32_LEAN_AND_MEAN','/DSPDLOG_USE_STD_FORMAT','/DSPDLOG_WCHAR_TO_UTF8_SUPPORT','/DSPDLOG_COMPILED_LIB']+['/I"'+str(i)+'"' for i in includes]
commands=['@echo off',f'call "{a.vcvars}" >nul','if errorlevel 1 exit /b %errorlevel%']
objects=[]
for name,sources,pch in [('shared',list((shared/'src').rglob('*.cpp')),shared/'src/REX/PCH.h'),('commonlib',list((lib/'src').rglob('*.cpp')),lib/'include/F4SE/Impl/PCH.h'),('plugin',[root/'src'/f'{n}.cpp' for n in ['Plugin','Live','Mods','Mines','Pipboy','EngineQuests','EngineProbe']]+[root/'ae/AeRuntime.cpp'],None)]:
 sources=[s for s in sources if s.stem.lower()!='pch'] # Upstream's PCH-only TU is replaced by the generated stub.
 d=out/name;d.mkdir(exist_ok=True)
 assert len({s.stem for s in sources})==len(sources),'duplicate object basenames'
 component=[d/(s.stem+'.obj') for s in sources]
 if name!='plugin':
  component += [d/'pch.obj']
  objects += [d/(name+'.lib')]
 else:objects += component
 if a.plugin_only and name!='plugin':
  rsp=d/'archive.rsp';rsp.write_text(' '.join(['/NOLOGO',f'/OUT:"{d}/{name}.lib"']+[f'"{s}"' for s in component]))
  commands += [f'lib @"{rsp}"','if errorlevel 1 exit /b %errorlevel%']
  continue
 f=flags.copy()
 if name=='plugin':f+=['/DPHOSPHOR_AE=1']
 if pch:
  stub=d/'pch.cpp';stub.write_text('// generated PCH translation unit\n')
  rsp=d/'pch.rsp';rsp.write_text(' '.join(f+[f'/FI"{pch}"',f'/Yc"{pch}"',f'/Fp"{d}/pch.pch"','/c',f'"{stub}"',f'/Fo"{d}/pch.obj"']))
  commands += [f'cl @"{rsp}"','if errorlevel 1 exit /b %errorlevel%']
  f += [f'/FI"{pch}"',f'/Yu"{pch}"',f'/Fp"{d}/pch.pch"']
 rsp=d/'compile.rsp';rsp.write_text(' '.join(f+['/MP4','/c']+[f'"{s}"' for s in sources]+[f'/Fo"{d}/"']))
 commands += [f'cl @"{rsp}"','if errorlevel 1 exit /b %errorlevel%']
 if name!='plugin':
  rsp=d/'archive.rsp';rsp.write_text(' '.join(['/NOLOGO',f'/OUT:"{d}/{name}.lib"']+[f'"{s}"' for s in component]))
  commands += [f'lib @"{rsp}"','if errorlevel 1 exit /b %errorlevel%']
# Build the engine-independent core separately too; never consume an OG build artifact.
core=out/'core.lib';core_dir=out/'core';core_dir.mkdir(exist_ok=True)
rsp=core_dir/'compile.rsp';rsp.write_text(' '.join(flags+['/MP2','/c']+[f'"{root}/src/{n}.cpp"' for n in ['Core','ModData']]+[f'/Fo"{core_dir}/"']))
commands += [f'cl @"{rsp}"','if errorlevel 1 exit /b %errorlevel%']
rsp=core_dir/'archive.rsp';rsp.write_text(f'/NOLOGO /OUT:"{core}" "{core_dir}/Core.obj" "{core_dir}/ModData.obj"')
commands += [f'lib @"{rsp}"','if errorlevel 1 exit /b %errorlevel%']
rsp=out/'link.rsp';rsp.write_text(' '.join(['/NOLOGO','/DLL','/OPT:REF','/OPT:ICF',f'/OUT:"{out}/PhosphorHUD.dll"']+[f'"{f}"' for f in objects]+[f'"{core}"',f'"{Path(a.spdlog)}/lib/spdlog.lib"']+[n+'.lib' for n in ['Advapi32','Bcrypt','D3d11','D3dcompiler','Dbghelp','Dxgi','Ole32','OleAut32','Shell32','User32','Version','Winmm','Ws2_32']]))
commands += [f'link @"{rsp}"','exit /b %errorlevel%']
bat=out/'build.cmd';bat.write_text('\n'.join(commands)+'\n')
with (out/'build.log').open('w') as log:r=subprocess.run(['cmd','/d','/c',str(bat)],cwd=root,stdout=log,stderr=subprocess.STDOUT)
print((out/'build.log').read_text(errors='replace')[-6000:]);raise SystemExit(r.returncode)
