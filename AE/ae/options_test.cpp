#include "AeOptions.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>
int main(int argc, char** argv) {
    using namespace Phosphor::AE;
    assert(argc == 2); // Pass the INI extracted from the runtime ZIP.
    Options options;
    for (auto key : kSwitches) assert(options.Enabled(key));
    assert(!options.Enabled("unknown"));
    for (auto disabled : kSwitches) {
        std::istringstream input("[Experimental]\n" + std::string(disabled) + "=0\n");
        options.Read(input);
        for (auto key : kSwitches) {
            const bool enabled = key != disabled;
            assert(options.Enabled(key) == enabled);
            int reads = 0;
            auto value = options.ReadIf(key, [&] { ++reads; return 42; }, -1);
            assert(reads == int(enabled) && value == (enabled ? 42 : -1));
        }
    }
    std::ostringstream off;
    off << "[Experimental]\n";
    for (auto key : kSwitches) off << key << "=0\n";
    std::istringstream allOff(off.str()); options.Read(allOff);
    int disabledReads = 0;
    for (auto key : kSwitches) {
        assert(!options.Enabled(key));
        assert(options.ReadIf(key, [&] { ++disabledReads; return 42; }, -1) == -1);
    }
    assert(disabledReads == 0);
    std::istringstream text("[Main]\nbEnemies=0\n[Experimental]\nbCompass=0\nbEnemies=0\nbNativeHudText=0\nbModData=0\n[Other]\nbCompass=1\n");
    options.Read(text);
    assert(!options.Enabled("bCompass"));
    assert(!options.Enabled("bEnemies"));
    assert(!options.Enabled("bNativeHudText"));
    assert(!options.Enabled("bModData"));
    assert(options.Enabled("bMines"));
    int reads=0;
    auto value=options.ReadIf("bCompass", [&] {++reads;return 42;}, -1);
    assert(value == -1 && reads == 0);
    value=options.ReadIf("bMines", [&] {++reads;return 42;}, -1);
    assert(value == 42 && reads == 1);
    std::ifstream packaged(argv[1]); assert(packaged.good());
    std::ostringstream contents; contents << packaged.rdbuf();
    for (auto key : kSwitches) assert(contents.str().find(std::string(key) + "=1") != std::string::npos);
    std::istringstream input(contents.str()); options.Read(input);
    for (auto key : kSwitches) assert(options.Enabled(key));
    int runtimeCases = 0, allowed = 0;
    for (unsigned major : {0u, 1u}) for (unsigned minor : {10u, 11u})
        for (unsigned patch : {163u, 240u, 241u, 984u}) for (unsigned build : {0u, 1u, 15u}) {
            const bool expected = major == 1 && minor == 11 && patch == 240 && build == 0;
            assert(RuntimeAllowed(major, minor, patch, build) == expected);
            ++runtimeCases; allowed += expected;
        }
    assert(runtimeCases == 48 && allowed == 1);
    std::cout << "16 defaults, 16 single-disable matrices, all-off, parser boundaries, packaged INI and 48 runtime cases passed\n";
}
