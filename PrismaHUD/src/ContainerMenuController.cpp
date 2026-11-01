#include "ContainerMenuController.h"

#include <RE/C/ContainerMenu.h>
#include <RE/I/InventoryItemDisplayData.h>
#include <RE/M/MenuOpenCloseEvent.h>
#include <RE/U/UI.h>
#include <RE/U/UIMessageQueue.h>

#include <algorithm>
#include <charconv>
#include <iterator>
#include <nlohmann/json.hpp>
#include <string_view>
#include <vector>

namespace
{
RE::ContainerMenu* GetLiveContainerMenu()
{
    auto* ui = RE::UI::GetSingleton();
    if (!ui) return nullptr;
    auto menu = ui->GetMenu<RE::ContainerMenu>();
    if (!menu || !menu->OnStack() || !menu->uiMovie) return nullptr;
    return menu.get();
}

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

bool ParseUInt(std::string_view text, std::uint32_t& value)
{
    value = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    return result.ec == std::errc{} && result.ptr == text.data() + text.size();
}

nlohmann::json BuildRows(RE::ContainerMenu& menu, bool fromContainer)
{
    auto& inventory = fromContainer ? menu.containerInv : menu.playerInv;
    nlohmann::json rows = nlohmann::json::array();
    rows.get_ref<nlohmann::json::array_t&>().reserve(inventory.stackedEntries.size());

    for (std::uint32_t index = 0; index < inventory.stackedEntries.size(); ++index) {
        const auto& entry = inventory.stackedEntries[index];
        try {
            RE::InventoryItemDisplayData display(inventory.inventoryRef, entry);
            const char* rawName = display.itemName.c_str();
            rows.push_back({
                { "index", index },
                { "handle", entry.invHandle.id },
                { "stackKey", StackKey(entry) },
                { "name", rawName && rawName[0] ? rawName : "?" },
                { "count", display.itemCount },
                { "filterFlag", display.filterFlag },
                { "category", CategoryName(display.filterFlag) },
                { "equipState", display.equipState },
                { "favorite", display.isFavorite },
                { "legendary", display.isLegendary },
                { "tagged", display.isTaggedForSearch },
                { "value", menu.GetItemValue(index, fromContainer) },
                { "canEquip", menu.GetCanEquipItem(index, fromContainer) },
                { "equipped", menu.GetIsItemEquipped(index, fromContainer) },
            });
        } catch (const std::exception& e) {
            logger::warn("PrismaHUD container: skipping {} row {} -- {}",
                         fromContainer ? "container" : "player", index, e.what());
        } catch (...) {
            logger::warn("PrismaHUD container: skipping {} row {} -- unknown exception",
                         fromContainer ? "container" : "player", index);
        }
    }
    return rows;
}

struct StableTransferRow
{
    std::uint32_t handle{ 0 };
    std::string stackKey;
    std::uint32_t count{ 0 };
};

bool ResolveStableRow(
    RE::ContainerMenu& menu,
    bool fromContainer,
    const StableTransferRow& wanted,
    std::uint32_t& outIndex,
    std::uint32_t& outCount)
{
    auto& inventory = fromContainer ? menu.containerInv : menu.playerInv;
    for (std::uint32_t index = 0; index < inventory.stackedEntries.size(); ++index) {
        const auto& entry = inventory.stackedEntries[index];
        if (entry.invHandle.id != wanted.handle || StackKey(entry) != wanted.stackKey) continue;
        try {
            RE::InventoryItemDisplayData display(inventory.inventoryRef, entry);
            if (display.itemCount == 0) return false;
            outIndex = index;
            outCount = display.itemCount;
            return true;
        } catch (...) {
            return false;
        }
    }
    return false;
}

class ContainerMenuSink final : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
{
public:
    static ContainerMenuSink* GetSingleton()
    {
        static ContainerMenuSink sink;
        return std::addressof(sink);
    }

