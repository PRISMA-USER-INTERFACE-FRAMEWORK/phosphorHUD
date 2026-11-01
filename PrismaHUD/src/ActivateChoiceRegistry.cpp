#include "ActivateChoiceRegistry.h"

ActivateChoiceRegistry* ActivateChoiceRegistry::GetSingleton()
{
    static ActivateChoiceRegistry s_instance;
    return &s_instance;
}

void ActivateChoiceRegistry::RegisterActivateChoice(const char* id, const char* label,
    const char* iconId, PRISMA_HUD_API::ActivateChoiceCallback onTrigger, void* context) noexcept
{
    if (!id || !*id || !onTrigger) return;
    std::lock_guard lk(m_lock);
    for (auto& e : m_choices) {
        if (e.id == id) {
            e.label = label  ? label  : "";
            e.icon  = iconId ? iconId : "";
            e.cb    = onTrigger;
            e.ctx   = context;
            return;
        }
    }
    m_choices.push_back({ id, label ? label : "", iconId ? iconId : "", onTrigger, context });
}

void ActivateChoiceRegistry::UnregisterActivateChoice(const char* id) noexcept
{
    if (!id) return;
    std::lock_guard lk(m_lock);
    for (auto it = m_choices.begin(); it != m_choices.end(); ++it) {
        if (it->id == id) { m_choices.erase(it); return; }
    }
}

std::vector<nlohmann::json> ActivateChoiceRegistry::BuildChoicesJSON()
{
    std::lock_guard lk(m_lock);
    std::vector<nlohmann::json> out;
    out.reserve(m_choices.size());
    std::uint32_t n = 1;
    for (auto& e : m_choices) {
        out.push_back({ {"id", e.id}, {"label", e.label}, {"icon", e.icon},
                        {"index", n}, {"key", std::to_string(n)} });
        ++n;
    }
    return out;
}

bool ActivateChoiceRegistry::TriggerByIndex(std::uint32_t index)
{
    PRISMA_HUD_API::ActivateChoiceCallback cb = nullptr;
    void* ctx = nullptr;
    {
        std::lock_guard lk(m_lock);
        if (index < 1 || index > m_choices.size()) return false;
        cb  = m_choices[index - 1].cb;
        ctx = m_choices[index - 1].ctx;
    }
    if (!cb) return false;
    cb(ctx);
    return true;
}
