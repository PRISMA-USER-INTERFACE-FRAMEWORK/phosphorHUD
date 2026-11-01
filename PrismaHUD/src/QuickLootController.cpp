#include "QuickLootController.h"
#include "QuickLootBatteryInfo.h"
#include "TrampolineGuard.h"
#include "ActivateChoiceRegistry.h"
#include "BindingRegistry.h"
#include "ControlLabels.h"
#include "HUDDataProvider.h"
#include "KeyHandler.h"
#include <algorithm>
#include <cmath>
#include <format>

QuickLootController* QuickLootController::GetSingleton()
{
    static QuickLootController s_instance;
    return &s_instance;
}

namespace {
    class PickRefSink : public RE::BSTEventSink<RE::PickRefUpdateEvent>
    {
    public:
        RE::BSEventNotifyControl ProcessEvent(const RE::PickRefUpdateEvent& a_event,
                                              RE::BSTEventSource<RE::PickRefUpdateEvent>*) override
        {
            const auto handle = *reinterpret_cast<const RE::ObjectRefHandle*>(&a_event);
            auto* qlc = QuickLootController::GetSingleton();

            const bool changed = (handle != qlc->CurrentCrosshairPick());
            qlc->SetCachedPick(handle);

            if (changed) {
                qlc->OnRequestUpdate();
                HUDDataProvider::GetSingleton()->PushPromptNow();
            }
            return RE::BSEventNotifyControl::kContinue;
        }
        static PickRefSink* GetSingleton() { static PickRefSink s; return &s; }
    };
}

void QuickLootController::InstallPickRefSink()
{
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) {
        logger::error("QuickLoot: PlayerCharacter null -- PickRefUpdateEvent sink not registered");
        return;
    }
    static bool s_registered = false;
    if (s_registered) return;
    static_cast<RE::BSTEventSource<RE::PickRefUpdateEvent>*>(player)->RegisterSink(PickRefSink::GetSingleton());
    s_registered = true;
    logger::info("QuickLoot: PickRefUpdateEvent sink registered (crosshair-activate target)");
}

static std::string QLStripBracketTag(std::string name)
{
    if (!name.empty() && name.front() == '[') {
        auto close = name.find(']');
        if (close != std::string::npos) {
            std::string rest = name.substr(close + 1);
            size_t start = rest.find_first_not_of(' ');
            if (start != std::string::npos) name = rest.substr(start);
        }
    }

    if (auto open = name.find("{{{"); open != std::string::npos) {
        if (auto close = name.find("}}}", open); close != std::string::npos) {
            name.erase(open, close + 3 - open);
            while (!name.empty() && name.back() == ' ') name.pop_back();
        }
    }

    return name;
}

static float QLGetAV(RE::Actor* actor, RE::ActorValueInfo** field, bool perm) noexcept
{
    if (!actor || !field || !*field) return 0.0f;
    try { return perm ? actor->GetPermanentActorValue(**field) : actor->GetActorValue(**field); }
    catch (...) { return 0.0f; }
}

static float QLPlayerCarriedWeight(RE::PlayerCharacter* player) noexcept
{
    if (!player || !player->inventoryList) return 0.0f;
    float total = 0.0f;
    try {
        player->inventoryList->ForEachStack(
            [&](RE::BGSInventoryItem& item) -> bool {
                auto* obj = item.object;
                if (!obj) return false;
                std::int32_t count = 0;
                for (auto* s = item.stackData.get(); s; s = s->nextStack.get())
                    count += static_cast<std::int32_t>(s->count);
                if (count > 0) total += RE::TESWeightForm::GetFormWeight(obj, nullptr) * count;
                return false;
            },
            [](RE::BGSInventoryItem&, RE::BGSInventoryItem::Stack&) { return true; });
    } catch (...) { return total; }
    return total;
}

static const char* QLRarity(int value)
{
    if (value >= 400)   return "veryrare";
    if (value >= 150)   return "rare";
    if (value >= 40)    return "uncommon";
    return "common";
}

static bool QLIsLegendary(RE::BGSInventoryItem& item)
{
    for (auto* s = item.stackData.get(); s; s = s->nextStack.get()) {
        if (auto* xl = s->extra.get(); xl && xl->GetLegendaryMod())
            return true;
    }
    return false;
}

