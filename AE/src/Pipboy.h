#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "Core.h"

namespace Phosphor::Pipboy {

struct QuestMarker {
    std::uint32_t pipId = 0;
    double x = 0, y = 0, z = 0;
    bool hasZ = false;
    bool onDoor = false;
    std::string name;
    std::vector<std::uint32_t> quests;
};

struct Location {
    std::uint32_t pipId = 0;
    double x = 0, y = 0;
    std::string name;
    int type = -1;
    bool discovered = false;
    bool visible = true;
    bool workshop = false;
    std::uint32_t markerFormId = 0;
};

struct Objective {
    std::string text;
    bool completed = false;
    bool active = false;
};

struct Quest {
    std::uint32_t formId = 0;
    std::string text;
    bool active = false;
    std::vector<Objective> objectives;
};

struct Snapshot {
    bool connected = false;
    std::uint64_t version = 0;
    bool hasPlayer = false;
    double playerX = 0, playerY = 0;
    std::optional<CustomWaypoint> customWorld, customLocal;
    std::vector<QuestMarker> markers;
    std::vector<Location> locations;
    std::vector<Quest> quests;
};

void Start(const std::string& dumpPath);
void Stop();
Snapshot Latest();
}
