#include "PluginState.h"

namespace Phosphor::Plugin {
std::atomic<std::uint32_t> lastHitId{0};
std::atomic<std::int64_t> lastHitAt{0};
std::int64_t NowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now().time_since_epoch()).count();
}
namespace {
RE::BSTEventSource<RE::TESHitEvent>* HitEventSource() {
#if defined(PHOSPHOR_AE)
    AE_GATE("bTarget", nullptr);
    return RE::TESHitEvent::GetEventSource();
#else
    using func_t = RE::BSTEventSource<RE::TESHitEvent>* (*)();
    static REL::Relocation<func_t> func{ REL::RelocationID(1411899, 2201886) };
    return func();
#endif
}
class HitWatch final : public RE::BSTEventSink<RE::TESHitEvent> {
    RE::BSEventNotifyControl ProcessEvent(const RE::TESHitEvent& event, RE::BSTEventSource<RE::TESHitEvent>*) override {
#if defined(PHOSPHOR_AE)
        if (!Phosphor::AE::Enabled("bTarget")) return RE::BSEventNotifyControl::kContinue;
#endif
        try {
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (player && event.cause.get() == player && event.target && event.target.get() != player && event.target->As<RE::Actor>()) {
                lastHitId.store(event.target->GetFormID());
                lastHitAt.store(NowMs());
            }
        } catch (...) {}
        return RE::BSEventNotifyControl::kContinue;
    }
} hitWatch;
}

void RegisterHitWatch() {
    if (auto* hits = HitEventSource()) hits->RegisterSink(&hitWatch);
}

namespace {
struct QuestSeen { std::string text; std::vector<std::pair<std::string, bool>> objectives; };
std::map<std::uint32_t, QuestSeen> questsSeen;
}
bool questBaseline = false;
void QuestEvents(const Phosphor::Pipboy::Snapshot& pip, Json& message) {
    if (!pip.connected || pip.quests.empty()) { questBaseline = false; return; }
    std::map<std::uint32_t, QuestSeen> now;
    for (const auto& q : pip.quests) {
        QuestSeen seen{q.text, {}};
        for (const auto& o : q.objectives) seen.objectives.emplace_back(o.text, o.completed);
        now[q.formId] = std::move(seen);
    }
    if (!questBaseline) { questsSeen = std::move(now); questBaseline = true; return; }
    Json banners = Json::array(), lines = Json::array();
    for (const auto& [id, q] : now) {
        const auto before = questsSeen.find(id);
        const auto firstOpen = [&]() -> std::string {
            for (const auto& [text, done] : q.objectives) if (!done) return text;
            return {};
        };
        if (before == questsSeen.end()) {
            if (id == 0) continue;
            Json banner{{"eyebrow", "Quest started"}, {"title", Phosphor::Live::CleanName(q.text, false)}};
            if (auto open = firstOpen(); !open.empty()) banner["objectives"] = Json::array({Json{{"t", open}}});
            banners.push_back(std::move(banner));
            continue;
        }
        const auto wasDone = [&](const std::string& text) -> int {
            for (const auto& [t, d] : before->second.objectives) if (t == text) return d ? 1 : 0;
            return -1;
        };
        bool allDone = !q.objectives.empty(), allDoneBefore = !before->second.objectives.empty();
        for (const auto& [t, d] : before->second.objectives) allDoneBefore = allDoneBefore && d;
        for (const auto& [text, done] : q.objectives) {
            allDone = allDone && done;
            const int was = wasDone(text);
            if (was == 0 && done) lines.push_back(Json{{"text", text}, {"sub", "Objective completed · " + Phosphor::Live::CleanName(q.text, false)}, {"kind", "quest"}});
            else if (was == -1 && !done) lines.push_back(Json{{"text", text}, {"sub", id == 0 ? std::string("Miscellaneous") : Phosphor::Live::CleanName(q.text, false)}, {"kind", "quest"}});
        }
        if (id != 0 && allDone && !allDoneBefore) {
            Json last = Json::array();
            if (!q.objectives.empty()) last.push_back(Json{{"t", q.objectives.back().first}, {"done", true}});
            banners.push_back(Json{{"eyebrow", "Quest completed"}, {"title", Phosphor::Live::CleanName(q.text, false)}, {"objectives", last}, {"tone", "good"}});
        }
    }
    questsSeen = std::move(now);
    for (std::size_t i = 0; i < banners.size(); ++i) {
        if (i == 0 && !message.contains("quest")) message["quest"] = banners[i];
        else Send(Json{{"quest", banners[i]}});
    }
    for (std::size_t i = 0; i < lines.size(); ++i) {
        if (i == 0 && !message.contains("notify")) message["notify"] = lines[i];
        else Send(Json{{"notify", lines[i]}});
    }
}