RE::TESObjectREFR* QuickLootController::ResolveTarget()
{
    m_isWorkshopBench = false;

    if (auto* ui = RE::UI::GetSingleton()) {
        auto menu = ui->GetMenu("ContainerMenu");
        if (menu && menu->OnStack() && menu->uiMovie && menu->uiMovie->GetVisible()) return nullptr;
    }

    if (m_api && m_api->HasAnyActiveFocus()) return nullptr;

    try {
        auto* ref = m_cachedPick.get().get();
        if (!ref || ref->IsDeleted()) return nullptr;

        auto* base = ref->GetObjectReference();
        if (!base) return nullptr;

        bool isContainer = (base->formType == RE::ENUM_FORM_ID::kCONT);
        bool isDeadNPC   = (base->formType == RE::ENUM_FORM_ID::kNPC_) && ref->IsDead(true);
        if (!isContainer && !isDeadNPC) return nullptr;

        if (auto* lock = ref->GetLock()) {
            if (lock->flags & static_cast<std::uint8_t>(RE::REFR_LOCK::Flags::kLocked)) return nullptr;
        }

        if (ref->extraList && ref->extraList->HasType<RE::Workshop::ExtraData>()) {
            m_isWorkshopBench = true;
        } else if (auto* ws = RE::Workshop::FindNearestValidWorkshop(*ref)) {
            if (ws->GetFormID() == ref->GetFormID()) m_isWorkshopBench = true;
        }

        return ref;
    } catch (...) {
        return nullptr;
    }
}

