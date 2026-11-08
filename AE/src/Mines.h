#pragma once
#include "Core.h"
#include <string>
#include <vector>

namespace Phosphor::Mines {
void RegisterSinks();
void Reset();
void SetEventCapture(bool enabled);
void SetEnabled(bool enabled);
bool Active();
Json Read(std::vector<std::string>* exploded);
}