    RE::BSEventNotifyControl ProcessEvent(
        const RE::MenuOpenCloseEvent& event,
        RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
    {
        if (event.menuName == RE::ContainerMenu::MENU_NAME) {
            ContainerMenuController::GetSingleton()->OnRawMenuEvent(event.opening);
        }
        return RE::BSEventNotifyControl::kContinue;
    }
};
}

ContainerMenuController* ContainerMenuController::GetSingleton()
{
    static ContainerMenuController controller;
    return std::addressof(controller);
}

void ContainerMenuController::Initialize(PRISMA_UI_API::IVPrismaUI11* api)
{
    if (!api) return;
    m_api = api;
    InstallMenuSink();
}

void ContainerMenuController::InstallMenuSink()
{
    if (m_sinkInstalled || !m_api) return;
    auto* ui = RE::UI::GetSingleton();
    if (!ui) return;
    ui->RegisterSink(ContainerMenuSink::GetSingleton());
    m_sinkInstalled = true;
    logger::info("PrismaHUD: ContainerMenu replacement sink registered");
}

void ContainerMenuController::DispatchOpen(void* userdata)
{
    static_cast<ContainerMenuController*>(userdata)->OnMenuOpenVerified();
}

void ContainerMenuController::DispatchClose(void* userdata)
{
    static_cast<ContainerMenuController*>(userdata)->OnMenuCloseVerified();
}

void ContainerMenuController::OnRawMenuEvent(bool opening)
{
    m_openRequested.store(opening);
    auto* api = m_api;
    if (!api) return;
    const auto callback = opening ? &ContainerMenuController::DispatchOpen
                                  : &ContainerMenuController::DispatchClose;
    if (!api->DispatchToGameThread(callback, this)) {
        logger::warn("PrismaHUD container: verified {} dispatch rejected",
                     opening ? "open" : "close");
    }
}

void ContainerMenuController::RestoreVanilla()
{
    auto* ui = RE::UI::GetSingleton();
    if (!ui) return;
    auto menu = ui->GetMenu<RE::ContainerMenu>();
    if (menu && menu->uiMovie) menu->uiMovie->SetVisible(true);
}

void ContainerMenuController::HideReplacement()
{
    if (m_api && m_view && m_api->IsValid(m_view)) {
        m_api->Unfocus(m_view);
        m_api->Hide(m_view);
    }
    m_active = false;
}

bool ContainerMenuController::ActivateReplacement()
{
    auto* menu = GetLiveContainerMenu();
    if (!menu || !m_api || !m_openRequested.load()) return false;

    if (m_view && !m_api->IsValid(m_view)) {
        m_view = 0;
        m_domReady = false;
        m_active = false;
    }

    if (!m_view) {
        m_view = m_api->CreateView("hud_container.html", &ContainerMenuController::DomReadyCallback);
        if (!m_view) {
            logger::error("PrismaHUD container: CreateView failed; keeping vanilla");
            return false;
        }
        m_api->SetViewRole(m_view, PRISMA_UI_API::ViewRole::kPanel);
        m_api->SetViewOwnsEscape(m_view, true);
        m_api->SetOrder(m_view, 1'000'000);
        m_api->Hide(m_view);
    }

    if (!m_domReady) return true;

    PushState();
    m_api->Show(m_view);
    if (!m_api->FocusOverlay(m_view, false, false)) {
        m_api->Hide(m_view);
        RestoreVanilla();
        m_active = false;
        logger::error("PrismaHUD container: focus failed; keeping vanilla");
        return false;
    }

    menu = GetLiveContainerMenu();
    if (!m_openRequested.load() || !menu) {
        HideReplacement();
        RestoreVanilla();
        return false;
    }

    menu->uiMovie->SetVisible(false);
    m_active = true;
    return true;
}

void ContainerMenuController::OnMenuOpenVerified()
{
    if (!m_api || !m_api->IsGameThread()) return;

    auto* menu = GetLiveContainerMenu();
    if (!menu || !menu->uiMovie->GetVisible()) return;
    ActivateReplacement();
}

void ContainerMenuController::OnMenuCloseVerified()
{
    if (!m_api || !m_api->IsGameThread()) return;
    RestoreVanilla();
    HideReplacement();
}

void ContainerMenuController::DomReadyCallback(PrismaView view)
{
    GetSingleton()->OnDomReady(view);
}

void ContainerMenuController::ActionEvent(const char* argument, void* userdata)
{
    static_cast<ContainerMenuController*>(userdata)->OnAction(argument);
}

void ContainerMenuController::TickEvent(const char*, void* userdata)
{
    static_cast<ContainerMenuController*>(userdata)->OnTick();
}

void ContainerMenuController::OnDomReady(PrismaView view)
{
    if (!m_api || !view || view != m_view || !m_api->IsValid(view)) return;
    const bool action = m_api->BindGameThreadUIEvent(
        view, "containerAction", &ContainerMenuController::ActionEvent, this);
    const bool tick = m_api->BindGameThreadUIEvent(
        view, "containerTick", &ContainerMenuController::TickEvent, this);
    if (!action || !tick) {
        RestoreVanilla();
        m_api->Hide(view);
        return;
    }
    m_domReady = true;
    if (m_openRequested.load()) {
        auto* menu = GetLiveContainerMenu();
        if (menu && menu->uiMovie->GetVisible()) ActivateReplacement();
    }
}

void ContainerMenuController::OnTick()
{
    if (!m_api || !m_api->IsGameThread()) return;
    if (!m_openRequested.load()) {
        RestoreVanilla();
        HideReplacement();
        return;
    }

    auto* ui = RE::UI::GetSingleton();
    if (!ui) {
        HideReplacement();
        return;
    }
    auto menu = ui->GetMenu<RE::ContainerMenu>();
    if (!menu || !menu->OnStack()) {
        RestoreVanilla();
        HideReplacement();
    }
}

void ContainerMenuController::PushState()
{
    auto* menu = GetLiveContainerMenu();
    if (!menu || !m_api || !m_view || !m_api->IsValid(m_view)) return;

    std::string containerName = "CONTAINER";
    if (auto ref = menu->containerRef.get(); ref) {
        if (const char* name = ref->GetDisplayFullName(); name && name[0]) containerName = name;
    }

    const char* playerName = "PLAYER";
    if (auto* player = RE::PlayerCharacter::GetSingleton()) {
        if (const char* name = player->GetDisplayFullName(); name && name[0]) playerName = name;
    }

    try {
        const auto payload = nlohmann::json{
            { "active", true },
            { "playerName", playerName },
            { "containerName", containerName },
            { "player", BuildRows(*menu, false) },
            { "container", BuildRows(*menu, true) },
        }.dump();
        m_api->InteropCall(m_view, "setContainer", payload.c_str());
    } catch (...) {
        logger::error("PrismaHUD container: snapshot failed; restoring vanilla");
        RestoreVanilla();
        HideReplacement();
    }
}

void ContainerMenuController::TransferCategory(bool fromContainer, std::uint32_t filterMask)
{
    auto* menu = GetLiveContainerMenu();
    if (!menu || !m_active) return;

    auto& inventory = fromContainer ? menu->containerInv : menu->playerInv;
    std::vector<StableTransferRow> plan;
    plan.reserve(inventory.stackedEntries.size());

    for (const auto& entry : inventory.stackedEntries) {
        try {
            RE::InventoryItemDisplayData display(inventory.inventoryRef, entry);
            if (display.itemCount == 0) continue;
            if (filterMask != 0xFFFFFFFFu && (display.filterFlag & filterMask) == 0) continue;
            if (!fromContainer && (display.isFavorite || display.equipState != 0)) continue;
            plan.push_back({ entry.invHandle.id, StackKey(entry), display.itemCount });
        } catch (...) {}
    }

    for (const auto& wanted : plan) {
        menu = GetLiveContainerMenu();
        if (!menu || !m_openRequested.load()) break;

        std::uint32_t liveIndex = 0;
        std::uint32_t liveCount = 0;
        if (!ResolveStableRow(*menu, fromContainer, wanted, liveIndex, liveCount)) continue;
        const auto transferCount = std::min(wanted.count, liveCount);
        if (transferCount == 0) continue;
        menu->DoItemTransfer(liveIndex, transferCount, fromContainer);
    }
    PushState();
}

void ContainerMenuController::OnAction(const char* raw)
{
    if (!raw || !raw[0] || !m_api || !m_api->IsGameThread()) return;
    auto* menu = GetLiveContainerMenu();
    if (!menu || !m_active) return;

    const std::string_view action{ raw };
    if (action == "close") {
        m_openRequested.store(false);
        RestoreVanilla();
        HideReplacement();
        if (auto* queue = RE::UIMessageQueue::GetSingleton()) {
            queue->AddMessage(RE::ContainerMenu::MENU_NAME, RE::UI_MESSAGE_TYPE::kHide);
        }
        return;
    }
    if (action == "takeall") {
        menu->TakeAllItems();
        PushState();
        return;
    }

    constexpr std::string_view bulkPrefix = "bulk|";
    if (action.starts_with(bulkPrefix)) {
        const auto rest = action.substr(bulkPrefix.size());
        const auto split = rest.find('|');
        if (split == std::string_view::npos) return;
        const auto side = rest.substr(0, split);
        const bool fromContainer = side == "container";
        if (!fromContainer && side != "player") return;
        std::uint32_t mask = 0;
        if (!ParseUInt(rest.substr(split + 1), mask)) return;
        TransferCategory(fromContainer, mask);
        return;
    }

    constexpr std::string_view transferPrefix = "transfer|";
    if (!action.starts_with(transferPrefix)) return;
    const auto rest = action.substr(transferPrefix.size());
    const auto p1 = rest.find('|');
    const auto p2 = p1 == std::string_view::npos ? p1 : rest.find('|', p1 + 1);
    const auto p3 = p2 == std::string_view::npos ? p2 : rest.find('|', p2 + 1);
    const auto p4 = p3 == std::string_view::npos ? p3 : rest.find('|', p3 + 1);
    if (p1 == std::string_view::npos || p2 == std::string_view::npos ||
        p3 == std::string_view::npos || p4 == std::string_view::npos) return;

    const auto side = rest.substr(0, p1);
    const bool fromContainer = side == "container";
    if (!fromContainer && side != "player") return;

    std::uint32_t index = 0, handle = 0, count = 0;
    const std::string stackKey(rest.substr(p3 + 1, p4 - p3 - 1));
    if (!ParseUInt(rest.substr(p1 + 1, p2 - p1 - 1), index) ||
        !ParseUInt(rest.substr(p2 + 1, p3 - p2 - 1), handle) ||
        !ParseUInt(rest.substr(p4 + 1), count) || count == 0) return;

    auto& inventory = fromContainer ? menu->containerInv : menu->playerInv;
    if (index >= inventory.stackedEntries.size()) { PushState(); return; }
    const auto& live = inventory.stackedEntries[index];
    if (live.invHandle.id != handle || StackKey(live) != stackKey) { PushState(); return; }

    RE::InventoryItemDisplayData display(inventory.inventoryRef, live);
    if (count > display.itemCount) { PushState(); return; }
    menu->DoItemTransfer(index, count, fromContainer);
    PushState();
}
