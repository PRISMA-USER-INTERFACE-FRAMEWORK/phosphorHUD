#pragma once
#include "PrismaHUD_F4_API.h"
#include <nlohmann/json.hpp>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

class ActivateChoiceRegistry : public PRISMA_HUD_API::IVPrismaHUD3 {
public:
    static ActivateChoiceRegistry* GetSingleton();

    void RegisterActivateChoice(const char* id, const char* label, const char* iconId,
                                PRISMA_HUD_API::ActivateChoiceCallback onTrigger,
                                void* context) noexcept override;
    void UnregisterActivateChoice(const char* id) noexcept override;

    std::vector<nlohmann::json> BuildChoicesJSON();

    bool TriggerByIndex(std::uint32_t index);

private:
    ActivateChoiceRegistry() = default;

    struct Entry {
        std::string                            id;
        std::string                            label;
        std::string                            icon;
        PRISMA_HUD_API::ActivateChoiceCallback cb  = nullptr;
        void*                                  ctx = nullptr;
    };

    std::mutex         m_lock;
    std::vector<Entry> m_choices;
};
