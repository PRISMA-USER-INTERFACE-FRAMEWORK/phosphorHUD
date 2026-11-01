#pragma once
#include <mutex>

#include "PrismaUI_F4_API.h"

void InstallVanillaHUDSuppression(PRISMA_UI_API::IVPrismaUI6* api);

class HUDDataProvider {
public:
    static HUDDataProvider* GetSingleton();

    void SetAPI(PRISMA_UI_API::IVPrismaUI4* api) { m_api = api; }

    void BindPositionReport(PrismaView view, const char* widgetId);

    static void PollCombatThrottled();

    static std::recursive_mutex& DataLock();

private:
    HUDDataProvider() = default;
    PRISMA_UI_API::IVPrismaUI4* m_api = nullptr;
};
