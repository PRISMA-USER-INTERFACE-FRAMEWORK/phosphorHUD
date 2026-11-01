#include <mutex>

#include "HUDDataProvider.h"
#include "HUDLayoutManager.h"
#include <shlobj.h>
#include <functional>

namespace
{
    constexpr const char* kSuppressedWidgets[] = {
        "HUDQuickContainer",
        "HUDRollover",
    };
}

void InstallVanillaHUDSuppression(PRISMA_UI_API::IVPrismaUI6* api)
{
    if (!api) {
        logger::error("PrismaHUD: InstallVanillaHUDSuppression called with a null V6 API pointer");
        return;
    }
    for (const char* widget : kSuppressedWidgets) {
        const bool ok = api->SuppressHUDWidget(widget, true);
        logger::info("PrismaHUD: SuppressHUDWidget('{}') -> {}", widget, ok);
    }
}

HUDDataProvider* HUDDataProvider::GetSingleton()
{
    static HUDDataProvider s_instance;
    return &s_instance;
}

static void OnPositionReport(const char* json)
{
    try {
        auto j  = nlohmann::json::parse(json);
        auto id = j.at("id").get<std::string>();
        HUDLayoutManager::GetSingleton()->UpdatePositionFromReport(id.c_str(), json);
    } catch (...) {}
}

void HUDDataProvider::BindPositionReport(PrismaView view, const char* )
{
    if (!m_api) return;
    m_api->BindUIEvent(view, "reportPosition", OnPositionReport);
}

void HUDDataProvider::PollCombatThrottled()
{
    auto* lm = HUDLayoutManager::GetSingleton();
    if (!lm->GetCombatOnly()) return;
    if (!lm->ShouldPollCombat()) return;
    bool inCombat = false;
    try {
        auto* pc = RE::PlayerCharacter::GetSingleton();
        if (pc) inCombat = pc->IsInCombat();
    } catch (...) {}
    lm->UpdateCombatState(inCombat);
}

std::recursive_mutex& HUDDataProvider::DataLock()
{
    static std::recursive_mutex m;
    return m;
}
