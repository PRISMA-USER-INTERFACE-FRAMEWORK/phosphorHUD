#include "BindingRegistry.h"

#include <ShlObj.h>
#include <fstream>
#include <nlohmann/json.hpp>

BindingRegistry* BindingRegistry::GetSingleton()
{
    static BindingRegistry instance;
    return &instance;
}

std::filesystem::path BindingRegistry::KeybindsPath()
{
    std::filesystem::path dir;
    PWSTR docs = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents, 0, nullptr, &docs)) && docs)
        dir = docs;
    if (docs) CoTaskMemFree(docs);
    if (dir.empty()) dir = std::filesystem::current_path();
    dir /= L"My Games"; dir /= L"Fallout4"; dir /= L"PrismaHUD";
    std::error_code ec; std::filesystem::create_directories(dir, ec);
    return dir / L"keybinds.json";
}

void BindingRegistry::LoadFromDisk()
{
    std::unique_lock lock(_mutex);
    _savedOverrides.clear();
    const auto path = KeybindsPath();
    std::ifstream in(path);
    if (!in) { logger::info("BindingRegistry: no keybinds.json (defaults will apply)"); return; }
    try {
        nlohmann::json j; in >> j;
        if (j.is_object()) {
            for (auto it = j.begin(); it != j.end(); ++it) {
                if (it.value().is_number_unsigned())
                    _savedOverrides[it.key()] = it.value().get<uint32_t>();
                else if (it.value().is_object() && it.value().contains("key"))
                    _savedOverrides[it.key()] = it.value()["key"].get<uint32_t>();
            }
        }
        logger::info("BindingRegistry: loaded {} saved key override(s)", _savedOverrides.size());
    } catch (const std::exception& e) {
        logger::warn("BindingRegistry: keybinds.json parse failed ({}); using defaults", e.what());
    }
    for (auto& [id, b] : _bindings) {
        auto     ov   = _savedOverrides.find(id);
        uint32_t want = (ov != _savedOverrides.end()) ? ov->second : b.defaultKey;
        if (b.currentKey != want) { b.currentKey = want; ApplyBinding(b); }
    }
}

void BindingRegistry::ApplyBinding(Binding& b)
{
    auto* kh = KeyHandler::GetSingleton();
    if (b.downReg) { kh->Unregister(b.downReg); b.downReg = 0; }
    if (b.upReg)   { kh->Unregister(b.upReg);   b.upReg   = 0; }
    if (b.currentKey == 0) return;

    const auto onDown = b.onDown; void* const downCtx = b.downCtx;
    const bool reqShift = b.requireShift, suppressible = b.suppressible, consume = b.consume;
    if (onDown) {
        b.downReg = kh->Register(b.currentKey, KeyEventType::KEY_DOWN,
            [this, onDown, downCtx, reqShift, suppressible, consume]() -> bool {
                if (_captureMode.load()) return false;
                if (reqShift && !InputMods::ShiftDown()) return false;
                if (suppressible && _exclusiveCount.load() > 0) return false;
                onDown(downCtx);
                return consume;
            });
    }
    const auto onUp = b.onUp; void* const upCtx = b.upCtx;
    if (onUp) {
        b.upReg = kh->Register(b.currentKey, KeyEventType::KEY_UP,
            [this, onUp, upCtx, consume]() -> bool {
                if (_captureMode.load()) return false;
                onUp(upCtx); return consume;
            });
    }
}

void BindingRegistry::RegisterBinding(PRISMA_HUD_API::BindingDesc desc,
                                      PRISMA_HUD_API::BindingCallback onDown, void* downContext,
                                      PRISMA_HUD_API::BindingCallback onUp, void* upContext) noexcept
{
    if (!desc.id) return;
    std::unique_lock lock(_mutex);
    Binding& b = _bindings[desc.id];
    b.id           = desc.id;
    b.label        = desc.displayName ? desc.displayName : desc.id;
    b.category     = desc.category ? desc.category : "General";
    b.defaultKey   = desc.defaultKey;
    b.requireShift = desc.requireShift;
    b.consume      = desc.consume;
    b.suppressible = desc.suppressible;
    b.onDown = onDown; b.downCtx = downContext;
    b.onUp   = onUp;   b.upCtx   = upContext;
    if (b.currentKey == 0) {
        auto ov = _savedOverrides.find(desc.id);
        b.currentKey = (ov != _savedOverrides.end()) ? ov->second : desc.defaultKey;
    }
    ApplyBinding(b);
    logger::info("BindingRegistry: '{}' ({}) bound to {} [{}]", b.id, b.label,
                 KeyName(b.currentKey), b.currentKey == b.defaultKey ? "default" : "custom");
}

void BindingRegistry::UnregisterBinding(const char* id) noexcept
{
    if (!id) return;
    std::unique_lock lock(_mutex);
    auto it = _bindings.find(id);
    if (it == _bindings.end()) return;
    auto* kh = KeyHandler::GetSingleton();
    if (it->second.downReg) kh->Unregister(it->second.downReg);
    if (it->second.upReg)   kh->Unregister(it->second.upReg);
    _bindings.erase(it);
}

uint32_t BindingRegistry::GetBoundKey(const char* id) noexcept
{
    if (!id) return 0;
    std::shared_lock lock(_mutex);
    auto it = _bindings.find(id);
    return it != _bindings.end() ? it->second.currentKey : 0u;
}

