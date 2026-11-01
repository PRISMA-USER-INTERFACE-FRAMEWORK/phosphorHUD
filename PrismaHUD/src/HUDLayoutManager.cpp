#include "HUDLayoutManager.h"
#include "HUDIconCatalog.h"
#include <cctype>
#include <shlobj.h>

namespace {
    std::filesystem::path DocumentsPath()
    {
        wchar_t buf[MAX_PATH]{};
        if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_PERSONAL, nullptr, SHGFP_TYPE_CURRENT, buf)))
            return std::filesystem::path(buf);
        return {};
    }

    static void MigrateToFractions(nlohmann::json& pos)
    {
        constexpr float kRefW = 1920.f, kRefH = 1080.f;
        auto migrateField = [&](const char* key, float ref) {
            if (pos.contains(key) && pos[key].get<float>() > 2.f)
                pos[key] = pos[key].get<float>() / ref;
        };
        migrateField("x", kRefW);
        migrateField("y", kRefH);
    }

    static bool IsSanePos(const nlohmann::json& p)
    {
        auto val = [&](const char* k, float def) {
            return p.contains(k) ? p[k].get<float>() : def;
        };
        float x = val("x", 0.f), y = val("y", 0.f), s = val("s", 1.f), r = val("r", 0.f);
        if (!(s > 0.19f) || s > 3.01f)  return false;
        if (r != 0.f && r != 90.f && r != 180.f && r != 270.f) return false;
        if (x < -0.1f || x > 1.001f)    return false;
        if (y < -0.1f || y > 1.001f)    return false;
        return true;
    }

    static void ClampExtent(float& x, float& y, float s, float w, float h)
    {
        constexpr float kMinVisible = 0.25f;
        auto clampAxis = [](float& v, float extent) {
            if (!(extent > 0.f)) return;
            const float keep = extent * kMinVisible;
            const float lo = keep - extent, hi = 1.f - keep;
            if (lo > hi) { v = 0.f; return; }
            if (v < lo)  v = lo;
            else if (v > hi) v = hi;
        };
        clampAxis(x, w * s);
        clampAxis(y, h * s);
    }
}

HUDLayoutManager* HUDLayoutManager::GetSingleton()
{
    static HUDLayoutManager s_instance;
    return &s_instance;
}

std::filesystem::path HUDLayoutManager::LayoutPath() const
{
    return DocumentsPath() / "My Games" / "Fallout4" / "PrismaHUD" / "layout.json";
}

std::filesystem::path HUDLayoutManager::SlotsPath() const
{
    return DocumentsPath() / "My Games" / "Fallout4" / "PrismaHUD" / "slots.json";
}

std::filesystem::path HUDLayoutManager::SettingsPath() const
{
    return DocumentsPath() / "My Games" / "Fallout4" / "PrismaHUD" / "settings.json";
}

std::filesystem::path HUDLayoutManager::ExportsDir() const
{
    return DocumentsPath() / "My Games" / "Fallout4" / "PrismaHUD" / "exports";
}

namespace {
    std::string SanitizeStem(std::string_view in)
    {
        std::string out;
        for (char c : in) {
            if (out.size() >= 48) break;
            const bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                            (c >= '0' && c <= '9') || c == ' ' || c == '_' || c == '-';
            out += ok ? c : '_';
        }
        while (!out.empty() && (out.back() == ' ' || out.back() == '.'))
            out.pop_back();
        while (!out.empty() && out.front() == ' ')
            out.erase(out.begin());

        static constexpr const char* kReserved[] = {
            "CON", "PRN", "AUX", "NUL",
            "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
            "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9",
        };
        std::string upper;
        for (char c : out) upper += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        for (const char* r : kReserved) {
            if (upper == r) { out += '_'; break; }
        }
        return out;
    }
}

void HUDLayoutManager::LoadSettings()
{
    auto path = SettingsPath();
    if (std::filesystem::exists(path)) {
        try {
            std::ifstream f(path);
            auto j = nlohmann::json::parse(f);
            std::unique_lock lock(m_lock);
            if (j.contains("hideKey"))    m_hideKey    = j["hideKey"].get<std::uint32_t>();
            if (j.contains("combatOnly")) m_combatOnly = j["combatOnly"].get<bool>();
        } catch (const std::exception& e) {
            logger::error("PrismaHUD: failed to parse settings.json: {}", e.what());
        }
    }
    if (m_hideKey == 0) m_hideKey = 0x71;
    RecomputeMaster();
    logger::info("PrismaHUD: settings loaded (hideKey=0x{:X}, combatOnly={}, manualHidden={})",
                 m_hideKey, m_combatOnly, m_manualHidden);
}

