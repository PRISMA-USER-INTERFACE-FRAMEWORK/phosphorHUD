#pragma once

// Quick loot first: perk activation choices (the four-way wheel that mods such as Alternative Activation Prompts,
// Scrap As You Go and You Are Exceptional add) stay off containers and dead bodies, so the game shows its quick-loot
// box, until the player holds the perk-choices key picked in the MCM (or Dynamic Activation Key's key).
//
// How: when the game data has loaded, every perk "Activate" entry gets four ordinary game conditions on its target
// tab: (gate open OR target is not a container) AND (gate open OR target is not dead). The gate is a temporary
// global Phosphor creates; the engine evaluates the conditions itself, so nothing in the game's code is changed.
// Nothing is touched until the player turns it on with a key bound; after that the gate stays open (= the game
// behaves exactly as before) whenever the feature is off, the key is unbound, or a key is held. If the layout checks
// fail, nothing is changed at all.
namespace Phosphor::QuickLootFirst {
// Game thread, at kGameDataReady: perk data may be inspected from now on.
void DataReady();
// Game thread, every tick: installs the conditions once, the first time the switch is on and a key is bound.
// Validates the perk-entry layout before changing anything.
void Maintain();
// Any thread: MCM switch and the bound key (Windows virtual-key code, MCM modifiers 1 Shift, 2 Ctrl, 4 Alt; 0 = unbound).
void Configure(bool enabled, int keyCode, int modifiers);
// Poll thread: opens or closes the gate from the key state. `foreground` is whether the game window has focus.
void Update(bool foreground);
}
