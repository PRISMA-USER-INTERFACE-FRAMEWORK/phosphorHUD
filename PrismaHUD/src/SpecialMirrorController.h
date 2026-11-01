#pragma once
#include "PrismaUI_F4_API.h"
#include <string>

class SpecialMirrorController {
public:
    static SpecialMirrorController* GetSingleton();

    void       SetAPI(PRISMA_UI_API::IVPrismaUI4* api) { m_api = api; }
    void       SetView(PrismaView view)                { m_view = view; }
    PrismaView View() const                            { return m_view; }

    void InstallMenuSink();

    void OnMenuOpen();
    void OnMenuClose();

    void OnRequestUpdate();

private:
    SpecialMirrorController() = default;

    std::string BuildJSON() const;

    PRISMA_UI_API::IVPrismaUI4* m_api  = nullptr;
    PrismaView                  m_view = 0;
    bool                        m_open = false;
    std::string                 m_last;
};