void HUDLayoutManager::SaveSettings() const
{
    static std::mutex s_settingsFileMx;
    std::scoped_lock fileLock(s_settingsFileMx);

    try {
        auto path = SettingsPath();
        std::filesystem::create_directories(path.parent_path());

        nlohmann::json out;
        {
            std::shared_lock lock(m_lock);
            out["hideKey"]    = m_hideKey;
            out["combatOnly"] = m_combatOnly;
        }

        auto tmp = path;
        tmp += ".tmp";
        {
            std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
            if (!f) {
                logger::error("PrismaHUD: cannot open {} for settings write", tmp.string());
                return;
            }
            f << out.dump(2);
            f.flush();
            if (!f) {
                logger::error("PrismaHUD: settings write failed for {}", tmp.string());
                return;
            }
        }

        std::error_code ec;
        std::filesystem::rename(tmp, path, ec);
        if (ec) {
            std::error_code removeEc;
            std::filesystem::remove(path, removeEc);
            ec.clear();
            std::filesystem::rename(tmp, path, ec);
        }
        if (ec) {
            logger::error("PrismaHUD: could not replace settings file {}: {}", path.string(), ec.message());
            std::error_code cleanupEc;
            std::filesystem::remove(tmp, cleanupEc);
        }
    } catch (const std::exception& e) {
        logger::error("PrismaHUD: SaveSettings failed: {}", e.what());
    } catch (...) {
        logger::error("PrismaHUD: SaveSettings failed with an unknown exception");
    }
}

void HUDLayoutManager::RecomputeMaster()
{
    bool visible;
    {
        std::shared_lock lock(m_lock);
        visible = !m_manualHidden && (!m_combatOnly || m_inCombat);
    }
    bool changed = (m_masterVisible.exchange(visible) != visible);
    if (changed) {
        std::unique_lock lock(m_lock);
        ReapplyAllVisibilityLocked();
    }
}

void HUDLayoutManager::ReapplyAllVisibilityLocked()
{
    if (!m_api) return;
    for (auto& [view, entry] : m_widgets)
        ApplyEffectiveVisibility(entry.id);
}

void HUDLayoutManager::SetMenuSuppressed(bool on)
{
    bool changed = (m_menuSuppressed.exchange(on) != on);
    if (!changed) return;
    std::unique_lock lock(m_lock);
    ReapplyAllVisibilityLocked();
    logger::info("PrismaHUD: HUD {} (Workshop/Terminal/Crafting menu {})",
                 on ? "suppressed" : "restored", on ? "opened" : "closed");
}

void HUDLayoutManager::ToggleMasterHidden()
{
    { std::unique_lock lock(m_lock); m_manualHidden = !m_manualHidden; }
    logger::info("PrismaHUD: HUD {} (hide key)", m_manualHidden ? "hidden" : "shown");
    RecomputeMaster();
}

void HUDLayoutManager::SetCombatOnly(bool on)
{
    { std::unique_lock lock(m_lock); m_combatOnly = on; }
    RecomputeMaster();
    std::thread([this]() { SaveSettings(); }).detach();
    logger::info("PrismaHUD: combat-only HUD {}", on ? "enabled" : "disabled");
}

void HUDLayoutManager::SetHideKey(std::uint32_t vk)
{
    if (vk == 0) return;
    { std::unique_lock lock(m_lock); m_hideKey = vk; }
    std::thread([this]() { SaveSettings(); }).detach();
    logger::info("PrismaHUD: hide key rebound to 0x{:X}", vk);
}

void HUDLayoutManager::UpdateCombatState(bool inCombat)
{
    {
        std::unique_lock lock(m_lock);
        if (inCombat == m_inCombat) return;
        m_inCombat = inCombat;
    }
    if (m_combatOnly) RecomputeMaster();
}

bool HUDLayoutManager::ShouldPollCombat()
{
    const std::uint64_t now = GetTickCount64();
    if (now - m_lastCombatTick < 200) return false;
    m_lastCombatTick = now;
    return true;
}

