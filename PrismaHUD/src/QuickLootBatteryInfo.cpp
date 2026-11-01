#include "QuickLootBatteryInfo.h"

#include "RE/B/BGSInventoryItem.h"
#include "RE/P/PowerArmor.h"

#include <algorithm>
#include <cmath>

namespace prismhud::quickloot
{
    BatteryInfo InspectBattery(const RE::TESBoundObject* object,
                               const RE::BGSInventoryItem& item) noexcept
    {
        BatteryInfo result{};
        if (!object) {
            return result;
        }

        try {
            if (!RE::PowerArmor::IsPowerArmorBattery(object)) {
                return result;
            }
            result.isPowerArmorBattery = true;

            if (auto* capacity = RE::PowerArmor::GetNewBatteryCapacity()) {
                result.fullCapacity = std::max(0.0f, capacity->GetFloat());
            }

            bool  sawCharge = false;
            float firstCharge = 1.0f;

            for (auto* stack = item.stackData.get(); stack; stack = stack->nextStack.get()) {
                if (stack->count == 0) {
                    continue;
                }

                float charge = 1.0f;
                if (auto* extra = stack->extra.get()) {
                    charge = extra->GetHealthPerc();
                }

                if (!std::isfinite(charge)) {
                    result.hasChargePercent = false;
                    result.mixedCharge = true;
                    return result;
                }

                charge = std::clamp(charge, 0.0f, 1.0f);
                if (!sawCharge) {
                    firstCharge = charge;
                    sawCharge = true;
                    continue;
                }

                if (std::fabs(charge - firstCharge) > 0.001f) {
                    result.hasChargePercent = false;
                    result.mixedCharge = true;
                    return result;
                }
            }

            if (sawCharge) {
                result.chargePercent = firstCharge * 100.0f;
                result.hasChargePercent = true;
            }
        } catch (...) {
            result.hasChargePercent = false;
        }

        return result;
    }
}
