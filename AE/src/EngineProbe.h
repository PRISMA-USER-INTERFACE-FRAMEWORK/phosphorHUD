#pragma once
namespace Phosphor::Pipboy { struct Snapshot; }

namespace Phosphor::EngineProbe {
void Run(const Pipboy::Snapshot& pip, const char* why);
}
