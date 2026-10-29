#pragma once
// Quest markers, objectives, map locations and the custom waypoint read straight from the engine, in the same shape
// as the Pip-Boy feed's snapshot. Used when the feed is not connected (the Pip-Boy App option is off, or AE, where the
// game no longer starts that service). Game thread only.
#include "Pipboy.h"

namespace Phosphor::EngineQuests {
Pipboy::Snapshot Read();
// Forget cached map markers and texts (save load, main menu).
void Reset();
// Logs where the engine snapshot and the Pip-Boy feed disagree (bounded); for validating the engine source.
void Compare(const Pipboy::Snapshot& engine, const Pipboy::Snapshot& feed);
}