void HUDLayoutManager::RegisterWidget(PrismaView view, PRISMA_HUD_API::WidgetDesc desc) noexcept
{
    if (!view || !desc.id || desc.id[0] == '\0')
    {
        logger::warn("PrismaHUD: rejected invalid widget registration (view={}, id={})",
                     view, desc.id ? desc.id : "<null>");
        return;
    }

    const std::string id(desc.id);
    const std::string displayName = (desc.displayName && desc.displayName[0] != '\0')
        ? std::string(desc.displayName)
        : id;

    std::unique_lock lock(m_lock);
    for (auto it = m_widgets.begin(); it != m_widgets.end();)
    {
        if (it->second.id == id)
            it = m_widgets.erase(it);
        else
            ++it;
    }

    WidgetEntry e;
    e.id = id;
    e.displayName = displayName;
    e.view = view;
    e.defaultX = desc.defaultX;
    e.defaultY = desc.defaultY;
    e.defaultW = desc.defaultW;
    e.defaultH = desc.defaultH;
    e.x = desc.defaultX;
    e.y = desc.defaultY;
    e.s = 1.f;

    m_widgets[view] = std::move(e);
    logger::info("PrismaHUD: registered widget '{}' (view={})", id, view);

    if (m_api && m_api->IsValid(view))
    {
        m_api->BindUIEvent(view, "reportPosition", [](const char* json) {
            try {
                auto j = nlohmann::json::parse(json ? json : "{}");
                auto widgetId = j.at("id").get<std::string>();
                HUDLayoutManager::GetSingleton()->UpdatePositionFromReport(widgetId.c_str(), json);
            } catch (...) {}
        });
        m_api->BindUIEvent(view, "reportVisibility", [](const char* json) {
            try {
                auto j = nlohmann::json::parse(json ? json : "{}");
                auto widgetId = j.at("id").get<std::string>();
                bool visible = j.at("visible").get<bool>();
                HUDLayoutManager::GetSingleton()->SetRuntimeVisible(widgetId.c_str(), visible);
            } catch (...) {}
        });
    }

    auto& we = m_widgets[view];
    if (auto it = m_savedPos.find(id); it != m_savedPos.end())
    {
        auto& p = it->second;
        if (p.contains("x")) we.x = p["x"].get<float>();
        if (p.contains("y")) we.y = p["y"].get<float>();
        if (p.contains("s")) we.s = p["s"].get<float>();
        if (p.contains("r")) we.r = p["r"].get<float>();
        RecoverOffscreen(we, p);
    }

    ApplyToView(we);
    PushIconToView(we);
    ApplyEffectiveVisibility(id, true);
}

void HUDLayoutManager::UnregisterWidget(PrismaView view) noexcept
{
    std::unique_lock lock(m_lock);
    m_widgets.erase(view);
}

void HUDLayoutManager::ApplyLayout(PrismaView view) noexcept
{
    std::shared_lock lock(m_lock);
    if (auto it = m_widgets.find(view); it != m_widgets.end()) {
        ApplyToView(it->second);
        PushIconToView(it->second);
    }
}

void HUDLayoutManager::RecoverOffscreen(WidgetEntry& e, nlohmann::json& saved) const
{
    const bool quarter = (e.r == 90.f || e.r == 270.f);
    float x = e.x, y = e.y;
    ClampExtent(x, y, e.s, quarter ? e.defaultH : e.defaultW,
                            quarter ? e.defaultW : e.defaultH);
    if (x == e.x && y == e.y)
        return;

    logger::warn("PrismaHUD: '{}' was off-screen at ({:.3f}, {:.3f}) scale {:.2f}; pulled back to ({:.3f}, {:.3f})",
                 e.id, e.x, e.y, e.s, x, y);
    e.x = x;
    e.y = y;
    saved["x"] = x;
    saved["y"] = y;
}

void HUDLayoutManager::ApplyToView(const WidgetEntry& e) const
{
    if (!m_api || !m_api->IsValid(e.view))
        return;

    const bool quarter = (e.r == 90.f || e.r == 270.f);
    const float ew = quarter ? e.defaultH : e.defaultW;
    const float eh = quarter ? e.defaultW : e.defaultH;

    float x = e.x, y = e.y;
    ClampExtent(x, y, e.s, ew, eh);

    auto payload = nlohmann::json{
        {"x", x}, {"y", y}, {"s", e.s}, {"r", e.r}
    }.dump();

    m_api->InteropCall(e.view, "setHUDPosition", payload.c_str());
}

