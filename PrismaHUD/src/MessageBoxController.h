#pragma once

#include "MenuReplacementSuite.h"
#include "PrismaUI_F4_API_V11.h"

#include <atomic>
#include <cstdint>

class MessageBoxController {
public:
    static MessageBoxController* GetSingleton();

    void SetAPI(PRISMA_UI_API::IVPrismaUI11* api)
    {
        m_api = api;
        InitializeMenuReplacementSuite(api);
    }
    void SetView(PrismaView view);
    [[nodiscard]] PrismaView View() const { return m_view.load(); }

    void InstallMenuSink();

    void OnMessageBoxOpen();
    void OnMessageBoxClose();

    void Answer(std::uint32_t index);
    void OnTick();

    [[nodiscard]] bool Active() const { return m_active.load(); }

private:
    MessageBoxController() = default;

    void OpenOnGameThread();
    void CloseOnGameThread();
    void ProbeDomReady();
    bool BindVerifiedEvents();

    static void DispatchOpen(void* userdata);
    static void DispatchClose(void* userdata);
    static void DispatchRetryOpen(void* userdata);
    static void DomReadyProbeResult(const char* result);
    static void MessageBoxTickEvent(const char* argument, void* userdata);
    static void MessageBoxAnswerEvent(const char* argument, void* userdata);

    PRISMA_UI_API::IVPrismaUI11* m_api = nullptr;
    std::atomic<PrismaView> m_view{ 0 };

    std::atomic<bool> m_openRequested{ false };
    std::atomic<bool> m_domReady{ false };
    std::atomic<bool> m_domProbeInFlight{ false };
    std::atomic<bool> m_bindingsReady{ false };
    std::atomic<bool> m_active{ false };
    std::atomic<bool> m_answered{ false };
};
