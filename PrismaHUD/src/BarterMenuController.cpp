#include "BarterMenuController.h"

#include <RE/B/BarterMenu.h>
#include <RE/I/InventoryItemDisplayData.h>
#include <RE/M/MenuOpenCloseEvent.h>
#include <RE/U/UI.h>
#include <RE/U/UIMessageQueue.h>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <iterator>
#include <limits>
#include <nlohmann/json.hpp>
#include <optional>
#include <string_view>
#include <vector>

namespace
{
RE::BarterMenu* GetLiveBarterMenu()
{
    auto* ui = RE::UI::GetSingleton();
    if (!ui) return nullptr;
    auto menu = ui->GetMenu<RE::BarterMenu>();
    if (!menu || !menu->OnStack()) return nullptr;
    return menu.get();
}

std::string FormName(RE::TESForm* form, const char* fallback)
{
    if (!form) return fallback;
    try {
        const auto name = RE::TESFullName::GetFullName(*form);
        if (!name.empty()) return std::string(name);
    } catch (...) {}
    return fallback;
}

const char* CategoryName(std::uint32_t flag)
{
    if (flag & 2u) return "WEAPONS";
    if (flag & 4u) return "APPAREL";
    if (flag & 8u) return "AID";
    if (flag & 1024u) return "JUNK";
    if (flag & 2048u) return "MODS";
    if (flag & 4096u) return "AMMO";
    if (flag & 8192u) return "HOLOTAPES";
    if (flag & 128u) return "NOTES";
    if (flag & 512u) return "MISC";
    return "OTHER";
}

bool VanillaTradeIsValid(RE::BarterMenu& menu)
{
    try {
        if (!menu.menuObj.IsObject()) return false;
        Scaleform::GFx::Value value;
        return menu.menuObj.GetMember("isValidTrade", std::addressof(value)) &&
               value.IsBoolean() && value.GetBoolean();
    } catch (...) {
        return false;
    }
}

struct VanillaQuantityState
{
    std::uint32_t total{ 0 };
    std::uint32_t queued{ 0 };
    std::uint32_t remaining{ 0 };
};

std::string StackKey(const RE::InventoryUserUIInterfaceEntry& entry)
{
    std::string out;
    char buffer[8]{};
    for (const auto value : entry.stackIndex) {
        if (!out.empty()) out.push_back('.');
        const auto [end, ec] = std::to_chars(
            std::begin(buffer), std::end(buffer), static_cast<unsigned int>(value));
        if (ec != std::errc{}) return {};
        out.append(buffer, end);
    }
    return out;
}

bool GfxUInt(const Scaleform::GFx::Value& value, std::uint32_t& out)
{
    if (value.IsUInt()) {
        out = value.GetUInt();
        return true;
    }
    if (value.IsInt()) {
        const auto signedValue = value.GetInt();
        if (signedValue < 0) return false;
        out = static_cast<std::uint32_t>(signedValue);
        return true;
    }
    if (value.IsNumber()) {
        const double number = value.GetNumber();
        if (!std::isfinite(number) || number < 0.0 ||
            number > static_cast<double>(std::numeric_limits<std::uint32_t>::max()) ||
            std::floor(number) != number) {
            return false;
        }
        out = static_cast<std::uint32_t>(number);
        return true;
    }
    return false;
}

bool GfxUIntMember(const Scaleform::GFx::Value& object, const char* name, std::uint32_t& out)
{
    if (!object.IsObject()) return false;
    Scaleform::GFx::Value value;
    return object.GetMember(name, std::addressof(value)) && GfxUInt(value, out);
}

std::optional<VanillaQuantityState> ReadVanillaQuantity(
    RE::BarterMenu& menu,
    bool fromContainer,
    std::uint32_t index,
    std::uint32_t expectedHandle)
{
    if (!menu.menuObj.IsObject()) return std::nullopt;

    Scaleform::GFx::Value rows;
    const char* member = fromContainer ? "containerListArray" : "playerListArray";
    if (!menu.menuObj.GetMember(member, std::addressof(rows)) || !rows.IsArray() ||
        index >= rows.GetArraySize()) {
        return std::nullopt;
    }

    Scaleform::GFx::Value row;
    if (!rows.GetElement(index, std::addressof(row)) || !row.IsObject()) return std::nullopt;

    std::uint32_t liveHandle = 0;
    std::uint32_t total = 0;
    if (!GfxUIntMember(row, "handle", liveHandle) || liveHandle != expectedHandle ||
        !GfxUIntMember(row, "count", total)) {
        return std::nullopt;
    }

    std::uint32_t queued = 0;
    if (row.HasMember("barterCount") && !GfxUIntMember(row, "barterCount", queued)) {
        return std::nullopt;
    }
    if (queued > total) return std::nullopt;

    return VanillaQuantityState{ total, queued, total - queued };
}

nlohmann::json BuildRows(RE::BarterMenu& menu, bool fromContainer)
{
    auto& inventory = fromContainer ? menu.containerInv : menu.playerInv;
    nlohmann::json rows = nlohmann::json::array();
    rows.get_ref<nlohmann::json::array_t&>().reserve(inventory.stackedEntries.size());

    for (std::uint32_t index = 0; index < inventory.stackedEntries.size(); ++index) {
        const auto& entry = inventory.stackedEntries[index];
        try {
            RE::InventoryItemDisplayData display(inventory.inventoryRef, entry);
            const char* rawName = display.itemName.c_str();
            const auto quantity = ReadVanillaQuantity(menu, fromContainer, index, entry.invHandle.id);
            const std::uint32_t total = quantity ? quantity->total : display.itemCount;
            const std::uint32_t queued = quantity ? quantity->queued : 0u;
            const std::uint32_t remaining = quantity ? quantity->remaining : (display.itemCount ? 1u : 0u);

            rows.push_back({
                { "index", index },
                { "handle", entry.invHandle.id },
                { "stackKey", StackKey(entry) },
                { "name", rawName && rawName[0] ? rawName : "?" },
                { "count", total },
                { "barterCount", queued },
                { "remaining", remaining },
                { "quantityKnown", quantity.has_value() },
                { "filterFlag", display.filterFlag },
                { "category", CategoryName(display.filterFlag) },
                { "value", menu.GetItemValue(index, fromContainer) },
                { "legendary", display.isLegendary },
                { "favorite", display.isFavorite },
                { "tagged", display.isTaggedForSearch },
            });
        } catch (const std::exception& e) {
            logger::warn("PrismaHUD barter: skipping {} row {} (handle={}) -- {}",
                         fromContainer ? "vendor" : "player", index, entry.invHandle.id, e.what());
        } catch (...) {
            logger::warn("PrismaHUD barter: skipping {} row {} (handle={}) -- unknown exception",
                         fromContainer ? "vendor" : "player", index, entry.invHandle.id);
        }
    }
    return rows;
}

bool ParseUInt(std::string_view text, std::uint32_t& value)
{
    value = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    return result.ec == std::errc{} && result.ptr == text.data() + text.size();
}

struct StableBarterRow
{
    std::uint32_t handle{ 0 };
    std::string stackKey;
    std::uint32_t requested{ 0 };
};

bool ResolveStableBarterRow(
    RE::BarterMenu& menu,
    bool fromContainer,
    const StableBarterRow& wanted,
    std::uint32_t& outIndex,
    std::uint32_t& outRemaining)
{
    auto& inventory = fromContainer ? menu.containerInv : menu.playerInv;
    for (std::uint32_t index = 0; index < inventory.stackedEntries.size(); ++index) {
        const auto& entry = inventory.stackedEntries[index];
        if (entry.invHandle.id != wanted.handle || StackKey(entry) != wanted.stackKey) continue;
        const auto quantity = ReadVanillaQuantity(menu, fromContainer, index, wanted.handle);
        if (!quantity || quantity->remaining == 0) return false;
        outIndex = index;
        outRemaining = quantity->remaining;
        return true;
    }
    return false;
}

class BarterMenuSink final : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
{
public:
    static BarterMenuSink* GetSingleton()
    {
        static BarterMenuSink sink;
        return std::addressof(sink);
    }

