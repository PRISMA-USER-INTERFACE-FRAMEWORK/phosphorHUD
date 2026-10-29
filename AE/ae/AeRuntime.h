#pragma once
#include "AeOptions.h"
#include <F4SE/F4SE.h>
#include <RE/Fallout.h>
#include <Scaleform/Scaleform.h>
#include <exception>

namespace RE {
namespace Scaleform = ::Scaleform;
using LIFE_STATE = ACTOR_LIFE_STATE;
// OG's kSurvival means the modern survival mode (difficulty 6).
enum class DIFFICULTY_LEVEL { kSurvival = static_cast<int>(DifficultyLevel::kTrueSurvival) };
}

namespace Phosphor::AE {
inline Options options;
inline std::string runtime, f4seVersion;
inline std::string modDetection = "optional-mod detection disabled/not resolved";
void SelfCheckOnce(std::string_view path);
void Initialize();
void ResetObservations();
void Observe(std::string_view subsystem, bool success);
inline bool Enabled(std::string_view key) { return options.Enabled(key); }
inline bool RuntimeAllowed(const REL::Version& version) {
    return RuntimeAllowed(version.major(), version.minor(), version.patch(), version.build());
}
class ReadScope {
    std::string_view name_;
    int exceptions_ = std::uncaught_exceptions();
    bool failed_ = false;
public:
    explicit ReadScope(std::string_view name) : name_(name) {}
    void Fail() { failed_ = true; }
    ~ReadScope() { Observe(name_, !failed_ && std::uncaught_exceptions() == exceptions_); }
};
}

// Every call is inside PHOSPHOR_AE conditionals; OG does not compile this header.
#define AE_GATE(key, ...) \
    if (!Phosphor::AE::Enabled(key)) return __VA_ARGS__; \
    Phosphor::AE::ReadScope aeReadScope{key}
