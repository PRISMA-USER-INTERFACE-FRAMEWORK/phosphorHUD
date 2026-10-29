#pragma once
// Read-only diagnostic: logs the engine's own quest targets, objectives and map markers next to what the
// Pip-Boy feed reports, to verify the data layouts on OG before the compass reads them directly.
// OG only. Game thread only.
namespace Phosphor::Pipboy { struct Snapshot; }

namespace Phosphor::EngineProbe {
void Run(const Pipboy::Snapshot& pip, const char* why);
}
