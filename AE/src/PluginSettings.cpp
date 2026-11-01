#include "PluginState.h"

namespace Phosphor::Plugin {
Phosphor::Ini ReadIni(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    return Phosphor::ReadMainIni(file);
}
void SendSettings() {
    const auto defaults = ReadIni("Data/MCM/Config/PhosphorHUD/settings.ini");
    const auto overrides = ReadIni("Data/MCM/Settings/PhosphorHUD.ini");
#if defined(PHOSPHOR_AE)
    Phosphor::Ini condition;
    if (Phosphor::AE::Enabled("bModData")) {
    condition = ReadIni("Data/MCM/Config/ConditionBoy/settings.ini");
#else
    auto condition = ReadIni("Data/MCM/Config/ConditionBoy/settings.ini");
#endif
    for (const auto& [key, value] : ReadIni("Data/MCM/Settings/ConditionBoy.ini")) {
        if (key != "iCharacterType" || value == 0 || value == 1) condition[key] = value;
    }
#if defined(PHOSPHOR_AE)
    }
#endif
    std::optional<int> character;
    if (const auto it = condition.find("iCharacterType"); it != condition.end()) character = it->second;
    auto settings = Phosphor::MakeSettings(defaults, overrides, character);
    {
        std::ifstream keybinds("Data/MCM/Settings/Keybinds.json", std::ios::binary);
        const auto key = keybinds ? Phosphor::FindKeybind(keybinds, "PhosphorHUD", "PerkChoicesKey") : std::nullopt;
        Phosphor::QuickLootFirst::Configure(Phosphor::QuickLootFirstEnabled(defaults, overrides),
                                            key ? key->code : 0, key ? key->modifiers : 0);
    }
    dropMarkers = settings.value("legendaryDropMarkers", true);
    if (settings.contains("contextRules") && settings["contextRules"].is_object())
        dialogueRule = settings["contextRules"].value("dialogue", std::string("recommended"));
    Send(Json{{"settings", settings}, {"perkKey", Phosphor::Mods::PerkMenuKey()}});
    maxEffectSeconds = 3600.0F * Phosphor::EffectHourLimit(defaults, overrides);
    effectsKey.clear();
    settingsDirty = false;
    spdlog::info("MCM settings refreshed");
}
}