void HUDLayoutManager::ResetTransientForLoad()
{
    std::unique_lock lock(m_lock);
    m_runtimeVisible.clear();
    m_manualHidden = false;
    m_inCombat = false;
    m_lastCombatTick = 0;
    m_menuSuppressed.store(false, std::memory_order_release);
    m_masterVisible.store(!m_combatOnly, std::memory_order_release);
}

void HUDLayoutManager::LoadAndApply()
{
    LoadLayoutFile();
    LoadSlotsFile();

    std::unique_lock lock(m_lock);
    for (auto& [view, entry] : m_widgets) {
        if (auto it = m_savedPos.find(entry.id); it != m_savedPos.end()) {
            auto& p = it->second;
            if (p.contains("x")) entry.x = p["x"].get<float>();
            if (p.contains("y")) entry.y = p["y"].get<float>();
            if (p.contains("s")) entry.s = p["s"].get<float>();
            if (p.contains("r")) entry.r = p["r"].get<float>();
            RecoverOffscreen(entry, p);
        }

        ApplyToView(entry);
        ApplyEffectiveVisibility(entry.id, true);
    }
}

void HUDLayoutManager::LoadLayoutFile()
{
    {
        std::unique_lock lock(m_lock);
        m_savedPos.clear();
        m_userVisible.clear();
    }

    auto path = LayoutPath();
    if (!std::filesystem::exists(path))
        return;

    try {
        std::ifstream f(path);
        auto j = nlohmann::json::parse(f);

        std::unique_lock lock(m_lock);
        if (j.contains("visibility") && j["visibility"].is_object()) {
            for (auto& [id, v] : j["visibility"].items())
                m_userVisible[id] = v.get<bool>();
            j.erase("visibility");
        }

        for (auto& [id, pos] : j.items()) {
            MigrateToFractions(pos);
            if (!IsSanePos(pos)) {
                logger::warn("PrismaHUD: dropping corrupt saved position for '{}' (using default)", id);
                continue;
            }
            m_savedPos[id] = pos;
        }

        logger::info("PrismaHUD: loaded layout from {}", path.string());
    } catch (const std::exception& e) {
        logger::error("PrismaHUD: failed to parse layout file ({}): {}", path.string(), e.what());
        std::error_code ec;
        auto bad = path;
        bad += ".corrupt";
        std::filesystem::remove(bad, ec);
        std::filesystem::rename(path, bad, ec);
        if (!ec) logger::error("PrismaHUD: moved corrupt layout aside to {}", bad.string());
    }
}

void HUDLayoutManager::SaveLayoutFile() const
{
    try {
        auto path = LayoutPath();
        std::filesystem::create_directories(path.parent_path());

        nlohmann::json out;
        {
            std::shared_lock lock(m_lock);
            for (auto& [id, pos] : m_savedPos)
                out[id] = pos;
            nlohmann::json visObj;
            for (auto& [id, v] : m_userVisible)
                visObj[id] = v;
            out["visibility"] = visObj;
        }

        static std::mutex s_fileMx;
        std::scoped_lock fileLock(s_fileMx);

        auto tmp = path;
        tmp += ".tmp";
        {
            std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
            if (!f) { logger::error("PrismaHUD: cannot open {} for write", tmp.string()); return; }
            f << out.dump(2);
            if (!f) { logger::error("PrismaHUD: write failed for {}", tmp.string()); return; }
        }
        std::error_code ec;
        std::filesystem::rename(tmp, path, ec);
        if (ec) {
            std::filesystem::remove(path, ec);
            std::filesystem::rename(tmp, path, ec);
            if (ec) logger::error("PrismaHUD: could not replace {}: {}", path.string(), ec.message());
        }
    } catch (const std::exception& e) {
        logger::error("PrismaHUD: SaveLayoutFile exception: {}", e.what());
    } catch (...) {
        logger::error("PrismaHUD: SaveLayoutFile unknown exception");
    }
}

void HUDLayoutManager::SaveFromJS(const char* json)
{
    try {
        auto j = nlohmann::json::parse(json);
        {
            std::unique_lock lock(m_lock);
            for (auto& [id, pos] : j.items()) {
                if (!IsSanePos(pos)) continue;

                auto& saved = m_savedPos[id];
                if (pos.contains("x")) saved["x"] = pos["x"];
                if (pos.contains("y")) saved["y"] = pos["y"];
                if (pos.contains("s")) saved["s"] = pos["s"];
                if (pos.contains("r")) saved["r"] = pos["r"];

                for (auto& [view, entry] : m_widgets) {
                    if (entry.id == id) {
                        if (pos.contains("x")) entry.x = pos["x"].get<float>();
                        if (pos.contains("y")) entry.y = pos["y"].get<float>();
                        if (pos.contains("s")) entry.s = pos["s"].get<float>();
                        if (pos.contains("r")) entry.r = pos["r"].get<float>();
                        ApplyToView(entry);
                    }
                }
            }
        }

        SaveLayoutFile();
        logger::info("PrismaHUD: layout saved.");
    } catch (const std::exception& e) {
        logger::error("PrismaHUD: SaveFromJS failed: {}", e.what());
    } catch (...) {
        logger::error("PrismaHUD: SaveFromJS failed with an unknown exception");
    }
}

