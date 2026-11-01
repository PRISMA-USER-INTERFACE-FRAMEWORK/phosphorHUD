#pragma once

namespace RE
{
    class BGSInventoryItem;
    class TESBoundObject;
}

namespace prismhud::quickloot
{
    struct BatteryInfo
    {
        bool  isPowerArmorBattery{ false };
        bool  hasChargePercent{ false };
        bool  mixedCharge{ false };
        float chargePercent{ 0.0f };
        float fullCapacity{ 0.0f };
    };

    [[nodiscard]] BatteryInfo InspectBattery(const RE::TESBoundObject* object,
                                             const RE::BGSInventoryItem& item) noexcept;
}
