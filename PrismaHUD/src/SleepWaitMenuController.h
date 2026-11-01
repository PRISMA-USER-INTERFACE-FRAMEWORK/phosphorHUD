#pragma once

#include "PrismaUI_F4_API_V11.h"

#include <string>

class SleepWaitMenuController
{
public:
    static SleepWaitMenuController* GetSingleton();

    void Initialize(PRISMA_UI_API::IVPrismaUI11* api);
    void InstallMenuSink();
    void OnRawMenuEvent(bool opening);

    void OnMenuOpenVerified();
    void OnMenuCloseVerified();
    void OnDomReady(PrismaView view);
    void OnAction(const char* raw);
    void OnTick();

private:
    SleepWaitMenuController() = default;

    [[nodiscard]] bool EnsureView();
    [[nodiscard]] bool ActivateIfNeeded();
    [[nodiscard]] bool WriteHours(int hours);
    [[nodiscard]] bool InvokeVanillaUserEvent(const char* eventName);
    [[nodiscard]] std::string BuildSnapshot();
    void RestoreVanilla();
    void HideReplacement();
    void PushSnapshot();

    static void DomReadyCallback(PrismaView view);
    static void DispatchOpen(void* userdata);
    static void DispatchClose(void* userdata);
    static void ActionEvent(const char* argument, void* userdata);
    static void TickEvent(const char* argument, void* userdata);

    PRISMA_UI_API::IVPrismaUI11* m_api{ nullptr };
    PrismaView m_view{ 0 };
    bool m_domReady{ false };
    bool m_open{ false };
    bool m_active{ false };
    bool m_sinkInstalled{ false };
    int m_selectedHours{ 1 };
    int m_maxHours{ 24 };
    std::string m_last;
};
