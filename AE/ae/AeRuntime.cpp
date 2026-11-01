#include "AeRuntime.h"
#include <spdlog/spdlog.h>
#include <fstream>
#include <mutex>
#include <set>

namespace Phosphor::AE {
namespace {
std::mutex observationsMutex;
std::set<std::pair<std::string, bool>> observations;
}
void Initialize() {
    std::ifstream input("Data/F4SE/Plugins/PhosphorHUD.ini");
    options.Read(input);
    spdlog::info("AE EXPERIMENTAL 0.9.25-ae1; runtime behavior UNVERIFIED");
    for (auto key : kSwitches) spdlog::info("AE Experimental: {}={}", key, Enabled(key) ? 1 : 0);
}
void ResetObservations() {
    std::scoped_lock lock(observationsMutex);
    observations.clear();
}
void Observe(std::string_view subsystem, bool success) {
    std::scoped_lock lock(observationsMutex);
    if (!observations.emplace(std::string(subsystem), success).second) return;
    if (success) spdlog::info("AE subsystem {}: first completed read (empty/absent data is allowed)", subsystem);
    else spdlog::warn("AE subsystem {}: first failed read", subsystem);
}
void SelfCheckOnce(std::string_view path) {
    std::scoped_lock lock(observationsMutex);
    if (observations.emplace("self-check:" + std::string(path), true).second)
        spdlog::info("AE self-check: first successful Flash path {}", path);
}
}
