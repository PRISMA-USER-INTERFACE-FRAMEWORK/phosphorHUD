#pragma once
#include <array>
#include <cstdint>
#include <istream>
#include <string>
#include <string_view>
#include <utility>

namespace Phosphor::AE {
inline constexpr std::array<std::string_view, 16> kSwitches{
    "bCompass", "bEnemies", "bMarkerHeights", "bMines", "bTarget", "bQuickLoot",
    "bNativeHudText", "bCrosshair", "bFlashlight", "bSurvival", "bEffects",
    "bCompanions", "bPipboyFeed", "bModData", "bHideVanilla", "bEngineQuests"};

constexpr bool RuntimeAllowed(unsigned major, unsigned minor, unsigned patch, unsigned build) {
    return major == 1 && minor == 11 && patch == 240 && build == 0;
}

class Options {
    std::array<bool, kSwitches.size()> values_{};
    static std::string Trim(std::string text) {
        const auto begin = text.find_first_not_of(" \t\r\n");
        if (begin == std::string::npos) return {};
        return text.substr(begin, text.find_last_not_of(" \t\r\n") - begin + 1);
    }
public:
    Options() { values_.fill(true); }
    bool Enabled(std::string_view key) const {
        for (std::size_t i=0; i<kSwitches.size(); ++i) if (key == kSwitches[i]) return values_[i];
        return false;
    }
    void Read(std::istream& input) {
        values_.fill(true);
        bool experimental=false;
        std::string line;
        while (std::getline(input,line)) {
            line=Trim(line.substr(0,line.find_first_of(";#")));
            if (line.empty()) continue;
            if (line.front() == '[') { experimental = line == "[Experimental]"; continue; }
            if (!experimental) continue;
            const auto equal=line.find('=');
            if (equal == std::string::npos) continue;
            const auto key=Trim(line.substr(0,equal)), value=Trim(line.substr(equal+1));
            if (value != "0" && value != "1") continue;
            for (std::size_t i=0; i<kSwitches.size(); ++i) if (key == kSwitches[i]) values_[i] = value == "1";
        }
    }
    template<class F, class T> T ReadIf(std::string_view key, F&& read, T fallback) const {
        if (!Enabled(key)) return fallback;
        return std::forward<F>(read)();
    }
};
}