void BindingRegistry::SetExclusiveInputMode(bool on) noexcept
{
    int result;
    if (on) {
        result = ++_exclusiveCount;
    } else {
        result = --_exclusiveCount;
        if (result < 0) { result = 0; _exclusiveCount.store(0); }
    }
    logger::info("BindingRegistry: exclusive input mode {} (count={})", on ? "ON" : "off", result);
}

bool BindingRegistry::SetKey(const char* id, uint32_t vk)
{
    if (!id) return false;
    {
        std::unique_lock lock(_mutex);
        auto it = _bindings.find(id);
        if (it == _bindings.end()) return false;
        it->second.currentKey = vk;
        ApplyBinding(it->second);
    }
    Save();
    logger::info("BindingRegistry: rebound '{}' -> {} ({})", id, KeyName(vk), vk);
    return true;
}

void BindingRegistry::ResetToDefault(const char* id)
{
    {
        std::unique_lock lock(_mutex);
        for (auto& [key, b] : _bindings) {
            if (id && *id && b.id != id) continue;
            b.currentKey = b.defaultKey;
            ApplyBinding(b);
        }
    }
    Save();
}

std::string BindingRegistry::FindConflict(uint32_t vk, bool requireShift, const char* excludeId)
{
    if (vk == 0) return {};
    std::shared_lock lock(_mutex);
    for (auto& [key, b] : _bindings) {
        if (excludeId && b.id == excludeId) continue;
        if (b.currentKey == vk && b.requireShift == requireShift) return b.label;
    }
    return {};
}

std::string BindingRegistry::CheckRebindConflict(const char* id, uint32_t vk)
{
    bool reqShift = false;
    {
        std::shared_lock lock(_mutex);
        auto it = _bindings.find(id ? id : "");
        if (it == _bindings.end()) return {};
        reqShift = it->second.requireShift;
    }
    return FindConflict(vk, reqShift, id);
}

std::string BindingRegistry::BuildListJSON()
{
    std::shared_lock lock(_mutex);
    nlohmann::json arr = nlohmann::json::array();
    for (auto& [key, b] : _bindings) {
        arr.push_back({
            {"id",           b.id},
            {"label",        b.label},
            {"category",     b.category},
            {"key",          b.currentKey},
            {"keyName",      KeyName(b.currentKey)},
            {"defaultKey",   b.defaultKey},
            {"requireShift", b.requireShift},
        });
    }
    return arr.dump();
}

void BindingRegistry::Save()
{
    nlohmann::json j = nlohmann::json::object();
    {
        std::shared_lock lock(_mutex);
        for (auto& [key, b] : _bindings)
            if (b.currentKey != b.defaultKey)
                j[b.id] = b.currentKey;
    }
    std::lock_guard<std::mutex> guard(_saveMutex);
    const auto path = KeybindsPath();
    const auto tmp  = std::filesystem::path(path) += L".tmp";
    try {
        { std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
          if (!out) { logger::error("BindingRegistry: cannot open {} for write", tmp.string()); return; }
          out << j.dump(2); }
        std::error_code ec;
        std::filesystem::rename(tmp, path, ec);
        if (ec) {
            std::filesystem::remove(path, ec);
            std::filesystem::rename(tmp, path, ec);
            if (ec) logger::error("BindingRegistry: rename keybinds.json failed: {}", ec.message());
        }
    } catch (const std::exception& e) {
        logger::error("BindingRegistry: save failed: {}", e.what());
    }
}

std::string BindingRegistry::KeyName(uint32_t vk)
{
    switch (vk) {
        case 0x00: return "(none)";
        case 0x08: return "Backspace"; case 0x09: return "Tab";   case 0x0D: return "Enter";
        case 0x1B: return "Esc";       case 0x20: return "Space"; case 0x2E: return "Delete";
        case 0x25: return "Left";      case 0x26: return "Up";    case 0x27: return "Right"; case 0x28: return "Down";
        case 0x2D: return "Insert";    case 0x24: return "Home";  case 0x23: return "End";
        case 0x21: return "PgUp";      case 0x22: return "PgDn";
        case 0xA0: return "LShift";    case 0xA1: return "RShift"; case 0x10: return "Shift";
        case 0x11: return "Ctrl";      case 0x12: return "Alt";
        case 0xBA: return ";";  case 0xBB: return "=";  case 0xBC: return ",";  case 0xBD: return "-";
        case 0xBE: return ".";  case 0xBF: return "/";  case 0xC0: return "`";
        case 0xDB: return "[";  case 0xDC: return "\\"; case 0xDD: return "]";  case 0xDE: return "'";
        default: break;
    }
    if (vk >= 0x30 && vk <= 0x39) return std::string(1, static_cast<char>(vk));
    if (vk >= 0x41 && vk <= 0x5A) return std::string(1, static_cast<char>(vk));
    if (vk >= 0x60 && vk <= 0x69) return "Num" + std::to_string(vk - 0x60);
    if (vk >= 0x70 && vk <= 0x87) return "F" + std::to_string(vk - 0x70 + 1);
    char b[16]; snprintf(b, sizeof(b), "0x%02X", vk);
    return b;
}