void QuickLootController::BuildSnapshot(RE::TESObjectREFR* container)
{
    m_items.clear();
    m_json.clear();
    if (!container) return;

    nlohmann::json perkButton = nullptr;
    if (std::string perkLabel = EnginePerkButtonLabel(); !perkLabel.empty()) {
        std::string perkKey = ControlLabels::ForHUDAction("perk");
        if (perkKey.empty()) perkKey = "Space";
        perkButton = nlohmann::json{{"label", perkLabel}, {"key", perkKey}};
    }

    if (m_isWorkshopBench) {
        m_json = R"({"visible":false})";
        return;
    }

    bool isCorpse = container->GetObjectReference() &&
                    container->GetObjectReference()->formType == RE::ENUM_FORM_ID::kNPC_;

    std::string containerName;
    {
        auto* base = container->GetObjectReference();
        auto  sv   = base ? RE::TESFullName::GetFullName(*base) : std::string_view{};
        containerName = sv.empty() ? (isCorpse ? "Body" : "Container")
                                    : QLStripBracketTag(std::string(sv));
    }

    float          totalWeight = 0.f;
    nlohmann::json items        = nlohmann::json::array();

    if (auto* invList = container->inventoryList) {
        invList->ForEachStack(
            [&](RE::BGSInventoryItem& item) -> bool {
                auto* obj = item.object;
                if (!obj) return false;

                std::int32_t count = 0;
                bool questItem = false;
                std::uint32_t stackId = 0;
                for (auto* s = item.stackData.get(); s; s = s->nextStack.get(), ++stackId) {
                    count += static_cast<std::int32_t>(s->count);
                    if (!questItem && item.IsQuestObject(stackId))
                        questItem = true;
                }
                if (count <= 0) return false;

                auto sv   = RE::TESFullName::GetFullName(*obj);
                auto name = QLStripBracketTag(sv.empty() ? std::string{} : std::string(sv));

                std::string    category = "misc";
                std::string    formType = "MISC";
                nlohmann::json extras;

                auto ft = obj->formType.get();
                if (ft == RE::ENUM_FORM_ID::kWEAP) {
                    category = "weap"; formType = "WEAP";
                    if (auto* weap = obj->As<RE::TESObjectWEAP>()) {
                        extras["dmg"] = static_cast<int>(weap->weaponData.attackDamage);
                        extras["rng"] = static_cast<int>(weap->weaponData.maxRange);
                        if (auto* ammo = weap->weaponData.ammo) {
                            auto asv = RE::TESFullName::GetFullName(*ammo);
                            if (!asv.empty()) extras["ammoType"] = std::string(asv);
                        }
                    }
                } else if (ft == RE::ENUM_FORM_ID::kMISC) {
                    if (auto* misc = obj->As<RE::TESObjectMISC>(); misc && misc->componentData) {
                        auto comps = nlohmann::json::array();
                        for (auto& pair : *misc->componentData) {
                            if (comps.size() >= 4) break;
                            auto* cf = pair.first;
                            if (!cf) continue;
                            auto csv = RE::TESFullName::GetFullName(*cf);
                            if (csv.empty()) continue;
                            comps.push_back({ {"name", std::string(csv)},
                                              {"count", static_cast<int>(pair.second.i)} });
                        }
                        if (!comps.empty()) extras["components"] = comps;
                    }
                } else if (ft == RE::ENUM_FORM_ID::kARMO) {
                    category = "armo"; formType = "ARMO";
                    if (auto* armo = obj->As<RE::TESObjectARMO>())
                        extras["dr"] = static_cast<int>(armo->armorData.rating);
                } else if (ft == RE::ENUM_FORM_ID::kALCH) { category = "aid";  formType = "ALCH"; }
                else if   (ft == RE::ENUM_FORM_ID::kAMMO) { category = "ammo"; formType = "AMMO"; }

                const auto battery = prismhud::quickloot::InspectBattery(obj, item);
                if (battery.isPowerArmorBattery) {
                    extras["powerArmorBattery"] = true;
                    extras["batteryChargeMixed"] = battery.mixedCharge;
                    if (battery.hasChargePercent)
                        extras["batteryChargePercent"] = battery.chargePercent;
                    if (battery.fullCapacity > 0.0f)
                        extras["batteryFullCapacity"] = battery.fullCapacity;
                }

                int   value  = static_cast<int>(RE::TESValueForm::GetFormValue(obj, nullptr));
                float weight = RE::TESWeightForm::GetFormWeight(obj, nullptr);

                std::string nifPath;
                if (auto* model = obj->As<RE::TESModel>()) {
                    if (const char* mp = model->GetModel(); mp && mp[0]) nifPath = mp;
                }
                if (nifPath.empty()) {
                    if (auto* armo = obj->As<RE::TESObjectARMO>()) {
                        for (auto& aa : armo->modelArray) {
                            auto* arma = aa.armorAddon;
                            if (!arma) continue;
                            const char* mp = arma->bipedModel[0].GetModel();
                            if (!(mp && mp[0])) mp = arma->bipedModel[1].GetModel();
                            if (mp && mp[0]) { nifPath = mp; break; }
                        }
                    }
                }

                totalWeight += weight * count;

                nlohmann::json entry = {
                    {"formId",   std::format("0x{:X}", obj->GetFormID())},
                    {"name",     name},
                    {"count",    count},
                    {"value",    value},
                    {"weight",   weight},
                    {"category", category},
                    {"formType", formType},
                    {"nifPath",  nifPath},
                    {"rarity",   QLRarity(value)},
                    {"legendary", QLIsLegendary(item)},
                    {"questItem", questItem},
                };
                for (auto it = extras.begin(); it != extras.end(); ++it)
                    entry[it.key()] = it.value();
                items.push_back(entry);

                m_items.push_back({obj, count});
                return false;
            },
            [](RE::BGSInventoryItem&, RE::BGSInventoryItem::Stack&) { return true; });
    }

    if (m_sel >= static_cast<int>(m_items.size()))
        m_sel = std::max(0, static_cast<int>(m_items.size()) - 1);
    if (m_sel < 0) m_sel = 0;

    auto*  player = RE::PlayerCharacter::GetSingleton();
    auto*  av     = RE::ActorValue::GetSingleton();
    float  carryW = QLPlayerCarriedWeight(player);
    float  carryM = QLGetAV(player, &av->carryWeight, true);

    const bool usingGamepad = ControlLabels::UsingGamepad();
    auto hideKeyName = ControlLabels::ForHUDAction("rollover_hide");
    auto takeKey     = ControlLabels::ForHUDAction("take");
    auto takeAllKey  = ControlLabels::ForHUDAction("take_all");
    auto transferKey = ControlLabels::ForHUDAction("transfer");
    auto selectKey   = usingGamepad ? std::string{"gp:DPad"} : std::string{"\xE2\x87\x85"};
    if (!usingGamepad) {
        if (takeAllKey.empty())  takeAllKey  = "T";
        if (transferKey.empty()) transferKey = "R";
    }

    if (usingGamepad && !perkButton.is_null()) {
        const auto padPerkCode = ControlLabels::GamepadCodeForHUDAction("perk");
        if (padPerkCode != 0u) {
            auto clearIfCollision = [&](std::string& key, const char* action) {
                const auto actionCode = ControlLabels::GamepadCodeForHUDAction(action);
                if (actionCode != 0u && padPerkCode == actionCode) key.clear();
            };
            clearIfCollision(takeKey, "take");
            clearIfCollision(takeAllKey, "take_all");
            clearIfCollision(transferKey, "transfer");

            const auto selectUpCode = ControlLabels::GamepadCodeForHUDAction("select_up");
            const auto selectDownCode = ControlLabels::GamepadCodeForHUDAction("select_down");
            if (padPerkCode == selectUpCode || padPerkCode == selectDownCode)
                selectKey.clear();
        }
    }

    m_json = nlohmann::json{
        {"visible",           true},
        {"containerName",     containerName},
        {"containerType",     isCorpse ? "corpse" : "container"},
        {"totalWeight",       totalWeight},
        {"playerCarryWeight", carryW},
        {"playerCarryMax",    carryM > 0.f ? carryM : 200.f},
        {"sel",               m_sel},
        {"items",             items},
        {"hideKeyName",       hideKeyName},
        {"takeKey",           takeKey},
        {"takeAllKey",        takeAllKey},
        {"transferKey",       transferKey},
        {"selectKey",         selectKey},
        {"controllerStyle",   ControlLabels::GetStyle() == ControlLabels::Style::kPlayStation ? "playstation" : "xbox"},
        {"perkButton",        perkButton},
        {"activateChoices",   ActivateChoiceRegistry::GetSingleton()->BuildChoicesJSON()},
    }.dump();
}

