#include "ControlLabels.h"
#include "BindingRegistry.h"
#include "PrismaUI_F4_API.h"

#include "RE/C/ControlMap.h"

#include <atomic>
#include <cstddef>
#include <string_view>

namespace ControlLabels {

    namespace {
        std::atomic<PRISMA_UI_API::IVPrismaUI9*> g_fw{ nullptr };

        struct HUDDefault { std::string_view action; const char* canon; std::uint32_t code; };
        constexpr HUDDefault kHUDDefaults[] = {
            { "take_all",       "X",        0x14000u },
            { "transfer",       "Y",        0x18000u },
            { "bench_transfer", "X",        0x14000u },
            { "select",         "DPad",     0u },
            { "select_up",      "DPadUp",   0x10001u },
            { "select_down",    "DPadDown", 0x10002u },
            { "rollover_hide",  "RS",       0x10080u },
        };

        bool IsValidMappedCode(std::uint32_t raw)
        {
            return raw != 0xFFu && raw != 0xFFFFu && raw != 0xFFFFFFFFu;
        }

        std::uint32_t NormalizeVerifiedGamepadCode(std::uint32_t raw)
        {
            auto* fw = g_fw.load();
            if (!fw || !IsValidMappedCode(raw)) return 0;

            char button[32]{};
            return fw->GetGamepadButtonName(raw, button, sizeof(button)) && button[0] != '\0'
                ? (raw | 0x10000u)
                : 0u;
        }

        std::uint32_t LiveQuickContainerPerkGamepadCode()
        {
            auto* cm = RE::ControlMap::GetSingleton();
            if (!cm) return 0;

            constexpr auto contextId = RE::UserEvents::INPUT_CONTEXT_ID::kQuickContainerMenuPerk;
            const auto contextIndex = static_cast<std::size_t>(contextId);
            auto* context = cm->controlMaps[contextIndex];
            if (!context) return 0;

            const auto deviceIndex = static_cast<std::size_t>(RE::INPUT_DEVICE::kGamepad);
            const auto& mappings = context->deviceMappings[deviceIndex];

            for (const auto& mapping : mappings) {
                const auto code = NormalizeVerifiedGamepadCode(static_cast<std::uint32_t>(mapping.inputKey));
                if (code) return code;
            }

            return 0;
        }

        std::string QuickContainerPerkGamepadPrompt()
        {
            auto* fw = g_fw.load();
            if (!fw) return {};

            const auto code = LiveQuickContainerPerkGamepadCode();
            if (!code) return {};

            char button[32]{};
            return fw->GetGamepadButtonName(code, button, sizeof(button)) && button[0] != '\0'
                ? std::string{"gp:"} + button
                : std::string{};
        }
    }

    void SetFrameworkAPI(void* v9) { g_fw.store(static_cast<PRISMA_UI_API::IVPrismaUI9*>(v9)); }

    void NoteInputDevice(RE::INPUT_DEVICE device)
    {
        auto* fw = g_fw.load();
        if (!fw) return;
        int d = device == RE::INPUT_DEVICE::kGamepad ? 2 : device == RE::INPUT_DEVICE::kMouse ? 1 : 0;
        fw->NoteInputDevice(d);
    }

    bool  UsingGamepad()      { auto* fw = g_fw.load(); return fw && fw->IsUsingGamepad(); }
    Style GetStyle()          { auto* fw = g_fw.load(); return (fw && fw->GetControllerStyle() == 1) ? Style::kPlayStation : Style::kXbox; }
    void  SetStyle(Style s)   { if (auto* fw = g_fw.load()) fw->SetControllerStyle(s == Style::kPlayStation ? 1 : 0); }

    std::string ForUserEvent(const char* userEvent)
    {
        auto* fw = g_fw.load();
        if (!fw || !userEvent) return {};
        char buf[64] = {};
        return fw->GetButtonPrompt(userEvent, buf, sizeof(buf)) ? std::string(buf) : std::string{};
    }

    std::uint32_t KeyboardCodeForUserEvent(const char* userEvent)
    {
        if (!userEvent) return 0;
        auto* cm = RE::ControlMap::GetSingleton();
        if (!cm) return 0;

        const auto raw = cm->GetMappedKey(userEvent, RE::INPUT_DEVICE::kKeyboard,
                                          RE::UserEvents::INPUT_CONTEXT_ID::kMainGameplay);
        return IsValidMappedCode(raw) ? raw : 0u;
    }

    std::uint32_t GamepadCodeForUserEvent(const char* userEvent)
    {
        if (!userEvent) return 0;
        auto* cm = RE::ControlMap::GetSingleton();
        if (!cm) return 0;

        const auto raw = cm->GetMappedKey(userEvent, RE::INPUT_DEVICE::kGamepad,
                                          RE::UserEvents::INPUT_CONTEXT_ID::kMainGameplay);
        return NormalizeVerifiedGamepadCode(raw);
    }

    std::uint32_t GamepadCodeForHUDAction(const char* actionId)
    {
        if (!actionId) return 0;
        const std::string_view id{ actionId };
        if (id == "perk") return LiveQuickContainerPerkGamepadCode();
        if (id == "take") return GamepadCodeForUserEvent("Activate");

        for (const auto& d : kHUDDefaults) {
            if (d.action == id) return d.code;
        }
        return 0;
    }

    std::string ForHUDAction(const char* actionId)
    {
        if (!actionId) return {};
        const std::string_view id{ actionId };

        if (id == "take") return ForUserEvent("Activate");

        if (UsingGamepad()) {
            if (id == "perk") return QuickContainerPerkGamepadPrompt();

            for (const auto& d : kHUDDefaults) {
                if (d.action == id) return std::string{"gp:"} + d.canon;
            }
            return {};
        }

        if (id == "perk")           return "Space";
        if (id == "bench_transfer") return "R";

        const auto vk = BindingRegistry::GetSingleton()->GetBoundKey(actionId);
        return vk ? BindingRegistry::KeyName(vk) : std::string{};
    }
}
