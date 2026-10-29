#pragma once
// Mine warnings (what Mine Detector HUD did): armed mines near the player, projected onto the HUD stage.
#include "Core.h"
#include <string>
#include <vector>

namespace Phosphor::Mines {
// Tracks mine references as their cells attach and detach. Idempotent.
void RegisterSinks();
// New save or main menu: forget tracked state and stop capture; attached cells are rescanned on resume.
void Reset();
void SetEventCapture(bool enabled);
// The HUD's `mines` list for this frame (game thread). Mines that vanished while counting down are added to
// *exploded. Untriggered mines show when the player is close to their trigger radius; triggered ones carry
// left/total (fuse seconds).
Json Read(std::vector<std::string>* exploded);
}