void QuickLootController::PushToView()
{
    if (!m_api || !m_view) return;
    m_api->InteropCall(m_view, "updateLoot", m_json.c_str());
    m_wasVisible = true;
}

void QuickLootController::PushHidden()
{
    if (!m_api || !m_view) return;
    if (!m_wasVisible) return;
    m_api->InteropCall(m_view, "updateLoot", R"({"visible":false})");
    m_wasVisible = false;
}

bool QuickLootController::IsActive()
{
    return ResolveTarget() != nullptr;
}

void QuickLootController::SetExclusive(bool on)
{
    if (on == m_exclusiveActive) return;
    m_exclusiveActive = on;
    BindingRegistry::GetSingleton()->SetExclusiveInputMode(on);
}

void QuickLootController::ResetTransientForLoad()
{
    SetExclusive(false);
    m_cachedPick = {};
    m_container = {};
    m_items.clear();
    m_sel = 0;
    m_json.clear();
    m_wasVisible = false;
    m_active = false;
    m_isWorkshopBench = false;
    m_allowFullOpen = false;
}

void QuickLootController::ToggleUserHidden()
{
    m_userHidden = !m_userHidden;
    if (m_userHidden) {
        m_active    = false;
        m_container = {};
        PushHidden();
        SetExclusive(false);
    }
    logger::info("PrismaHUD QuickLoot: rollover {} by user hotkey", m_userHidden ? "HIDDEN" : "restored");
}

void QuickLootController::OnRequestUpdate()
{
    std::lock_guard<std::recursive_mutex> lock(HUDDataProvider::DataLock());

    if (m_userHidden) { m_active = false; m_container = {}; PushHidden(); SetExclusive(false); return; }

    auto* target = ResolveTarget();
    m_active = (target != nullptr);
    if (!target) {
        m_container = {};
        m_items.clear();
        m_json.clear();
        PushHidden();
        SetExclusive(false);
        return;
    }
    SetExclusive(true);

    auto newHandle = target->GetHandle();
    if (newHandle != m_container) {
        m_container = newHandle;
        m_sel       = 0;
    }

    std::string prev = m_json;
    BuildSnapshot(target);
    if (m_json != prev || !m_wasVisible)
        PushToView();
}

