#pragma once

#include "PrismaHUD_F4_API.h"
#include "KeyHandler.h"

#include <cstdint>
#include <functional>
#include <map>
#include <shared_mutex>
#include <string>
#include <vector>

class BindingRegistry : public PRISMA_HUD_API::IVPrismaHUD2
{
public:
    static BindingRegistry* GetSingleton();

    void LoadFromDisk();

    void     RegisterBinding(PRISMA_HUD_API::BindingDesc desc, PRISMA_HUD_API::BindingCallback onDown,
                             void* downContext, PRISMA_HUD_API::BindingCallback onUp,
                             void* upContext) noexcept override;
    void     UnregisterBinding(const char* id) noexcept override;
    uint32_t GetBoundKey(const char* id) noexcept override;
    void     SetExclusiveInputMode(bool on) noexcept override;
    bool     IsExclusiveModeActive() const noexcept override { return _exclusiveCount.load() > 0; }

    std::string BuildListJSON();

    bool SetKey(const char* id, uint32_t vk);

    void ResetToDefault(const char* id);

    std::string FindConflict(uint32_t vk, bool requireShift, const char* excludeId);

    std::string CheckRebindConflict(const char* id, uint32_t vk);

    static std::string KeyName(uint32_t vk);

    void SetCaptureMode(bool on) { _captureMode.store(on); }

private:
    BindingRegistry() = default;

    struct Binding {
        std::string                     id, label, category;
        uint32_t                        defaultKey  = 0;
        uint32_t                        currentKey  = 0;
        bool                            requireShift = false;
        bool                            consume      = false;
        bool                            suppressible = false;
        PRISMA_HUD_API::BindingCallback onDown = nullptr;  void* downCtx = nullptr;
        PRISMA_HUD_API::BindingCallback onUp   = nullptr;  void* upCtx   = nullptr;
        KeyHandlerEvent                 downReg = 0;
        KeyHandlerEvent                 upReg   = 0;
    };

    void ApplyBinding(Binding& b);

    void Save();

    static std::filesystem::path KeybindsPath();

    std::map<std::string, Binding>   _bindings;
    std::map<std::string, uint32_t>  _savedOverrides;
    std::atomic<int>                 _exclusiveCount{ 0 };
    std::atomic<bool>                _captureMode{ false };
    mutable std::shared_mutex        _mutex;
    std::mutex                       _saveMutex;
};
