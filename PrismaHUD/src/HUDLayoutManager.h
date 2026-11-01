#pragma once
#include "PrismaHUD_F4_API.h"
#include "PrismaUI_F4_API.h"
#include <atomic>
#include <unordered_set>
#include <cstdint>
#include <string>

struct WidgetEntry {
    std::string id;
    std::string displayName;
    PrismaView  view = 0;
    float       defaultX = 0.f;
    float       defaultY = 0.f;
    float       defaultW = 0.f;
    float       defaultH = 0.f;
    float       x = 0.f, y = 0.f;
    float       s = 1.f;
    float       r = 0.f;
};

class HUDLayoutManager : public PRISMA_HUD_API::IVPrismaHUD1 {
public:
    static HUDLayoutManager* GetSingleton();

    void RegisterWidget(PrismaView view, PRISMA_HUD_API::WidgetDesc desc) noexcept override;
    void UnregisterWidget(PrismaView view) noexcept override;
    void ApplyLayout(PrismaView view) noexcept override;

    void LoadAndApply();
    void ResetTransientForLoad();
    void SaveFromJS(const char* json);
    std::string BuildEditorPayload() const;
    void LiveMove(const char* json);
    void UpdatePositionFromReport(const char* widgetId, const char* json);
    void ShowAllWidgets();
    void RestoreWidgetVisibility();
    void SetEditorPreview(bool on);

    void LoadSettings();
    void ToggleMasterHidden();
    void SetCombatOnly(bool on);
    void SetHideKey(std::uint32_t vk);
    [[nodiscard]] std::uint32_t GetHideKey() const { return m_hideKey; }
    [[nodiscard]] bool GetCombatOnly() const { return m_combatOnly; }
    [[nodiscard]] bool GetManualHidden() const { return m_manualHidden; }
    [[nodiscard]] bool IsMasterVisible() const { return m_masterVisible.load(); }
    void UpdateCombatState(bool inCombat);
    [[nodiscard]] bool ShouldPollCombat();

    void SetWidgetVisible(const char* id, bool visible);
    void SetMenuSuppressed(bool on);
    void SetRuntimeVisible(const char* id, bool visible);

    std::string GetSlotsPayload() const;
    void LoadSlot(int index);
    void SaveSlot(int index, const char* name);
    void RenameSlot(int index, const char* name);

    std::string GetIconsPayload() const;
    void SetWidgetIcon(const char* id, const char* file);
    void SetIconCapable(const char* id);

    std::string GetPresetsPayload() const;
    void ApplyPreset(const char* name);

    std::string ExportLayout(const char* name);
    std::string ListImportFiles() const;
    void ImportLayout(const char* name);

    void SetAPI(PRISMA_UI_API::IVPrismaUI4* api) { m_api = api; }
    void ResetEditorListeners() { m_editorListenersRegistered = false; }
    bool EditorListenersRegistered() const { return m_editorListenersRegistered; }
    void SetEditorListenersRegistered() { m_editorListenersRegistered = true; }

private:
    HUDLayoutManager() = default;

    void ApplyToView(const WidgetEntry& e) const;
    void PushIconToView(const WidgetEntry& e) const;
    void RecoverOffscreen(WidgetEntry& e, nlohmann::json& saved) const;
    void ApplyEffectiveVisibility(const std::string& id, bool preserveIfRuntimeUnknown = false);
    void LoadLayoutFile();
    void SaveLayoutFile() const;
    void SaveSlotsFile() const;
    void LoadSlotsFile();
    void SaveSettings() const;
    void RecomputeMaster();
    void ReapplyAllVisibilityLocked();
    std::filesystem::path LayoutPath() const;
    std::filesystem::path SlotsPath() const;
    std::filesystem::path SettingsPath() const;
    std::filesystem::path ExportsDir() const;

    struct Preset {
        const char* name;
        float scale;
    };
    static const Preset kPresets[];

    struct SlotData {
        std::string name;
        std::unordered_map<std::string, nlohmann::json> layout;
    };

    PRISMA_UI_API::IVPrismaUI4* m_api = nullptr;
    mutable std::shared_mutex m_lock;
    std::unordered_map<PrismaView, WidgetEntry> m_widgets;
    std::unordered_map<std::string, nlohmann::json> m_savedPos;
    std::unordered_map<std::string, bool> m_userVisible;
    std::unordered_set<std::string> m_iconCapable;
    std::unordered_map<std::string, bool> m_runtimeVisible;
    bool m_editorListenersRegistered = false;
    SlotData m_slots[4];

    std::uint32_t m_hideKey = 0x71;
    bool m_combatOnly = false;
    bool m_manualHidden = false;
    bool m_inCombat = false;
    std::atomic<bool> m_masterVisible{ true };
    std::atomic<bool> m_menuSuppressed{ false };
    std::uint64_t m_lastCombatTick = 0;
};
