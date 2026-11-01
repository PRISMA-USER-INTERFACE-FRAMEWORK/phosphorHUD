#pragma once

#include "PrismaUI_F4_API_V11.h"

#include <string>

class SleepWaitMirrorController
{
public:
    static SleepWaitMirrorController* GetSingleton();

    void SetAPI(PRISMA_UI_API::IVPrismaUI11* api) noexcept { m_api = api; }
    void SetView(PrismaView view) noexcept;
    void OnDomReady(PrismaView view);

    void InstallMenuSink();
    void ResetForLoad();

    void OnRequestUpdate();

    void OnMenuOpenVerified();
    void OnMenuCloseVerified();

private:
    SleepWaitMirrorController() = default;

    std::string BuildSnapshot();
    void HideMirror();
    void DumpLiveMovieOnce();

    PRISMA_UI_API::IVPrismaUI11* m_api{ nullptr };
    PrismaView m_view{ 0 };
    bool m_domReady{ false };
    bool m_open{ false };
    bool m_dumped{ false };
    std::string m_last;
};