void HUDLayoutManager::UpdatePositionFromReport(const char* widgetId, const char* json)
{
    try {
        auto j = nlohmann::json::parse(json);
        if (!IsSanePos(j)) return;
        std::unique_lock lock(m_lock);
        for (auto& [view, entry] : m_widgets) {
            if (std::string(entry.id) == widgetId) {
                if (j.contains("x")) entry.x = j["x"].get<float>();
                if (j.contains("y")) entry.y = j["y"].get<float>();
                if (j.contains("s")) entry.s = j["s"].get<float>();
                if (j.contains("r")) entry.r = j["r"].get<float>();
                break;
            }
        }
    } catch (...) {}
}

void HUDLayoutManager::LiveMove(const char* json)
{
    try {
        auto j = nlohmann::json::parse(json);
        std::string id = j.at("id").get<std::string>();

        std::unique_lock lock(m_lock);
        for (auto& [view, entry] : m_widgets) {
            if (std::string(entry.id) != id) continue;
            if (j.contains("x")) entry.x = j["x"].get<float>();
            if (j.contains("y")) entry.y = j["y"].get<float>();
            if (j.contains("s")) entry.s = j["s"].get<float>();
            if (j.contains("r")) entry.r = j["r"].get<float>();
            ApplyToView(entry);
            break;
        }
    } catch (const std::exception& e) {
        logger::error("PrismaHUD: LiveMove failed: {}", e.what());
    }
}

void HUDLayoutManager::ShowAllWidgets()
{
    if (!m_api) return;
    std::unique_lock lock(m_lock);
    for (auto& [view, entry] : m_widgets) {
        m_api->Show(view);
        ApplyToView(entry);
    }
}

void HUDLayoutManager::RestoreWidgetVisibility()
{
    if (!m_api) return;
    std::unique_lock lock(m_lock);
    for (auto& [view, entry] : m_widgets)
        ApplyEffectiveVisibility(entry.id);
}

void HUDLayoutManager::SetEditorPreview(bool on)
{
    if (!m_api) return;
    std::shared_lock lock(m_lock);
    for (auto& [view, entry] : m_widgets)
        if (m_api->IsValid(view))
            m_api->InteropCall(view, "setEditorPreview", on ? "1" : "0");
}

void HUDLayoutManager::ApplyEffectiveVisibility(const std::string& id, bool preserveIfRuntimeUnknown)
{
    if (!m_api) return;

    bool userOk = true;
    if (auto it = m_userVisible.find(id); it != m_userVisible.end())
        userOk = it->second;
    bool runtimeOk = true;
    const auto runtimeIt = m_runtimeVisible.find(id);
    const bool runtimeKnown = runtimeIt != m_runtimeVisible.end();
    if (runtimeKnown)
        runtimeOk = runtimeIt->second;

    static constexpr std::string_view kMenuViews[] = { "prisma_quickloot" };
    bool masterExempt = false;
    for (auto v : kMenuViews) if (v == id) { masterExempt = true; break; }
    bool show = userOk && runtimeOk && !m_menuSuppressed.load() && (masterExempt || m_masterVisible.load());

    if (show && preserveIfRuntimeUnknown && !runtimeKnown)
        return;

    for (auto& [view, entry] : m_widgets) {
        if (entry.id == id) {
            if (show) m_api->Show(view);
            else      m_api->Hide(view);
            break;
        }
    }
}

void HUDLayoutManager::SetWidgetVisible(const char* id, bool visible)
{
    if (!m_api) return;
    {
        std::unique_lock lock(m_lock);
        m_userVisible[id] = visible;
        ApplyEffectiveVisibility(id);
    }
    std::thread([this]() { SaveLayoutFile(); }).detach();
}

