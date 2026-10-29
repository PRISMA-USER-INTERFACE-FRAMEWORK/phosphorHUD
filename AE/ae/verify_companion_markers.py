"""Offline actual-code checks for companion marker math/gates and feed notice policy."""
import argparse
from pathlib import Path
import subprocess

p=argparse.ArgumentParser()
p.add_argument('--vcvars',required=True)
p.add_argument('--json',required=True,help='nlohmann_json package root with include/')
a=p.parse_args()
root=Path(__file__).resolve().parents[1]
out=root/'.build/ae-companion-markers';out.mkdir(parents=True,exist_ok=True)
live=(root/'src/Live.cpp').read_text(encoding='utf-8')
party=live.split('Json ReadParty(Json* allyMarkers) {',1)[1].split('\nnamespace {',1)[0]
prefix=party.split('    auto* player = RE::PlayerCharacter::GetSingleton();',1)[0]
marker='        if (allyMarkers) {'+party.split('        if (allyMarkers) {',1)[1].split('        if (Contains(name, "dogmeat"))',1)[0]
gate='#define AE_GATE'+(root/'ae/AeRuntime.h').read_text(encoding='utf-8').split('#define AE_GATE',1)[1]
plugin=(root/'src/Plugin.cpp').read_text(encoding='utf-8')
notice=plugin.split('        if (!pipNoticeSent',1)[1].split('            pipNoticeSent = true;',1)[0]
source=r'''
#include "AeOptions.h"
#include <nlohmann/json.hpp>
#include <cassert>
#include <chrono>
#include <cmath>
#include <numbers>
#include <sstream>
#include <iostream>
using Json=nlohmann::json;
namespace Phosphor::AE { inline Options options; bool Enabled(std::string_view k) {return options.Enabled(k);} struct ReadScope {ReadScope(std::string_view) {}}; }
namespace AE=Phosphor::AE;
''' + gate + r'''
int actorReads=0, heightReads=0;
struct Actor { struct { struct {double x=0,y=0,z=0;} location; } data; };
double Round1(double x,double step) {return std::round(x/step)*step;}
double HeightThreshold() {++heightReads;return 560;}
void AddHeight(Json& m,double dz,double threshold) {m["dz"]=dz/70; m["testAbove"]=dz>threshold;}
Json markerRead(Json* allyMarkers,Actor* actor,Actor* player) {
''' + prefix + r'''
    ++actorReads; // Boundary at the first production PlayerCharacter singleton access.
    const std::string name="Dogmeat";
''' + marker + r'''
    return out;
}
bool showNotice(bool pipNoticeSent,bool connected,int ms) {
    struct {bool connected;} pip{connected};
    auto liveStartedAt=std::chrono::steady_clock::time_point{};
    auto now=liveStartedAt+std::chrono::milliseconds(ms);
    if (!pipNoticeSent''' + notice + r'''
        return true;
    }
    return false;
}
int main() {
    int cases=0;
    for(int companions:{0,1}) for(int heights:{0,1}) {
        std::istringstream ini("[Experimental]\nbCompanions="+std::to_string(companions)+"\nbMarkerHeights="+std::to_string(heights)+"\n");
        AE::options.Read(ini);
#if defined(PHOSPHOR_AE)
        bool enabled=companions, heightEnabled=companions && heights;
#else
        bool enabled=true, heightEnabled=true;
#endif
        Actor player; player.data.location={100,200,300};
        for(int direction=0;direction<8;++direction) {
            double angle=direction*45.0*std::numbers::pi/180;
            Actor actor;actor.data.location={100+700*std::sin(angle),200+700*std::cos(angle),1000};
            Json markers=Json::array();actorReads=heightReads=0;
            markerRead(&markers,&actor,&player);
            assert(actorReads==int(enabled)); assert(heightReads==int(heightEnabled));
            assert(markers.size()==std::size_t(enabled));
            if(enabled) {assert(markers[0]["id"]=="ally:Dogmeat"); assert(markers[0]["b"]==direction*45); assert(markers[0]["dist"]==10); assert(markers[0].contains("dz")==heightEnabled);}
            ++cases;
        }
        Actor actor;actorReads=heightReads=0;markerRead(nullptr,&actor,&player);
        assert(actorReads==int(enabled) && heightReads==0);++cases;
    }
    int notices=0;
    for(int feed:{0,1}) {
        std::istringstream ini("[Experimental]\nbPipboyFeed="+std::to_string(feed)+"\n");AE::options.Read(ini);
        for(bool sent:{false,true}) for(bool connected:{false,true}) for(int ms:{0,19999,20000,20001}) {
            bool expected=!sent && !connected && ms>20000;
#if defined(PHOSPHOR_AE)
            expected=expected && feed;
#endif
            assert(showNotice(sent,connected,ms)==expected);++notices;
        }
    }
    std::cout<<cases<<" marker math/gate cases; "<<notices<<" notice-policy cases passed\n";
}
'''
(out/'probe.cpp').write_text(source,encoding='utf-8')
for mode in ('ae','og'):
    define='/DPHOSPHOR_AE=1' if mode=='ae' else ''
    cmd=out/(mode+'.cmd')
    cmd.write_text(f'@echo off\ncall "{a.vcvars}" >nul\ncl /nologo /std:c++latest /EHsc {define} /I"{root}/ae" /I"{Path(a.json).resolve()}/include" "{out}/probe.cpp" /Fe"{out}/{mode}.exe" /Fo"{out}/{mode}.obj"\nif errorlevel 1 exit /b %errorlevel%\n"{out}/{mode}.exe"\nexit /b %errorlevel%\n')
    r=subprocess.run(['cmd','/d','/c',str(cmd)],cwd=root)
    if r.returncode:raise SystemExit(r.returncode)
