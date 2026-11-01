#pragma once
#include <mutex>
#include <string>
#include <unordered_map>

#include "PrismaUI_F4_API.h"

void InstallVanillaHUDSuppression(PRISMA_UI_API::IVPrismaUI6* api);

struct BuiltinViews {
    PrismaView health    = 0;
    PrismaView ap        = 0;
    PrismaView compass   = 0;
    PrismaView rads      = 0;
    PrismaView ammo      = 0;
    PrismaView grenade   = 0;
    PrismaView quickloot = 0;
    PrismaView survival  = 0;
    PrismaView prompt    = 0;
    PrismaView notify    = 0;
};

class HUDDataProvider {
public:
    static HUDDataProvider* GetSingleton();

    void SetAPI(PRISMA_UI_API::IVPrismaUI4* api) { m_api = api; }
    void SetViews(BuiltinViews views)
    {
        std::lock_guard<std::recursive_mutex> lock(DataLock());
        m_views = views;
        m_lastPayload.clear();
        m_lastPrompt.clear();
    }

    void ResetTransientForLoad();

    void BindPositionReport(PrismaView view, const char* widgetId);

    void BindHealthUpdate(PrismaView view);
    void BindAPUpdate(PrismaView view);
    void BindCompassUpdate(PrismaView view);
    void BindRadsUpdate(PrismaView view);
    void BindAmmoUpdate(PrismaView view);
    void BindGrenadeUpdate(PrismaView view);
    void BindSurvivalUpdate(PrismaView view);
    void BindPromptUpdate(PrismaView view);

    static void PollCombatThrottled();

    void PushPromptNow();

    static std::recursive_mutex& DataLock();

private:
    HUDDataProvider() = default;
    PRISMA_UI_API::IVPrismaUI4*              m_api = nullptr;
    BuiltinViews                              m_views;
    std::unordered_map<PrismaView, std::string> m_lastPayload;
    std::string                               m_lastPrompt;
};
