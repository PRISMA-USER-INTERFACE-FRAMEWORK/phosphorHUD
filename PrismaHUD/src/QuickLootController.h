#pragma once
#include "PrismaUI_F4_API.h"

class QuickLootController {
public:
    static QuickLootController* GetSingleton();

    void SetAPI(PRISMA_UI_API::IVPrismaUI8* api) { m_api = api; }
    void SetView(PrismaView view)                { m_view = view; }
    PrismaView GetView() const                   { return m_view; }

    bool IsActive();

    bool Active() const { return m_active; }

    bool AnyViewFocused() const { return m_api && m_api->HasAnyActiveFocus(); }

    bool IsWorkshopBenchTarget() const { return m_isWorkshopBench; }

    static std::string EnginePerkButtonLabel();

    static bool HasEnginePerkButton();

    bool LivePickIsCurrentTarget() const {
        auto pick = m_cachedPick.get();
        auto cont = m_container.get();
        if (!cont) return false;
        if (!pick) return true;
        return pick.get() == cont.get();
    }

    void OnRequestUpdate();

    void MoveSelection(int delta);
    void TakeSelected();
    void TakeAll();

    bool TriggerActivateChoice(std::uint32_t buttonIndex);

    static void InstallQuickContainerTakeHook();

    void InstallMenuSink();

    bool ShouldSuppressContainer() const;

    void OpenFullContainer();

    void ToggleUserHidden();
    bool UserHidden() const { return m_userHidden; }

    void ResetTransientForLoad();

    void InstallPickRefSink();

    void SetCachedPick(RE::ObjectRefHandle a_handle) { m_cachedPick = a_handle; }

    RE::ObjectRefHandle CurrentCrosshairPick() const { return m_cachedPick; }

private:
    QuickLootController() = default;

    struct LootEntry {
        RE::TESBoundObject* object = nullptr;
        std::int32_t        count  = 0;
    };

    RE::TESObjectREFR* ResolveTarget();

    RE::TESObjectREFR* CurrentContainer();

    void BuildSnapshot(RE::TESObjectREFR* container);

    void PushToView();

    void PushHidden();

    void SetExclusive(bool on);

    PRISMA_UI_API::IVPrismaUI8* m_api  = nullptr;
    PrismaView                  m_view = 0;

    RE::ObjectRefHandle      m_cachedPick;
    RE::ObjectRefHandle      m_container;
    std::vector<LootEntry>   m_items;
    int                      m_sel  = 0;
    std::string              m_json;
    bool                     m_wasVisible = false;
    bool                     m_active     = false;
    bool                     m_isWorkshopBench = false;
    bool                     m_userHidden = false;
    bool                     m_exclusiveActive = false;
    mutable bool             m_allowFullOpen = false;
};

class QuickLootInput : public RE::PlayerInputHandler {
public:
    explicit QuickLootInput(RE::PlayerControlsData& a_data) : RE::PlayerInputHandler(a_data) {}

    bool ShouldHandleEvent(const RE::InputEvent* a_event) override;
    void OnButtonEvent(const RE::ButtonEvent* a_event) override;

    static void Install();

private:
    bool m_tookAll = false;
    bool m_activateHeld = false;
};
