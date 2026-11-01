/*
 * PrismaHUD_F4_API.h
 * For modders: copy this file into your plugin's src/ to register your PrismaUI
 * views as HUD widgets so users can position them with the in-game HUD editor.
 *
 * Timing: call RegisterWidget() inside kPostLoadGame / kNewGame, after you have
 * called CreateView() and stored the PrismaView handle.
 *
 * Your view's HTML must expose window.setHUDPosition(json) to receive layout
 * updates. PrismaHUD passes a JSON string shaped like:
 * {"x":0.0156,"y":0.8167,"s":1.0,"r":0.0}
 * x/y are viewport fractions, s is uniform scale, and r is rotation in degrees
 * (0/90/180/270). The widget's intrinsic width/height come from
 * WidgetDesc::defaultW/defaultH. Apply it however suits your view.
 */
#pragma once

#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
    #define NOMINMAX
#endif

#include <Windows.h>
#include <stdint.h>

typedef uint64_t PrismaView;

namespace PRISMA_HUD_API {

    constexpr const auto PrismaHUDPluginName = "PrismaHUD_F4";

    // V1 = HUD widget positioning (IVPrismaHUD1).
    // V2 = shared keybind registry (IVPrismaHUD2) — one place that owns every global hotkey across
    //      PrismaHUD / PrismaWheelMenu / WastelandHunter, so bindings are user-rebindable, persisted,
    //      conflict-checked, and centrally suppressible (fixes the fragmented-input class of bug).
    // V3 = activate-choice registry (IVPrismaHUD3) — mod-agnostic opt-in for surfacing a custom
    //      perk/activate button in the rollover, for the cases the engine's single perkButtonText
    //      slot can't cover (a second button on one target, or a custom-UI mod with no Activate perk).
    enum class InterfaceVersion : uint8_t { V1, V2, V3 };

    struct WidgetDesc {
        const char* id;           // Stable identifier used as the key in layout JSON (e.g. "prisma_inventory")
        const char* displayName;  // Human-readable label shown in the editor (e.g. "Inventory")
        // Default position/size as a FRACTION of the viewport (value / 1920 or / 1080), NOT pixels.
        // The editor multiplies these by the live viewport, so raw pixels put the drag box off-screen
        // (e.g. 30 -> 30*1920). Pass e.g. 30.0f/1920.0f for "30px from the left on a 1920-wide screen".
        float       defaultX;     // 0.0 = left edge, 1.0 = right edge
        float       defaultY;     // 0.0 = top edge,  1.0 = bottom edge
        float       defaultW;     // fraction of viewport width
        float       defaultH;     // fraction of viewport height
    };

    class IVPrismaHUD1 {
    protected:
        ~IVPrismaHUD1() = default;

    public:
        // Register a PrismaUI view as a positionable HUD widget.
        // Call on kPostLoadGame / kNewGame after CreateView(). Safe to call again on reload —
        // re-registering the same id with a new view handle replaces the old entry. PrismaHUD always
        // pushes setHUDPosition after registration. Optional JS feedback hooks are also bound:
        //   reportPosition({id,x,y,s,r})      updates the manager's observed position
        //   reportVisibility({id,visible})    updates transient auto-hide state
        // A view that does not need either hook may simply never call them.
        virtual void RegisterWidget(PrismaView view, WidgetDesc desc) noexcept = 0;

        // Remove a widget from the registry (e.g. when your view is destroyed).
        virtual void UnregisterWidget(PrismaView view) noexcept = 0;

        // Force-apply the current saved layout to a single registered view.
        // Useful after recreating a view on reload.
        virtual void ApplyLayout(PrismaView view) noexcept = 0;
    };

    // ── V2: shared keybind registry ───────────────────────────────────────────────────────────────
    // Plain C function pointer (NOT std::function) so the callback is ABI-safe across the DLL boundary.
    // Invoked on the game thread when the action's CURRENT (possibly-rebound) key is pressed/released.
    typedef void (*BindingCallback)(void* context);

    struct BindingDesc {
        const char* id;           // Stable identifier, key in keybinds.json (e.g. "wheel_open")
        const char* displayName;  // Human label shown in the keybind menu (e.g. "Open Wheel Menu")
        const char* category;     // Grouping label in the menu (e.g. "Wheel Menu")
        uint32_t    defaultKey;   // Default Virtual-Key code (this fork's ButtonEvent idCode is a VK)
        bool        requireShift; // Only fire when Shift is held (checked at dispatch time)
        bool        consume;      // Consume the key event (kStop) so other input users don't also fire
        bool        suppressible; // If true, suppressed while an exclusive input mode is active
                                  // (e.g. the wheel open key is suppressed during WH camp placement)
    };

    class IVPrismaHUD2 {
    protected:
        ~IVPrismaHUD2() = default;

    public:
        // Register (or replace) a global hotkey action. The registry immediately binds it to the
        // persisted key (from keybinds.json) or the default, and invokes onDown/onUp when that key
        // fires. Pass nullptr for onUp if the action has no release gesture. Idempotent by id —
        // re-registering the same id (e.g. on game reload) replaces the callbacks, keeping the key.
        virtual void RegisterBinding(BindingDesc desc, BindingCallback onDown, void* downContext,
                                     BindingCallback onUp, void* upContext) noexcept = 0;

        // Remove an action from the registry (unbinds its key).
        virtual void UnregisterBinding(const char* id) noexcept = 0;

        // Current Virtual-Key bound to an action id (0 if the id is unknown).
        virtual uint32_t GetBoundKey(const char* id) noexcept = 0;

