#include "PluginState.h"

namespace Phosphor::Plugin {
namespace {
struct AutoTest {
    bool checked = false;
    int wanted = 0, done = 0, phase = 0, step = 0;
    std::uint64_t gen = 0;
    Clock::time_point at{};
} autotest;
void AutoTestQueueLoad(const char* what) {
    auto* manager = RE::BGSSaveLoadManager::GetSingleton();
    if (!manager) { spdlog::warn("Autotest: no save/load manager"); return; }
    autotest.gen = generation.load();
    spdlog::info("Autotest: {} - loading the most recent save", what);
    manager->QueueSaveLoadTask(RE::BGSSaveLoadManager::QUEUED_TASK::kLoadMostRecentSave);
}
}
void AutoTestTick() {
    if (!autotest.checked) {
        autotest.checked = true;
        const auto ini = ReadIni("Data/F4SE/Plugins/PhosphorHUD/autotest.ini");
        if (const auto it = ini.find("iInGameLoads"); it != ini.end()) autotest.wanted = std::clamp(it->second, 0, 20);
        if (autotest.wanted > 0) spdlog::info("Autotest: on, {} in-game loads planned", autotest.wanted);
    }
    if (autotest.wanted <= 0 || autotest.phase >= 6) return;
    auto* ui = RE::UI::GetSingleton();
    if (!ui) return;
    const auto now = Clock::now();
    const bool mainMenu = ui->GetMenuOpen(RE::BSFixedString("MainMenu"));
    const bool loading = ui->GetMenuOpen(RE::BSFixedString("LoadingMenu")) || ui->GetMenuOpen(RE::BSFixedString("FaderMenu"));
    using Clk = Clock::time_point;
    switch (autotest.phase) {
    case 0: {
        if (session) {
            std::ofstream("Data/F4SE/Plugins/PhosphorHUD/autotest.ini", std::ios::trunc) << "[Main]\niInGameLoads=0\n";
            autotest.phase = 1; autotest.at = {};
            break;
        }
        if (ui->GetMenuOpen(RE::BSFixedString("LoadingMenu")) || !mainMenu) break;
        if (autotest.at == Clk{}) { autotest.at = now + std::chrono::seconds(20); break; }
        if (now < autotest.at) break;
        ++autotest.step;
        AutoTestQueueLoad(std::format("main menu, attempt {}", autotest.step).c_str());
        autotest.at = now + std::chrono::seconds(30);
        break;
    }
    case 1:
        if (generation.load() == autotest.gen || !session || !liveStarted || loading || mainMenu) { autotest.at = {}; break; }
        if (autotest.at == Clk{}) {
            spdlog::info("Autotest: in game after load {} of {}", autotest.done, autotest.wanted);
            autotest.at = now + std::chrono::seconds(45);
            break;
        }
        if (now >= autotest.at) {
            if (autotest.done >= autotest.wanted) {
                spdlog::info("Autotest: PASSED - {} in-game loads, no crash", autotest.done);
                autotest.phase = 5; autotest.at = now + std::chrono::seconds(5);
                break;
            }
            if (auto* queue = RE::UIMessageQueue::GetSingleton()) queue->AddMessage(RE::BSFixedString("PauseMenu"), RE::UI_MESSAGE_TYPE::kShow);
            spdlog::info("Autotest: pause menu opened");
            autotest.phase = 2; autotest.at = now + std::chrono::seconds(4);
        }
        break;
    case 2:
        if (now < autotest.at) break;
        if (auto* queue = RE::UIMessageQueue::GetSingleton()) queue->AddMessage(RE::BSFixedString("PauseMenu"), RE::UI_MESSAGE_TYPE::kHide);
        spdlog::info("Autotest: pause menu closed");
        autotest.phase = 3; autotest.at = now + std::chrono::seconds(5);
        break;
    case 3:
        if (now < autotest.at) break;
        ++autotest.done;
        AutoTestQueueLoad(std::format("in-game load {} of {}", autotest.done, autotest.wanted).c_str());
        autotest.phase = 1; autotest.at = {};
        break;
    case 5:
        if (now < autotest.at) break;
        spdlog::info("Autotest: quitting to desktop");
        if (auto* main = RE::Main::GetSingleton()) main->quitGame = true;
        autotest.phase = 6;
        break;
    default: break;
    }
}
}