namespace {
std::vector<std::string> shownMessages;
}
bool messagesBaseline = false;
void HudTextFields(Json& current, Json& message, const Phosphor::Live::HudText& t) {
    if (!t.ok) return;
    if (t.rolloverShown) {
        Json keys = Json::array();
        for (const auto& [key, label] : t.keys) {
            Json k{{"text", Phosphor::Live::CleanName(label, false)}};
            if (!key.empty()) k["key"] = key;
            keys.push_back(std::move(k));
        }
        Json p{{"name", Phosphor::Live::CleanName(t.rolloverName, false)}, {"keys", keys}};
        if (t.crime) p["crime"] = true;
        current["prompt"] = std::move(p);
    } else current["prompt"] = nullptr;
    current["subtitle"] = t.subtitleShown ? Json{{"who", t.who}, {"line", t.line}, {"ms", 60000}} : Json(nullptr);
    current["message"] = t.tutorialShown ? Json{{"text", t.tutorial}, {"ms", 60000}} : Json(nullptr);
    std::vector<std::string> fresh;
    for (const auto& m : t.messages)
        if (std::find(shownMessages.begin(), shownMessages.end(), m) == shownMessages.end()) fresh.push_back(m);
    shownMessages = t.messages;
    if (!messagesBaseline) { messagesBaseline = true; return; }
    const auto has = [](std::string text, std::string_view part) {
        for (auto& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return text.find(part) != std::string::npos;
    };
    bool first = true;
    for (const auto& text : fresh) {
        if (auto done = Phosphor::Mods::ChallengeFromMessage(text)) { Send(Json{{"challengeComplete", *done}}); continue; }
        Phosphor::Live::NoteSurvivalMessage(text);
        if (Phosphor::Mods::HasKarma() && has(text, "karma")) continue;
        if (Phosphor::Mods::HasReputation() && (has(text, "fame") || has(text, "infamy"))) continue;
        Json line{{"text", Phosphor::Live::CleanName(text, false)}, {"kind", "item"}};
        if (first && !message.contains("notify")) message["notify"] = std::move(line);
        else Send(Json{{"notify", line}});
        first = false;
    }
}

std::map<std::string, float> affinitySeen;
std::optional<std::string> localFactionSeen;
bool localFactionBaseline = false;

void ModFields(Json& current, const Phosphor::Pipboy::Snapshot& pip, int enemies) {
    if (auto karma = Phosphor::Mods::ReadKarma(); !karma.is_null()) current["karma"] = std::move(karma);
    if (Phosphor::Mods::HasArmorPlates()) current["armor"] = Phosphor::Mods::ReadArmorPlates();
    if (auto challenges = Phosphor::Mods::ReadChallenges(pip); !challenges.is_null()) current["challenges"] = std::move(challenges);
    current["settlementAttack"] = Phosphor::Mods::ReadAttack(pip, enemies);
    current["gear"] = Phosphor::Live::ReadGear();
    Json status;
    const auto faction = Phosphor::Mods::LocalFaction(&status);
    if (!localFactionBaseline || faction != localFactionSeen) {
        if (faction) Send(Json{{"reputation", status}});
        localFactionSeen = faction;
        localFactionBaseline = true;
    }
    current["faction"] = faction ? Json(*faction) : Json(nullptr);
}
void ModEvents(const Json& current) {
    for (auto& e : Phosphor::Mods::ReputationEvents()) { Send(Json{{"reputation", e}}); lastRepEvent = Clock::now(); }
    if (current.contains("party")) {
        const bool baseline = !affinitySeen.empty();
        std::map<std::string, float> now;
        for (const auto& m : current["party"]) {
            if (!m.contains("affinity") || !m["affinity"].is_number()) continue;
            const auto name = m.value("name", std::string{});
            const float value = m["affinity"].get<float>();
            now[name] = value;
            const auto before = affinitySeen.find(name);
            if (!baseline || before == affinitySeen.end()) continue;
            const float d = value - before->second;
            if (std::abs(d) < 5) continue;
            const char* kind = d >= 30 ? "loved" : d > 0 ? "liked" : d <= -30 ? "hated" : "disliked";
            Send(Json{{"companionReact", Json{{"name", name}, {"kind", kind}}}});
            spdlog::info("Companion {} affinity {:.0f} -> {:.0f} ({})", name, before->second, value, kind);
        }
        for (const auto& [name, value] : affinitySeen) now.try_emplace(name, value);
        affinitySeen = std::move(now);
    }
}
}