        // True while ANY caller currently holds exclusive input mode. The automatic per-binding dispatch
        // gate (suppressible=true on RegisterBinding) is a blunt instrument: it suppresses BOTH halves of
        // a toggle action registered on one key (e.g. "press to open, press again to close"), which is
        // wrong -- closing/cancelling an already-active state of your OWN must never be blocked by
        // exclusivity, only STARTING something new should be. A suppressible consumer whose single key
        // does double duty should register with suppressible=false and call this explicitly, ONLY in its
        // "would this open/start something new" branch, leaving its own close/cancel branch ungated.
        virtual bool IsExclusiveModeActive() const noexcept = 0;

        // Enter/leave an EXCLUSIVE input mode: while active, every action flagged suppressible is not
        // dispatched. Used so a modal-ish gameplay mode (WH camp placement, which reuses Q/E/R/F; the
        // PrismaHUD quick-loot rollover, a custom overlay RE::UI can't see) doesn't also trigger global
        // hotkeys like the wheel open. REFERENCE-COUNTED internally: multiple independent callers can each
        // hold it on at once (e.g. camp placement AND the rollover simultaneously) without one caller's
        // "off" prematurely clearing another's still-active "on". Every true MUST be paired with exactly
        // one false by each caller — do not call true twice in a row without an intervening false.
        virtual void SetExclusiveInputMode(bool on) noexcept = 0;
    };

    // ── V3: activate-choice registry ────────────────────────────────────────────────────────────
    // Plain C function pointer (ABI-safe across the DLL boundary). Invoked on the game thread when the
    // player selects the choice (its number key in the quick-loot panel, or a mouse click).
    typedef void (*ActivateChoiceCallback)(void* context);

    class IVPrismaHUD3 {
    protected:
        ~IVPrismaHUD3() = default;

    public:
        // Register (or replace) a generic activate choice surfaced in the PrismaHUD rollover (the
        // quick-loot panel and the centered interaction prompt). Mod-agnostic, exactly like
        // RegisterWidget — PrismaHUD never inspects who you are or what kind of mod you are:
        //
        //   id         Stable identifier; re-registering the same id replaces the entry in place
        //              (keeps its row order). Also the UnregisterActivateChoice key.
        //   label      Text shown on the choice row (e.g. "Skin Animal", "Harvest").
        //   iconId     Optional icon path, relative to the PrismaUI views root and INCLUDING the
        //              extension (e.g. "icons/game-icons/andymeneely/riposte.svg", or your own
        //              "Interface/MyMod/icon.png"). Rendered verbatim as an <img src>. "" = text-only
        //              row. (A bare id is not resolved -- game-icons are nested by author, so pass the
        //              full relative path.)
        //   onTrigger  Called on the game thread when the player selects this choice; may itself
        //              register/unregister choices. Required (nullptr registrations are ignored).
        //   context    Opaque pointer passed back to onTrigger.
        //
        // Visibility is entirely the mod's responsibility via the register/unregister lifecycle:
        // register when your choice applies (e.g. you detect a valid target), UnregisterActivateChoice
        // when it does not. Choices render in registration order, badged with their 1-based row number,
        // which is also the number key that triggers them while the quick-loot panel is up.
        virtual void RegisterActivateChoice(const char* id, const char* label, const char* iconId,
                                            ActivateChoiceCallback onTrigger, void* context) noexcept = 0;

        // Remove a registered choice by id (no-op if unknown).
        virtual void UnregisterActivateChoice(const char* id) noexcept = 0;
    };

    typedef void* (*RequestPluginAPIFunc)(InterfaceVersion interfaceVersion);

    [[nodiscard]] inline IVPrismaHUD1* RequestPluginAPI() {
        auto pluginHandle = GetModuleHandleW(L"PrismaHUD_F4.dll");
        if (!pluginHandle)
            return nullptr;

        auto fn = reinterpret_cast<RequestPluginAPIFunc>(
            GetProcAddress(pluginHandle, "RequestPrismaHUDAPI"));
        if (!fn)
            return nullptr;

        return static_cast<IVPrismaHUD1*>(fn(InterfaceVersion::V1));
    }

    // Fetch the V2 keybind registry. Returns nullptr if PrismaHUD_F4.dll is absent or too old (pre-V2).
    [[nodiscard]] inline IVPrismaHUD2* RequestBindingsAPI() {
        auto pluginHandle = GetModuleHandleW(L"PrismaHUD_F4.dll");
        if (!pluginHandle)
            return nullptr;

        auto fn = reinterpret_cast<RequestPluginAPIFunc>(
            GetProcAddress(pluginHandle, "RequestPrismaHUDAPI"));
        if (!fn)
            return nullptr;

        return static_cast<IVPrismaHUD2*>(fn(InterfaceVersion::V2));
    }

    // Fetch the V3 activate-choice registry. Returns nullptr if PrismaHUD_F4.dll is absent or too old
    // (pre-V3). Idempotent to call repeatedly — always returns the same singleton.
    [[nodiscard]] inline IVPrismaHUD3* RequestActivateChoicesAPI() {
        auto pluginHandle = GetModuleHandleW(L"PrismaHUD_F4.dll");
        if (!pluginHandle)
            return nullptr;

        auto fn = reinterpret_cast<RequestPluginAPIFunc>(
            GetProcAddress(pluginHandle, "RequestPrismaHUDAPI"));
        if (!fn)
            return nullptr;

        return static_cast<IVPrismaHUD3*>(fn(InterfaceVersion::V3));
    }

} // namespace PRISMA_HUD_API
