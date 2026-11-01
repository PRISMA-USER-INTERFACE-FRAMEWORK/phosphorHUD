#pragma once
#include "PrismaUI_F4_API.h"
#include <chrono>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <vector>

class NotificationController {
public:
    static NotificationController* GetSingleton();

    void       SetAPI(PRISMA_UI_API::IVPrismaUI8* api) { m_api = api; }
    void       SetView(PrismaView view)                { m_view = view; }
    PrismaView GetView() const                         { return m_view; }

    void InstallHook();

    void LoadConfig();

    void NoteGameLoaded();

    void Enqueue(const char* text, bool warning);

    void OnRequestUpdate();

private:
    NotificationController() = default;

    [[nodiscard]] bool ShouldSuppress(std::string_view text) const;

    struct Pending {
        std::string text;
        bool        warning = false;
    };

    static constexpr std::size_t kMaxQueued = 64;

    PRISMA_UI_API::IVPrismaUI8* m_api  = nullptr;
    PrismaView                  m_view = 0;

    mutable std::mutex   m_lock;
    std::deque<Pending>  m_pending;

    std::string                           m_lastText;
    std::chrono::steady_clock::time_point m_lastTime{};
    static constexpr auto                 kCoalesceWindow = std::chrono::milliseconds(3000);

    std::vector<std::string>              m_patterns;
    bool                                  m_startupGate = true;
    float                                 m_muteSeconds = 8.0f;
    std::chrono::steady_clock::time_point m_loadTime{};
    bool                                  m_gateActive = false;
};
