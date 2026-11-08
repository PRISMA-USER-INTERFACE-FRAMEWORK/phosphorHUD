#pragma once
#include "Pipboy.h"

namespace Phosphor::EngineQuests {
Pipboy::Snapshot Read();
void Reset();
void Compare(const Pipboy::Snapshot& engine, const Pipboy::Snapshot& feed);
}