RE::TESObjectREFR* QuickLootController::CurrentContainer()
{
    auto ptr = m_container.get();
    return ptr ? ptr.get() : nullptr;
}

void QuickLootController::MoveSelection(int delta)
{
    if (m_items.empty()) return;
    int n  = static_cast<int>(m_items.size());
    m_sel  = std::clamp(m_sel + delta, 0, n - 1);
    if (auto* target = CurrentContainer())
        BuildSnapshot(target);
    PushToView();
}

void QuickLootController::TakeSelected()
{
    auto* container = CurrentContainer();
    auto* player    = RE::PlayerCharacter::GetSingleton();
    if (!container || !player || m_items.empty()) {
        logger::warn("PrismaHUD QuickLoot: TakeSelected no-op (container={} player={} items={})",
                     container != nullptr, player != nullptr, m_items.size());
        return;
    }
    if (m_sel < 0 || m_sel >= static_cast<int>(m_items.size())) {
        logger::warn("PrismaHUD QuickLoot: TakeSelected() sel={} out of range for {} items", m_sel, m_items.size());
        return;
    }

    {
        auto nameOf = [](RE::TESBoundObject* o) -> std::string {
            if (!o) return "<null>";
            auto nm = RE::TESFullName::GetFullName(*o);
            return nm.empty() ? "<noname>" : std::string(nm);
        };
        const auto& s = m_items[m_sel];
        logger::info("PrismaHUD QuickLoot TRACE: TAKE sel={} -> 0x{:X} '{}' x{}",
                     m_sel, s.object ? s.object->GetFormID() : 0u, nameOf(s.object), s.count);
        std::string list;
        for (int i = 0; i < static_cast<int>(m_items.size()); ++i)
            list += std::format("[{}]{}0x{:X}:{} ", i, i == m_sel ? "*" : "-",
                                m_items[i].object ? m_items[i].object->GetFormID() : 0u, nameOf(m_items[i].object));
        logger::info("PrismaHUD QuickLoot TRACE: list {}", list);
    }

    auto entry = m_items[m_sel];
    if (entry.object && entry.count > 0) {
        RE::TESObjectREFR::RemoveItemData data(entry.object, entry.count);
        data.reason         = RE::ITEM_REMOVE_REASON::kStoreContainer;
        data.otherContainer = player;
        container->RemoveItem(data);
    }

    BuildSnapshot(CurrentContainer());
    if (m_items.empty()) { m_active = false; PushHidden(); }
    else                 PushToView();
}

void QuickLootController::TakeAll()
{
    auto* container = CurrentContainer();
    auto* player    = RE::PlayerCharacter::GetSingleton();
    if (!container || !player || m_items.empty()) {
        if (m_active && !container) {
            static int s_st = 0;
            if ((s_st++ % 10) == 0)
                logger::warn("PrismaHUD QuickLoot: take-all requested but container handle is stale (no-op)");
        }
        return;
    }

    auto pending = m_items;
    for (auto& e : pending) {
        if (!e.object || e.count <= 0) continue;
        RE::TESObjectREFR::RemoveItemData data(e.object, e.count);
        data.reason         = RE::ITEM_REMOVE_REASON::kStoreContainer;
        data.otherContainer = player;
        container->RemoveItem(data);
    }

    m_sel = 0;
    BuildSnapshot(CurrentContainer());
    if (m_items.empty()) { m_active = false; PushHidden(); }
    else                 PushToView();
}

bool QuickLootController::TriggerActivateChoice(std::uint32_t buttonIndex)
{
    if (!ActivateChoiceRegistry::GetSingleton()->TriggerByIndex(buttonIndex)) return false;

    auto* container = CurrentContainer();
    if (!container) { m_active = false; PushHidden(); return true; }
    BuildSnapshot(container);
    if (m_items.empty() && CurrentContainer() == nullptr) { m_active = false; PushHidden(); }
    else                                                   PushToView();
    return true;
}

