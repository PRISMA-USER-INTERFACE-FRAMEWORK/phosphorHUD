#pragma once

#include "PrismaUI_F4_API_V11.h"

#include <atomic>

class BarterMenuController
{
public:
    static BarterMenuController* GetSingleton();

    void Initialize(PRISMA_UI_API::IVPrismaUI11* api);
    void InstallMenuSink();
    void OnRawMenuEvent(bool opening);

    void OnMenuOpenVerified();
    void OnMenuCloseVerified();
    void OnDomReady(PrismaView view);
    void OnAction(const char* raw);
    void OnTick();

private:
    BarterMenuController() = default;

    [[nodiscard]] bool ActivateReplacement();
    void RestoreVanilla();
    void HideReplacement();
    void PushState();
    void QueueCategory(bool fromContainer, std::uint32_t filterMask);

    static void DomReadyCallback(PrismaView view);
    static void DispatchOpen(void* userdata);
    static void DispatchClose(void* userdata);
    static void ActionEvent(const char* argument, void* userdata);
    static void TickEvent(const char* argument, void* userdata);

    PRISMA_UI_API::IVPrismaUI11* m_api{ nullptr };
    PrismaView m_view{ 0 };
    std::atomic<bool> m_openRequested{ false };
    bool m_domReady{ false };
    bool m_active{ false };
    bool m_sinkInstalled{ false };
};