void HUDLayoutManager::SetRuntimeVisible(const char* id, bool visible)
{
    if (!m_api) return;
    std::unique_lock lock(m_lock);
    m_runtimeVisible[id] = visible;
    ApplyEffectiveVisibility(id);
}

std::string HUDLayoutManager::BuildEditorPayload() const
{
    std::shared_lock lock(m_lock);

    nlohmann::json arr = nlohmann::json::array();
    for (auto& [view, entry] : m_widgets) {
        bool visible = true;
        if (auto it = m_userVisible.find(entry.id); it != m_userVisible.end())
            visible = it->second;

        arr.push_back({
            {"id",       entry.id},
            {"name",     entry.displayName},
            {"x",        entry.x},
            {"y",        entry.y},
            {"s",        entry.s},
            {"r",        entry.r},
            {"canSetIcon", m_iconCapable.count(entry.id) > 0},
            {"icon",       [&]() -> std::string {
                 auto it = m_savedPos.find(entry.id);
                 if (it == m_savedPos.end() || !it->second.contains("icon")) return {};
                 const auto file = it->second["icon"].get<std::string>();
                 return prismhud::icons::IsAllowed(file) ? file : std::string{};
             }()},
            {"defaultX", entry.defaultX},
            {"defaultY", entry.defaultY},
            {"defaultW", entry.defaultW},
            {"defaultH", entry.defaultH},
            {"visible",  visible},
        });
    }
    return arr.dump();
}

std::string HUDLayoutManager::GetSlotsPayload() const
{
    std::shared_lock lock(m_lock);
    nlohmann::json arr = nlohmann::json::array();
    for (int i = 0; i < 4; ++i)
        arr.push_back({{"index", i}, {"name", m_slots[i].name.empty() ? (i == 0 ? "Default" : "") : m_slots[i].name}});
    return arr.dump();
}

std::string HUDLayoutManager::ExportLayout(const char* name)
{
    auto stem = SanitizeStem(name ? name : "");
    if (stem.empty()) {
        logger::warn("PrismaHUD: export rejected -- name sanitized to nothing");
        return {};
    }

    try {
        auto dir = ExportsDir();
        std::filesystem::create_directories(dir);

        nlohmann::json out;
        {
            std::shared_lock lock(m_lock);
            for (auto& [id, pos] : m_savedPos)
                out[id] = pos;
            nlohmann::json visObj;
            for (auto& [id, v] : m_userVisible)
                visObj[id] = v;
            out["visibility"] = visObj;
        }

        std::ofstream f(dir / (stem + ".json"));
        if (!f) { logger::error("PrismaHUD: cannot write export '{}'", stem); return {}; }
        f << out.dump(2);
        logger::info("PrismaHUD: exported layout as '{}'", stem);
        return stem;
    } catch (const std::exception& e) {
        logger::error("PrismaHUD: ExportLayout failed: {}", e.what());
        return {};
    }
}

std::string HUDLayoutManager::ListImportFiles() const
{
    nlohmann::json arr = nlohmann::json::array();
    try {
        auto dir = ExportsDir();
        if (std::filesystem::exists(dir)) {
            for (auto& e : std::filesystem::directory_iterator(dir)) {
                if (!e.is_regular_file()) continue;
                if (e.path().extension() != ".json") continue;
                arr.push_back(e.path().stem().string());
            }
        }
    } catch (const std::exception& e) {
        logger::error("PrismaHUD: ListImportFiles failed: {}", e.what());
    }
    return arr.dump();
}

void HUDLayoutManager::ImportLayout(const char* name)
{
    auto stem = SanitizeStem(name ? name : "");
    if (stem.empty()) return;

    auto path = ExportsDir() / (stem + ".json");
    if (!std::filesystem::exists(path)) {
        logger::warn("PrismaHUD: import '{}' does not exist", stem);
        return;
    }

    try {
        std::ifstream f(path);
        auto j = nlohmann::json::parse(f);

        {
            std::unique_lock lock(m_lock);
            if (j.contains("visibility") && j["visibility"].is_object()) {
                for (auto& [id, v] : j["visibility"].items())
                    m_userVisible[id] = v.get<bool>();
                j.erase("visibility");
            }

            int applied = 0, dropped = 0;
            for (auto& [id, pos] : j.items()) {
                MigrateToFractions(pos);
                if (!IsSanePos(pos)) { ++dropped; continue; }
                m_savedPos[id] = pos;
                ++applied;
                for (auto& [view, entry] : m_widgets) {
                    if (std::string(entry.id) != id) continue;
                    if (pos.contains("x")) entry.x = pos["x"].get<float>();
                    if (pos.contains("y")) entry.y = pos["y"].get<float>();
                    if (pos.contains("s")) entry.s = pos["s"].get<float>();
                    if (pos.contains("r")) entry.r = pos["r"].get<float>();
                    RecoverOffscreen(entry, m_savedPos[id]);
                    ApplyToView(entry);
                    PushIconToView(entry);
                }
            }
            ReapplyAllVisibilityLocked();
            logger::info("PrismaHUD: imported '{}' ({} applied, {} dropped as corrupt)",
                         stem, applied, dropped);
        }

        SaveLayoutFile();
    } catch (const std::exception& e) {
        logger::error("PrismaHUD: ImportLayout failed for '{}': {}", stem, e.what());
    }
}