    RE::BSEventNotifyControl ProcessEvent(
        const RE::MenuOpenCloseEvent& event,
        RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
    {
        if (event.menuName == RE::BarterMenu::MENU_NAME) {
            BarterMenuController::GetSingleton()->OnRawMenuEvent(event.opening);
        }
        return RE::BSEventNotifyControl::kContinue;
    }
};
}

BarterMenuController* BarterMenuController::GetSingleton()
{
    static BarterMenuController controller;
    return std::addressof(controller);
}

void BarterMenuController::Initialize(PRISMA_UI_API::IVPrismaUI11* api)
{
    if (!api) return;
    m_api = api;
    InstallMenuSink();
}

void BarterMenuController::InstallMenuSink()
{
    if (m_sinkInstalled || !m_api) return;
    auto* ui = RE::UI::GetSingleton();
    if (!ui) {
        logger::warn("PrismaHUD barter: RE::UI unavailable; vanilla BarterMenu remains active");
        return;
    }
    ui->RegisterSink(BarterMenuSink::GetSingleton());
    m_sinkInstalled = true;
    logger::info("PrismaHUD: BarterMenu replacement sink registered");
}

void BarterMenuController::DispatchOpen(void* userdata)
{
    static_cast<BarterMenuController*>(userdata)->OnMenuOpenVerified();
}

void BarterMenuController::DispatchClose(void* userdata)
{
    static_cast<BarterMenuController*>(userdata)->OnMenuCloseVerified();
}

void BarterMenuController::OnRawMenuEvent(bool opening)
{
    m_openRequested.store(opening);
    auto* api = m_api;
    if (!api) return;
    const auto callback = opening ? &BarterMenuController::DispatchOpen
                                  : &BarterMenuController::DispatchClose;
    if (!api->DispatchToGameThread(callback, this)) {
        logger::warn("PrismaHUD barter: verified {} dispatch rejected; vanilla remains authoritative",
                     opening ? "open" : "close");
    }
}

void BarterMenuController::RestoreVanilla()
{
    if (auto* menu = GetLiveBarterMenu(); menu && menu->uiMovie) {
        menu->uiMovie->SetVisible(true);
    }
}

void BarterMenuController::HideReplacement()
{
    if (m_api && m_view && m_api->IsValid(m_view)) {
        m_api->Unfocus(m_view);
        m_api->Hide(m_view);
    }
    m_active = false;
}

bool BarterMenuController::ActivateReplacement()
{
    auto* menu = GetLiveBarterMenu();
    if (!menu || !m_api || !m_openRequested.load()) return false;
    RestoreVanilla();

    if (m_view && !m_api->IsValid(m_view)) {
        m_view = 0;
        m_domReady = false;
        m_active = false;
    }

    if (!m_view) {
        try {
            m_view = m_api->CreateView("hud_barter.html", &BarterMenuController::DomReadyCallback);
            if (!m_view) {
                logger::error("PrismaHUD barter: CreateView returned 0; keeping vanilla BarterMenu");
                return false;
            }
            m_api->SetViewRole(m_view, PRISMA_UI_API::ViewRole::kPanel);
            m_api->SetViewOwnsEscape(m_view, true);
            m_api->SetOrder(m_view, 1'000'000);
            m_api->Hide(m_view);
        } catch (const std::exception& e) {
            logger::error("PrismaHUD barter: view creation failed: {}", e.what());
            m_view = 0;
            return false;
        }
    }

    if (!m_domReady) return true;

    PushState();
    m_api->Show(m_view);
    if (!m_api->FocusOverlay(m_view, false, false)) {
        logger::error("PrismaHUD barter: Prisma focus failed; restoring vanilla BarterMenu");
        RestoreVanilla();
        m_api->Hide(m_view);
        m_active = false;
        return false;
    }

    menu = GetLiveBarterMenu();
    if (!m_openRequested.load() || !menu) {
        HideReplacement();
        RestoreVanilla();
        return false;
    }

    if (menu->uiMovie) menu->uiMovie->SetVisible(false);
    m_active = true;
    logger::info("PrismaHUD: Prisma barter view took over live BarterMenu");
    return true;
}

void BarterMenuController::OnMenuOpenVerified()
{
    if (!m_api || !m_api->IsGameThread()) {
        logger::error("PrismaHUD barter: open reached unverified thread; keeping vanilla BarterMenu");
        return;
    }
    (void)ActivateReplacement();
}

void BarterMenuController::OnMenuCloseVerified()
{
    if (!m_api || !m_api->IsGameThread()) return;
    RestoreVanilla();
    HideReplacement();
}

void BarterMenuController::DomReadyCallback(PrismaView view)
{
    GetSingleton()->OnDomReady(view);
}

void BarterMenuController::ActionEvent(const char* argument, void* userdata)
{
    static_cast<BarterMenuController*>(userdata)->OnAction(argument);
}

void BarterMenuController::TickEvent(const char*, void* userdata)
{
    static_cast<BarterMenuController*>(userdata)->OnTick();
}

void BarterMenuController::OnDomReady(PrismaView view)
{
    if (!m_api || !view || view != m_view || !m_api->IsValid(view)) return;

    const bool actionBound = m_api->BindGameThreadUIEvent(
        view, "barterAction", &BarterMenuController::ActionEvent, this);
    const bool tickBound = m_api->BindGameThreadUIEvent(
        view, "barterTick", &BarterMenuController::TickEvent, this);
    if (!actionBound || !tickBound) {
        logger::error("PrismaHUD barter: V11 UI bindings failed; keeping vanilla BarterMenu");
        RestoreVanilla();
        m_api->Hide(view);
        m_domReady = false;
        return;
    }

    m_domReady = true;
    if (m_openRequested.load()) (void)ActivateReplacement();
}

void BarterMenuController::OnTick()
{
    if (!m_api || !m_api->IsGameThread()) return;
    if (!m_openRequested.load() || !GetLiveBarterMenu()) {
        RestoreVanilla();
        HideReplacement();
    }
}

void BarterMenuController::PushState()
{
    auto* menu = GetLiveBarterMenu();
    if (!menu || !m_api || !m_view || !m_api->IsValid(m_view)) return;

    try {
        nlohmann::json state{
            { "active", true },
            { "playerName", FormName(RE::PlayerCharacter::GetSingleton(), "PLAYER") },
            { "vendorName", [&]() {
                const auto vendor = menu->vendorActor.get();
                return FormName(vendor.get(), "VENDOR");
            }() },
            { "capsOwed", menu->GetCapsOwedByPlayer() },
            { "validTrade", VanillaTradeIsValid(*menu) },
            { "player", BuildRows(*menu, false) },
            { "vendor", BuildRows(*menu, true) },
        };
        const auto payload = state.dump();
        m_api->InteropCall(m_view, "setBarter", payload.c_str());
    } catch (const std::exception& e) {
        logger::error("PrismaHUD barter: state build failed: {}; restoring vanilla", e.what());
        RestoreVanilla();
        HideReplacement();
    } catch (...) {
        logger::error("PrismaHUD barter: state build failed; restoring vanilla");
        RestoreVanilla();
        HideReplacement();
    }
}

void BarterMenuController::QueueCategory(bool fromContainer, std::uint32_t filterMask)
{
    auto* menu = GetLiveBarterMenu();
    if (!menu || !m_active || !m_openRequested.load()) return;

    auto& inventory = fromContainer ? menu->containerInv : menu->playerInv;
    std::vector<StableBarterRow> plan;
    plan.reserve(inventory.stackedEntries.size());

    for (std::uint32_t index = 0; index < inventory.stackedEntries.size(); ++index) {
        const auto& entry = inventory.stackedEntries[index];
        try {
            RE::InventoryItemDisplayData display(inventory.inventoryRef, entry);
            if (filterMask != 0xFFFFFFFFu && (display.filterFlag & filterMask) == 0) continue;
            if (!fromContainer && display.isFavorite) continue;
            const auto quantity = ReadVanillaQuantity(*menu, fromContainer, index, entry.invHandle.id);
            if (!quantity || quantity->remaining == 0) continue;
            plan.push_back({ entry.invHandle.id, StackKey(entry), quantity->remaining });
        } catch (...) {}
    }

    for (const auto& wanted : plan) {
        menu = GetLiveBarterMenu();
        if (!menu || !m_openRequested.load()) break;
        std::uint32_t liveIndex = 0;
        std::uint32_t liveRemaining = 0;
        if (!ResolveStableBarterRow(*menu, fromContainer, wanted, liveIndex, liveRemaining)) continue;
        const auto count = std::min(wanted.requested, liveRemaining);
        if (count == 0) continue;
        menu->DoItemTransfer(liveIndex, count, fromContainer);
    }
    PushState();
}

void BarterMenuController::OnAction(const char* raw)
{
    if (!raw || !raw[0] || !m_api || !m_api->IsGameThread()) return;
    auto* menu = GetLiveBarterMenu();
    if (!menu || !m_active || !m_openRequested.load()) {
        RestoreVanilla();
        HideReplacement();
        return;
    }

    const std::string_view action{ raw };
    if (action == "reset") {
        menu->ClearTradingData();
        PushState();
        return;
    }
    if (action == "accept") {
        if (!VanillaTradeIsValid(*menu)) {
            PushState();
            return;
        }
        menu->CompleteTrade();
        PushState();
        return;
    }
    if (action == "close") {
        menu->ClearTradingData();
        m_openRequested.store(false);
        RestoreVanilla();
        HideReplacement();
        if (auto* queue = RE::UIMessageQueue::GetSingleton()) {
            queue->AddMessage(RE::BarterMenu::MENU_NAME, RE::UI_MESSAGE_TYPE::kHide);
        }
        return;
    }

    constexpr std::string_view bulkPrefix = "bulk|";
    if (action.starts_with(bulkPrefix)) {
        const auto rest = action.substr(bulkPrefix.size());
        const auto split = rest.find('|');
        if (split == std::string_view::npos) return;
        const auto side = rest.substr(0, split);
        const bool fromContainer = side == "vendor";
        if (!fromContainer && side != "player") return;
        std::uint32_t mask = 0;
        if (!ParseUInt(rest.substr(split + 1), mask)) return;
        QueueCategory(fromContainer, mask);
        return;
    }

    constexpr std::string_view prefix = "transfer|";
    if (!action.starts_with(prefix)) {
        logger::warn("PrismaHUD barter: unknown UI action '{}'", raw);
        return;
    }

    const auto rest = action.substr(prefix.size());
    const auto p1 = rest.find('|');
    const auto p2 = p1 == std::string_view::npos ? p1 : rest.find('|', p1 + 1);
    const auto p3 = p2 == std::string_view::npos ? p2 : rest.find('|', p2 + 1);
    const auto p4 = p3 == std::string_view::npos ? p3 : rest.find('|', p3 + 1);
    if (p1 == std::string_view::npos || p2 == std::string_view::npos ||
        p3 == std::string_view::npos || p4 == std::string_view::npos ||
        rest.find('|', p4 + 1) != std::string_view::npos) {
        logger::warn("PrismaHUD barter: malformed transfer action '{}'", raw);
        return;
    }

    const auto side = rest.substr(0, p1);
    const bool fromContainer = side == "vendor";
    if (!fromContainer && side != "player") return;

    std::uint32_t index = 0;
    std::uint32_t expectedHandle = 0;
    std::uint32_t requestedCount = 0;
    const std::string expectedStackKey(rest.substr(p3 + 1, p4 - p3 - 1));
    if (!ParseUInt(rest.substr(p1 + 1, p2 - p1 - 1), index) ||
        !ParseUInt(rest.substr(p2 + 1, p3 - p2 - 1), expectedHandle) ||
        !ParseUInt(rest.substr(p4 + 1), requestedCount) || requestedCount == 0) {
        return;
    }

    auto& inventory = fromContainer ? menu->containerInv : menu->playerInv;
    if (index >= inventory.stackedEntries.size()) {
        logger::warn("PrismaHUD barter: rejected stale {} row index={}",
                     fromContainer ? "vendor" : "player", index);
        PushState();
        return;
    }

    const auto& liveEntry = inventory.stackedEntries[index];
    if (liveEntry.invHandle.id != expectedHandle || StackKey(liveEntry) != expectedStackKey) {
        logger::warn("PrismaHUD barter: rejected stale {} row index={} handle={} stack='{}'",
                     fromContainer ? "vendor" : "player", index, expectedHandle, expectedStackKey);
        PushState();
        return;
    }

    const auto quantity = ReadVanillaQuantity(*menu, fromContainer, index, expectedHandle);
    if (quantity) {
        if (quantity->remaining == 0 || requestedCount > quantity->remaining) {
            logger::warn("PrismaHUD barter: rejected {} count={} with only {} remaining",
                         fromContainer ? "buy" : "sell", requestedCount, quantity->remaining);
            PushState();
            return;
        }
    } else if (requestedCount != 1) {
        logger::warn("PrismaHUD barter: live quantity unavailable; rejecting bulk count={}", requestedCount);
        PushState();
        return;
    }

    menu->DoItemTransfer(index, requestedCount, fromContainer);
    PushState();
}