std::string QuickLootController::EnginePerkButtonLabel()
{
    try {
        auto* qlc = GetSingleton();
        if (!qlc->CurrentContainer()) return {};
        const auto expected = qlc->m_container;

        if (auto* fav = RE::FavoritesManager::GetSingleton()) {
            auto& cached = fav->quickContainerMode.eventDataStruct;
            if (cached.optionalValue.has_value()) {
                const auto& qc = *cached.optionalValue;
                if (qc.containerRef != expected) return {};
                if (qc.perkButtonEnabled && !qc.perkButtonText.empty())
                    return std::string(qc.perkButtonText.c_str());
            }
        }
    } catch (...) {}
    return {};
}

bool QuickLootController::HasEnginePerkButton()
{
    try {
        auto* qlc = GetSingleton();
        if (!qlc->CurrentContainer()) return false;
        const auto expected = qlc->m_container;

        if (auto* fav = RE::FavoritesManager::GetSingleton()) {
            auto& cached = fav->quickContainerMode.eventDataStruct;
            if (cached.optionalValue.has_value()) {
                const auto& qc = *cached.optionalValue;
                if (qc.containerRef != expected) return false;
                return qc.perkButtonEnabled && !qc.perkButtonText.empty();
            }
        }
    } catch (...) {}
    return false;
}

bool QuickLootController::ShouldSuppressContainer() const
{
    if (!m_active) return false;
    if (m_allowFullOpen) { m_allowFullOpen = false; return false; }
    if (InputMods::ShiftDown()) return false;
    return true;
}

void QuickLootController::OpenFullContainer()
{
    auto* container = CurrentContainer();
    auto* player    = RE::PlayerCharacter::GetSingleton();
    if (!container || !player) return;
    m_active = false;
    PushHidden();
    SetExclusive(false);
    m_allowFullOpen = true;
    container->ActivateRef(player, nullptr, 0, false, false, false);
    logger::info("PrismaHUD QuickLoot: R -> open full container/transfer menu");
}

namespace {
    bool ContainerSuppressPredicate() {
        return QuickLootController::GetSingleton()->ShouldSuppressContainer();
    }
}

void QuickLootController::InstallMenuSink()
{
    if (!m_api) {
        logger::error("QuickLoot: no PrismaUI API — ContainerMenu suppression not registered");
        return;
    }
    m_api->SuppressVanillaMenuIf("ContainerMenu", &ContainerSuppressPredicate);
    logger::info("QuickLoot: ContainerMenu suppression predicate registered with the framework");
}

bool QuickLootInput::ShouldHandleEvent(const RE::InputEvent* a_event)
{
    if (!a_event) return false;

    return a_event->eventType.get() == RE::INPUT_EVENT_TYPE::kButton;
}