std::string HUDLayoutManager::GetIconsPayload() const
{
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& icon : prismhud::icons::Enumerate())
        arr.push_back({ {"file", icon.file}, {"label", icon.label} });
    return arr.dump();
}

void HUDLayoutManager::SetIconCapable(const char* id)
{
    if (!id) return;
    std::unique_lock lock(m_lock);
    m_iconCapable.insert(id);
}

void HUDLayoutManager::SetWidgetIcon(const char* id, const char* file)
{
    if (!id) return;
    const std::string_view want = file ? file : "";

    if (!prismhud::icons::IsAllowed(want)) {
        logger::warn("PrismaHUD: rejected unsafe or missing icon '{}'", want);
        return;
    }

    {
        std::unique_lock lock(m_lock);
        m_savedPos[id]["icon"] = std::string(want);
        for (auto& [view, entry] : m_widgets) {
            if (std::string(entry.id) != id) continue;
            PushIconToView(entry);
            break;
        }
    }

    SaveLayoutFile();
    logger::info("PrismaHUD: widget '{}' icon set to '{}'", id, want.empty() ? "(default)" : want);
}

void HUDLayoutManager::PushIconToView(const WidgetEntry& e) const
{
    if (!m_api || !m_api->IsValid(e.view)) return;
    std::string file;
    if (auto it = m_savedPos.find(e.id); it != m_savedPos.end() && it->second.contains("icon")) {
        const auto saved = it->second["icon"].get<std::string>();
        if (prismhud::icons::IsAllowed(saved))
            file = saved;
        else if (!saved.empty())
            logger::warn("PrismaHUD: ignored unsafe or missing saved icon '{}' for '{}'", saved, e.id);
    }
    auto payload = nlohmann::json{ {"icon", file} }.dump();
    m_api->InteropCall(e.view, "setWidgetIcon", payload.c_str());
}

const HUDLayoutManager::Preset HUDLayoutManager::kPresets[] = {
    { "Defaults", 1.00f },
    { "Compact",  0.80f },
    { "Large",    1.25f },
};

std::string HUDLayoutManager::GetPresetsPayload() const
{
    nlohmann::json arr = nlohmann::json::array();
    for (auto& p : kPresets)
        arr.push_back({ {"name", p.name}, {"scale", p.scale} });
    return arr.dump();
}

void HUDLayoutManager::ApplyPreset(const char* name)
{
    if (!name) return;

    const Preset* preset = nullptr;
    for (auto& p : kPresets) {
        if (std::string_view(p.name) == name) { preset = &p; break; }
    }
    if (!preset) { logger::warn("PrismaHUD: unknown preset '{}'", name); return; }

    {
        std::unique_lock lock(m_lock);
        for (auto& [view, entry] : m_widgets) {
            entry.x = entry.defaultX;
            entry.y = entry.defaultY;
            entry.s = preset->scale;
            entry.r = 0.f;

            auto& saved = m_savedPos[entry.id];
            saved["x"] = entry.x;
            saved["y"] = entry.y;
            saved["s"] = entry.s;
            saved["r"] = entry.r;
            RecoverOffscreen(entry, saved);
            ApplyToView(entry);
        }
    }

    SaveLayoutFile();
    logger::info("PrismaHUD: applied preset '{}' (scale {:.2f})", preset->name, preset->scale);
}

