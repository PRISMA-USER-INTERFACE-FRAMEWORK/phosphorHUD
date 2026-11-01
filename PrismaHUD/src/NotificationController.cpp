#include "NotificationController.h"
#include "TrampolineGuard.h"

#include <Windows.h>
#include <algorithm>
#include <cctype>
#include <filesystem>

namespace {
    using ShowHUDMessageFn = void (*)(const char*, const char*, bool, bool);
    ShowHUDMessageFn g_origShowHUDMessage = nullptr;

    std::string ToLower(std::string_view in)
    {
        std::string out{ in };
        std::transform(out.begin(), out.end(), out.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return out;
    }

    std::string IniPath()
    {
        wchar_t buf[MAX_PATH]{};
        GetModuleFileNameW(GetModuleHandleW(L"PrismaHUD_F4.dll"), buf, MAX_PATH);
        return std::filesystem::path(buf).replace_extension(L".ini").string();
    }

    void Detour_ShowHUDMessage(const char* a_message, const char* a_sound, bool a_throttle, bool a_warning)
    {
        if (a_message && *a_message) {
            NotificationController::GetSingleton()->Enqueue(a_message, a_warning);
            return;
        }
        if (g_origShowHUDMessage) g_origShowHUDMessage(a_message, a_sound, a_throttle, a_warning);
    }
}

NotificationController* NotificationController::GetSingleton()
{
    static NotificationController s_instance;
    return &s_instance;
}

void NotificationController::InstallHook()
{
    static bool s_installed = false;
    if (s_installed) return;

    REL::Relocation<std::uintptr_t> target{ RE::ID::SendHUDMessage::ShowHUDMessage };

    EnsurePrismaHUDTrampoline();

    g_origShowHUDMessage = reinterpret_cast<ShowHUDMessageFn>(
        REL::GetTrampoline().write_jmp<5>(target.address(), &Detour_ShowHUDMessage));
    s_installed = true;
    logger::info("PrismaHUD notify: hooked SendHUDMessage::ShowHUDMessage -- all vanilla notifications "
                 "now render as Prisma toasts");
}

void NotificationController::LoadConfig()
{
    const auto ini = IniPath();
    char       buf[4096]{};

    m_startupGate = GetPrivateProfileIntA("Notifications", "startupGateEnabled", 1, ini.c_str()) != 0;

    GetPrivateProfileStringA("Notifications", "muteSeconds", "8.0", buf, sizeof(buf), ini.c_str());
    m_muteSeconds = std::strtof(buf, nullptr);

    constexpr const char* kDefaultPatterns =
        "leveled list|injection complete|added to level|"
        "initialized|initialization|has been installed|"
        "workshop registration|mcm control quest";
    GetPrivateProfileStringA("Notifications", "patterns", kDefaultPatterns, buf, sizeof(buf), ini.c_str());

    m_patterns.clear();
    std::string_view raw{ buf };
    std::size_t      start = 0;
    while (start <= raw.size()) {
        auto end = raw.find('|', start);
        if (end == std::string_view::npos) end = raw.size();
        auto token = raw.substr(start, end - start);
        while (!token.empty() && std::isspace(static_cast<unsigned char>(token.front()))) token.remove_prefix(1);
        while (!token.empty() && std::isspace(static_cast<unsigned char>(token.back())))  token.remove_suffix(1);
        if (!token.empty()) m_patterns.push_back(ToLower(token));
        start = end + 1;
    }

    logger::info("PrismaHUD notify: config startupGate={} muteSeconds={:.1f} patterns={}",
                 m_startupGate, m_muteSeconds, m_patterns.size());
}

void NotificationController::NoteGameLoaded()
{
    std::lock_guard lk(m_lock);
    m_loadTime   = std::chrono::steady_clock::now();
    m_gateActive = m_startupGate;
    m_pending.clear();
    m_lastText.clear();
}

bool NotificationController::ShouldSuppress(std::string_view text) const
{
    if (m_gateActive) {
        const auto elapsed = std::chrono::duration<float>(
            std::chrono::steady_clock::now() - m_loadTime).count();
        if (elapsed < m_muteSeconds) return true;
    }
    const auto lower = ToLower(text);
    for (const auto& p : m_patterns) {
        if (lower.find(p) != std::string::npos) return true;
    }
    return false;
}

void NotificationController::Enqueue(const char* a_text, bool a_warning)
{
    if (!a_text || !*a_text) return;

    std::lock_guard lk(m_lock);
    if (m_gateActive) {
        const auto elapsed = std::chrono::duration<float>(
            std::chrono::steady_clock::now() - m_loadTime).count();
        if (elapsed >= m_muteSeconds) m_gateActive = false;
    }
    if (ShouldSuppress(a_text)) return;

    const auto now = std::chrono::steady_clock::now();
    if (a_text == m_lastText && (now - m_lastTime) < kCoalesceWindow && !m_pending.empty()) {
        m_lastTime = now;
        m_pending.back().text = a_text;
        return;
    }
    m_lastText = a_text;
    m_lastTime = now;

    if (m_pending.size() >= kMaxQueued) m_pending.pop_front();
    m_pending.push_back({ std::string{ a_text }, a_warning });
}

void NotificationController::OnRequestUpdate()
{
    if (!m_api || !m_view) return;

    std::vector<Pending> batch;
    {
        std::lock_guard lk(m_lock);
        if (m_pending.empty()) return;
        batch.assign(m_pending.begin(), m_pending.end());
        m_pending.clear();
    }

    nlohmann::json arr = nlohmann::json::array();
    for (auto& p : batch)
        arr.push_back({ { "text", p.text }, { "warning", p.warning } });

    m_api->InteropCall(m_view, "pushToasts", nlohmann::json{ { "toasts", arr } }.dump().c_str());
}