void QuickLootInput::OnButtonEvent(const RE::ButtonEvent* a_event)
{
    if (!a_event) return;

    auto* ev   = const_cast<RE::ButtonEvent*>(a_event);
    auto  code = ev->GetBSButtonCode();

    const bool isWheel    = ev->device == RE::INPUT_DEVICE::kMouse &&
                            (code == RE::BS_BUTTON_CODE::kWheelUp || code == RE::BS_BUTTON_CODE::kWheelDown);

    const bool isPad = ev->device == RE::INPUT_DEVICE::kGamepad;
    const std::uint32_t gp = isPad ? (static_cast<std::uint32_t>(code) | 0x10000u) : 0u;
    auto* qlc = QuickLootController::GetSingleton();
    const bool padBench = qlc->IsWorkshopBenchTarget();
    const bool padPerkPresent = isPad && qlc->HasEnginePerkButton();
    const std::uint32_t padPerkCode = padPerkPresent
        ? ControlLabels::GamepadCodeForHUDAction("perk")
        : 0u;
    const bool padEnginePerk = padPerkCode != 0u && gp == padPerkCode;

    const auto keyboardTakeCode = ControlLabels::KeyboardCodeForUserEvent("Activate");
    const bool keyboardTake = ev->device == RE::INPUT_DEVICE::kKeyboard &&
                              ((keyboardTakeCode != 0u && ev->idCode == keyboardTakeCode) ||
                               (keyboardTakeCode == 0u && ev->idCode == 0x45));
    const auto padTakeCode = ControlLabels::GamepadCodeForHUDAction("take");
    const bool padTake = isPad && !padEnginePerk && padTakeCode != 0u && gp == padTakeCode;
    const bool isActivate = keyboardTake || (!isPad && ev->QUserEvent() == "Activate") || padTake;

    const auto padTakeAllCode = ControlLabels::GamepadCodeForHUDAction("take_all");
    const auto padTransferCode = ControlLabels::GamepadCodeForHUDAction("transfer");
    const auto padBenchTransferCode = ControlLabels::GamepadCodeForHUDAction("bench_transfer");
    const auto padSelUpCode = ControlLabels::GamepadCodeForHUDAction("select_up");
    const auto padSelDownCode = ControlLabels::GamepadCodeForHUDAction("select_down");
    const bool padTakeAll    = isPad && !padEnginePerk && !padBench && gp == padTakeAllCode;
    const bool padOpenFull   = isPad && !padEnginePerk &&
                               gp == (padBench ? padBenchTransferCode : padTransferCode);
    const bool padSelUp      = isPad && !padEnginePerk && gp == padSelUpCode;
    const bool padSelDown    = isPad && !padEnginePerk && gp == padSelDownCode;
    const bool isArrowUp   = (ev->device == RE::INPUT_DEVICE::kKeyboard && ev->idCode == 0x26) || padSelUp;
    const bool isArrowDown = (ev->device == RE::INPUT_DEVICE::kKeyboard && ev->idCode == 0x28) || padSelDown;
    const bool isTakeAll   = (ev->device == RE::INPUT_DEVICE::kKeyboard && ev->idCode == 0x54) || padTakeAll;
    const bool isOpenFull  = (ev->device == RE::INPUT_DEVICE::kKeyboard && ev->idCode == 0x52) || padOpenFull;
    const bool isChoice1   = ev->device == RE::INPUT_DEVICE::kKeyboard && ev->idCode == 0x31;
    const bool isChoice2   = ev->device == RE::INPUT_DEVICE::kKeyboard && ev->idCode == 0x32;
    const bool isChoice3   = ev->device == RE::INPUT_DEVICE::kKeyboard && ev->idCode == 0x33;
    if (!isWheel && !isActivate && !isArrowUp && !isArrowDown && !isTakeAll && !isOpenFull &&
        !isChoice1 && !isChoice2 && !isChoice3) return;

    const bool active   = qlc->Active();
    const bool livePick = qlc->LivePickIsCurrentTarget();
    if (!active || !livePick || qlc->AnyViewFocused()) {
        static int s_dropLogThrottle = 0;
        if ((s_dropLogThrottle++ % 30) == 0)
            logger::debug("QuickLoot: input gate blocked (active={} livePick={})", active, livePick);
        m_tookAll = false;
        return;
    }

    if (qlc->IsWorkshopBenchTarget() && !isOpenFull && !isChoice1 && !isChoice2 && !isChoice3) return;

    if (isWheel) {
        if (ev->value != 0.f) qlc->MoveSelection(code == RE::BS_BUTTON_CODE::kWheelUp ? -1 : 1);
        ev->handled = RE::InputEvent::HANDLED_RESULT::kStop;
        return;
    }
    if (isArrowUp || isArrowDown) {
        if (ev->QReleased()) qlc->MoveSelection(isArrowUp ? -1 : 1);
        ev->handled = RE::InputEvent::HANDLED_RESULT::kStop;
        return;
    }

    if (isTakeAll) {
        if (ev->QReleased()) qlc->TakeAll();
        ev->handled = RE::InputEvent::HANDLED_RESULT::kStop;
        return;
    }

    if (isOpenFull) {
        if (ev->QReleased()) qlc->OpenFullContainer();
        ev->handled = RE::InputEvent::HANDLED_RESULT::kStop;
        return;
    }

    if (isChoice1 || isChoice2 || isChoice3) {
        if (!ev->QReleased()) return;
        const std::uint32_t idx = isChoice1 ? 1u : isChoice2 ? 2u : 3u;
        if (qlc->TriggerActivateChoice(idx))
            ev->handled = RE::InputEvent::HANDLED_RESULT::kStop;
        return;
    }

    {
        if (InputMods::ShiftDown())
            return;

        if (a_event->value != 0.0f) {
            if (!m_activateHeld) { m_activateHeld = true; qlc->TakeSelected(); }
        } else {
            m_activateHeld = false;
        }
        ev->handled = RE::InputEvent::HANDLED_RESULT::kStop;
        return;
    }
}