void HUDLayoutManager::LoadSlot(int index)
{
    if (index < 0 || index >= 4) return;

    std::unique_lock lock(m_lock);
    auto& slot = m_slots[index];
    if (slot.layout.empty()) return;

    for (auto it = slot.layout.begin(); it != slot.layout.end(); ++it) {
        const std::string& id  = it->first;
        nlohmann::json&    pos = it->second;
        m_savedPos[id] = pos;
        for (auto& [view, entry] : m_widgets) {
            if (std::string(entry.id) == id) {
                if (pos.contains("x")) entry.x = pos["x"].get<float>();
                if (pos.contains("y")) entry.y = pos["y"].get<float>();
                if (pos.contains("s")) entry.s = pos["s"].get<float>();
                if (pos.contains("r")) entry.r = pos["r"].get<float>();
                ApplyToView(entry);
                PushIconToView(entry);
            }
        }
    }
    logger::info("PrismaHUD: loaded slot {}", index);
}

void HUDLayoutManager::SaveSlot(int index, const char* name)
{
    if (index < 0 || index >= 4) return;

    {
        std::unique_lock lock(m_lock);
        m_slots[index].name   = name ? name : "";
        m_slots[index].layout = m_savedPos;
    }

    std::thread([this]() { SaveSlotsFile(); }).detach();
    logger::info("PrismaHUD: saved slot {}", index);
}

void HUDLayoutManager::RenameSlot(int index, const char* name)
{
    if (index < 0 || index >= 4) return;

    {
        std::unique_lock lock(m_lock);
        m_slots[index].name = name ? name : "";
    }

    std::thread([this]() { SaveSlotsFile(); }).detach();
    logger::info("PrismaHUD: renamed slot {} to '{}'", index, name ? name : "");
}

void HUDLayoutManager::SaveSlotsFile() const
{
    static std::mutex s_slotsFileMx;
    std::scoped_lock fileLock(s_slotsFileMx);

    try {
        auto path = SlotsPath();
        std::filesystem::create_directories(path.parent_path());

        nlohmann::json out = nlohmann::json::array();
        {
            std::shared_lock lock(m_lock);
            for (int i = 0; i < 4; ++i) {
                nlohmann::json slotJson;
                slotJson["name"] = m_slots[i].name.empty() ? (i == 0 ? "Default" : "") : m_slots[i].name;
                nlohmann::json layout;
                for (auto it = m_slots[i].layout.begin(); it != m_slots[i].layout.end(); ++it)
                    layout[it->first] = it->second;
                slotJson["layout"] = layout;
                out.push_back(slotJson);
            }
        }

        auto tmp = path;
        tmp += ".tmp";
        {
            std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
            if (!f) {
                logger::error("PrismaHUD: cannot open {} for slots write", tmp.string());
                return;
            }
            f << out.dump(2);
            f.flush();
            if (!f) {
                logger::error("PrismaHUD: slots write failed for {}", tmp.string());
                return;
            }
        }

        std::error_code ec;
        std::filesystem::rename(tmp, path, ec);
        if (ec) {
            std::error_code removeEc;
            std::filesystem::remove(path, removeEc);
            ec.clear();
            std::filesystem::rename(tmp, path, ec);
        }
        if (ec) {
            logger::error("PrismaHUD: could not replace slots file {}: {}", path.string(), ec.message());
            std::error_code cleanupEc;
            std::filesystem::remove(tmp, cleanupEc);
        }
    } catch (const std::exception& e) {
        logger::error("PrismaHUD: SaveSlotsFile failed: {}", e.what());
    } catch (...) {
        logger::error("PrismaHUD: SaveSlotsFile failed with an unknown exception");
    }
}

void HUDLayoutManager::LoadSlotsFile()
{
    {
        std::unique_lock lock(m_lock);
        for (auto& slot : m_slots) {
            slot.name.clear();
            slot.layout.clear();
        }
        m_slots[0].name = "Default";
    }

    auto path = SlotsPath();
    if (!std::filesystem::exists(path))
        return;

    try {
        std::ifstream f(path);
        auto j = nlohmann::json::parse(f);

        std::unique_lock lock(m_lock);
        for (int i = 0; i < 4 && i < static_cast<int>(j.size()); ++i) {
            auto& s = j[i];
            m_slots[i].name = s.value("name", i == 0 ? "Default" : "");
            m_slots[i].layout.clear();
            if (s.contains("layout") && s["layout"].is_object()) {
                for (auto it = s["layout"].begin(); it != s["layout"].end(); ++it)
                    m_slots[i].layout[it.key()] = it.value();
            }
        }
    } catch (const std::exception& e) {
        logger::error("PrismaHUD: failed to load slots file: {}", e.what());
    }
}
