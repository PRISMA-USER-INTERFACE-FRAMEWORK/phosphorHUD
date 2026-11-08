#pragma once

namespace Phosphor::QuickLootFirst {
void DataReady();
void Maintain();
void Configure(bool enabled, int keyCode, int modifiers);
void Update(bool foreground);
}