namespace {
    using TakeSelectedItemFn = void (*)(void*);
    TakeSelectedItemFn g_origTakeSelectedItem = nullptr;

    constexpr std::uintptr_t kTakeSelectedItemCallSite = 0xa923bb;
    constexpr std::uintptr_t kTakeSelectedItem         = 0xa92810;

    void Detour_TakeSelectedItem(void* a_self)
    {
        auto* qlc = QuickLootController::GetSingleton();
        if (qlc->Active() && qlc->LivePickIsCurrentTarget() && !qlc->IsWorkshopBenchTarget() &&
            !qlc->UserHidden()) {
            return;
        }
        if (g_origTakeSelectedItem) g_origTakeSelectedItem(a_self);
    }
}

void QuickLootController::InstallQuickContainerTakeHook()
{
    static bool s_installed = false;
    if (s_installed) return;

    if (!REX::FModule::IsRuntimeOG()) {
        logger::warn("QuickLoot: skipping the TakeSelectedItem call-site patch -- RVA is 1.10.163 "
                     "OG-only and this runtime is NG/AE. Vanilla will double-take on Activate.");
        return;
    }

    REL::Relocation<std::uintptr_t> site{ REL::Offset{ kTakeSelectedItemCallSite } };
    const auto* p = reinterpret_cast<const std::uint8_t*>(site.address());
    bool ok = (p[0] == 0xE8);
    if (ok) {
        const auto rel    = *reinterpret_cast<const std::int32_t*>(p + 1);
        const auto target = site.address() + 5 + rel;
        REL::Relocation<std::uintptr_t> expected{ REL::Offset{ kTakeSelectedItem } };
        ok = (target == expected.address());
    }
    if (!ok) {
        logger::critical("QuickLoot: REFUSING to patch the TakeSelectedItem call site -- RVA {:#x} is "
                         "not a CALL to {:#x}. Bytes: {:02x} {:02x} {:02x} {:02x} {:02x}. Stale RVA, "
                         "wrong runtime, or another mod patched it first; not touching it. Vanilla "
                         "will keep double-taking, but nothing is corrupted.",
                         kTakeSelectedItemCallSite, kTakeSelectedItem, p[0], p[1], p[2], p[3], p[4]);
        return;
    }

    EnsurePrismaHUDTrampoline();

    g_origTakeSelectedItem = reinterpret_cast<TakeSelectedItemFn>(
        REL::GetTrampoline().write_call<5>(site.address(), &Detour_TakeSelectedItem));
    s_installed = true;
    logger::info("QuickLoot: patched the TakeSelectedItem call site in OnActivate (RVA {:#x} -> {:#x}) "
                 "-- vanilla no longer takes a second item while the Prisma panel owns the target",
                 kTakeSelectedItemCallSite, kTakeSelectedItem);
}

void QuickLootInput::Install()
{
    static QuickLootInput* s_handler = nullptr;
    auto* pc = RE::PlayerControls::GetSingleton();
    if (!pc) {
        logger::error("QuickLoot: PlayerControls null — take/scroll handler not installed");
        return;
    }

    if (s_handler) {
        for (std::uint32_t i = 0; i < pc->handlers.size(); ++i)
            if (pc->handlers[i] == s_handler) return;
    } else {
        s_handler = new QuickLootInput(pc->data);
    }

    pc->RegisterHandler(s_handler);
    auto& h = pc->handlers;
    if (h.size() > 1 && h[h.size() - 1] == s_handler) {
        for (std::uint32_t i = static_cast<std::uint32_t>(h.size()) - 1; i > 0; --i)
            h[i] = h[i - 1];
        h[0] = s_handler;
    }
    logger::info("QuickLoot: take/scroll/R input handler installed at front ({} handlers)", h.size());
}
