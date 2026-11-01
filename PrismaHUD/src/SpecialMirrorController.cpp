#include "SpecialMirrorController.h"

#include <Scaleform/G/GFx_Value.h>

namespace {
    constexpr const char* kSpecialMenu = "SPECIALMenu";

    class SpecialMenuSink : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
    {
    public:
        RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent& a_event,
                                              RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
        {
            if (a_event.menuName != kSpecialMenu)
                return RE::BSEventNotifyControl::kContinue;

            auto* ctl = SpecialMirrorController::GetSingleton();
            if (a_event.opening) ctl->OnMenuOpen();
            else                 ctl->OnMenuClose();
            return RE::BSEventNotifyControl::kContinue;
        }
        static SpecialMenuSink* GetSingleton() { static SpecialMenuSink s; return &s; }
    };

    struct Row { const char* key; RE::ActorValueInfo* RE::ActorValue::* member; };
    constexpr Row kRows[] = {
        { "strength",     &RE::ActorValue::strength     },
        { "perception",   &RE::ActorValue::perception   },
        { "endurance",    &RE::ActorValue::endurance    },
        { "charisma",     &RE::ActorValue::charisma     },
        { "intelligence", &RE::ActorValue::intelligence },
        { "agility",      &RE::ActorValue::agility      },
        { "luck",         &RE::ActorValue::luck         },
    };
}

namespace {
    void DumpValue(const char* a_name, const Scaleform::GFx::Value& a_val, int a_depth)
    {
        const std::string indent(static_cast<std::size_t>(a_depth) * 2, ' ');
        if (a_val.IsString()) {
            const char* sv = a_val.GetString();
            logger::info("[SPECIAL SWF] {}{} = \"{}\"   (string)", indent, a_name, sv ? sv : "");
        } else if (a_val.IsInt()) {
            logger::info("[SPECIAL SWF] {}{} = {}   (int)", indent, a_name, a_val.GetInt());
        } else if (a_val.IsUInt()) {
            logger::info("[SPECIAL SWF] {}{} = {}   (uint)", indent, a_name, a_val.GetUInt());
        } else if (a_val.IsNumber()) {
            logger::info("[SPECIAL SWF] {}{} = {}   (number)", indent, a_name, a_val.GetNumber());
        } else if (a_val.IsBoolean()) {
            logger::info("[SPECIAL SWF] {}{} = {}   (bool)", indent, a_name, a_val.GetBoolean());
        } else if (a_val.IsArray()) {
            logger::info("[SPECIAL SWF] {}{} = [{} elements]   (array)", indent, a_name, a_val.GetArraySize());
        } else if (a_val.IsDisplayObject() || a_val.IsObject()) {
            logger::info("[SPECIAL SWF] {}{}   ({})", indent, a_name,
                         a_val.IsDisplayObject() ? "displayObject" : "object");
            if (a_depth < 2) {
                try {
                    a_val.VisitMembers([a_depth](const char* n, const Scaleform::GFx::Value& child) {
                        DumpValue(n ? n : "<null>", child, a_depth + 1);
                    });
                } catch (...) {}
            }
        } else {
            logger::info("[SPECIAL SWF] {}{}   (undefined/other)", indent, a_name);
        }
    }

    void DumpSpecialMovie()
    {
        auto* ui = RE::UI::GetSingleton();
        if (!ui) return;
        auto menu = ui->GetMenu(kSpecialMenu);
        if (!menu || !menu->uiMovie) {
            logger::warn("[SPECIAL SWF] menu/uiMovie unavailable -- cannot dump");
            return;
        }
        for (const char* path : { "root1.Menu_mc", "root1" }) {
            Scaleform::GFx::Value v;
            if (menu->uiMovie->GetVariable(std::addressof(v), path)) {
                logger::info("[SPECIAL SWF] ===== dump of '{}' =====", path);
                DumpValue(path, v, 0);
            } else {
                logger::warn("[SPECIAL SWF] GetVariable('{}') failed", path);
            }
        }
        logger::info("[SPECIAL SWF] ===== end dump =====");
    }
}

SpecialMirrorController* SpecialMirrorController::GetSingleton()
{
    static SpecialMirrorController s_instance;
    return &s_instance;
}

void SpecialMirrorController::InstallMenuSink()
{
    static bool s_installed = false;
    if (s_installed) return;
    auto* ui = RE::UI::GetSingleton();
    if (!ui) {
        logger::error("SpecialMirror: RE::UI null -- SPECIALMenu sink not registered");
        return;
    }
    ui->RegisterSink(SpecialMenuSink::GetSingleton());
    s_installed = true;
    logger::info("SpecialMirror: MenuOpenCloseEvent sink registered for '{}' (read-only mirror; "
                 "vanilla menu NOT suppressed)", kSpecialMenu);
}

void SpecialMirrorController::OnMenuOpen()
{
    m_open = true;
    m_last.clear();
    logger::info("SpecialMirror: SPECIALMenu opened -- showing read-only mirror alongside vanilla");

    static bool s_dumped = false;
    if (!s_dumped) { s_dumped = true; try { DumpSpecialMovie(); } catch (...) {} }
    if (m_api && m_view) m_api->Show(m_view);
}

void SpecialMirrorController::OnMenuClose()
{
    m_open = false;
    m_last.clear();
    logger::info("SpecialMirror: SPECIALMenu closed -- hiding mirror");
    if (m_api && m_view) {
        m_api->InteropCall(m_view, "updateSpecial", R"({"visible":false})");
        m_api->Hide(m_view);
    }
}

std::string SpecialMirrorController::BuildJSON() const
{
    auto* player = RE::PlayerCharacter::GetSingleton();
    auto* av     = RE::ActorValue::GetSingleton();
    if (!player || !av) return R"({"visible":false})";

    std::string name;
    try {
        if (auto* npc = player->GetNPC()) {
            auto sv = RE::TESFullName::GetFullName(*npc);
            if (!sv.empty()) name = std::string(sv);
        }
    } catch (...) {}

    nlohmann::json stats = nlohmann::json::array();
    for (const auto& r : kRows) {
        auto* info = av->*(r.member);
        if (!info) continue;

        int value = 0;
        try { value = static_cast<int>(player->GetActorValue(*info)); } catch (...) {}

        std::string label;
        try {
            auto sv = RE::TESFullName::GetFullName(*info);
            if (!sv.empty()) label = std::string(sv);
        } catch (...) {}

        std::string desc;
        try {
            RE::BSString buf;
            info->GetDescription(buf, info);
            if (buf.c_str()) desc = buf.c_str();
        } catch (...) {}

        stats.push_back({ {"key", r.key}, {"label", label}, {"value", value}, {"description", desc} });
    }

    return nlohmann::json{
        {"visible", true},
        {"name",    name},
        {"stats",   stats},
    }.dump();
}

void SpecialMirrorController::OnRequestUpdate()
{
    if (!m_api || !m_view) return;
    if (!m_open) return;

    auto json = BuildJSON();
    if (json == m_last) return;
    m_last = json;
    m_api->InteropCall(m_view, "updateSpecial", json.c_str());
}
