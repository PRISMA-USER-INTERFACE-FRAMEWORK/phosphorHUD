#pragma once
// Reads the game's own Pip-Boy data feed (the companion app server on 127.0.0.1:27000).
// The game fills it with quests, objectives and map markers; this is what its own Pip-Boy shows.
// Runs on a background thread; the game thread takes copies with Latest().
#include <cstdint>
#include <string>
#include <vector>
#include "Core.h"

namespace Phosphor::Pipboy {

struct QuestMarker {
    std::uint32_t pipId = 0;      // node id in the Pip-Boy database (stable while connected)
    double x = 0, y = 0, z = 0;   // world units
    bool hasZ = false;
    bool onDoor = false;          // the objective is behind this load door
    std::string name;
    std::vector<std::uint32_t> quests;  // quest form ids this marker belongs to
};

struct Location {
    std::uint32_t pipId = 0;
    double x = 0, y = 0;
    std::string name;
    int type = -1;            // the Pip-Boy's map marker type number
    bool discovered = false;
    bool visible = true;
    bool workshop = false;    // a settlement (has WorkshopOwned)
    std::uint32_t markerFormId = 0;  // the map marker reference ("LocationMarkerFormId")
};

struct Objective {
    std::string text;
    bool completed = false;
    bool active = false;
};

struct Quest {
    std::uint32_t formId = 0;     // 0 is the Miscellaneous group
    std::string text;
    bool active = false;          // tracked in the Pip-Boy
    std::vector<Objective> objectives;  // enabled objectives only, in Pip-Boy order
};

struct Snapshot {
    bool connected = false;
    std::uint64_t version = 0;    // bumps on every data update
    bool hasPlayer = false;
    double playerX = 0, playerY = 0;  // Map.World.Player (last exterior position)
    std::optional<CustomWaypoint> customWorld, customLocal;
    std::vector<QuestMarker> markers;
    std::vector<Location> locations;
    std::vector<Quest> quests;
};

void Start(const std::string& dumpPath);  // starts the reader thread (idempotent)
void Stop();
Snapshot Latest();
}
