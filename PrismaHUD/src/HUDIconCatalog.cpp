#include "HUDIconCatalog.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <unordered_map>

namespace prismhud::icons
{
namespace
{
constexpr std::size_t kMaxIcons = 512;

std::filesystem::path IconRoot()
{
    return std::filesystem::current_path() / "Data" / "PrismaUI_F4" / "views" / "icons";
}

std::string Lower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool ReservedDosDeviceName(std::string_view file)
{
    auto firstDot = file.find('.');
    std::string base = Lower(std::string(file.substr(0, firstDot)));
    while (!base.empty() && (base.back() == ' ' || base.back() == '.')) base.pop_back();

    if (base == "con" || base == "prn" || base == "aux" || base == "nul") return true;
    if (base.size() == 4 && base[3] >= '1' && base[3] <= '9') {
        const auto prefix = base.substr(0, 3);
        if (prefix == "com" || prefix == "lpt") return true;
    }
    return false;
}

bool SafeLeaf(std::string_view file)
{
    if (file.empty() || file.size() > 128) return false;
    if (file == "." || file == "..") return false;
    if (file.find('/') != std::string_view::npos ||
        file.find('\\') != std::string_view::npos ||
        file.find(':') != std::string_view::npos) return false;
    if (file.back() == ' ' || file.back() == '.') return false;
    for (unsigned char c : file) {
        if (c < 0x20) return false;
    }
    if (ReservedDosDeviceName(file)) return false;

    const std::filesystem::path p{ std::string(file) };
    if (p.has_parent_path() || p.is_absolute() || p.filename().string() != file) return false;

    const auto ext = Lower(p.extension().string());
    return ext == ".png" || ext == ".svg";
}

std::unordered_map<std::string, std::string> ManifestLabels(const std::filesystem::path& root)
{
    std::unordered_map<std::string, std::string> labels;
    try {
        std::ifstream file(root / "icons.json");
        if (!file) return labels;
        const auto doc = nlohmann::json::parse(file);
        if (!doc.contains("icons") || !doc["icons"].is_array()) return labels;

        for (const auto& row : doc["icons"]) {
            if (!row.is_object()) continue;
            const auto name = row.value("file", std::string{});
            const auto label = row.value("label", std::string{});
            if (!SafeLeaf(name) || label.empty() || label.size() > 96) continue;
            labels[name] = label;
        }
    } catch (...) {
    }
    return labels;
}
}

std::vector<Choice> Enumerate()
{
    std::vector<Choice> out;
    out.push_back({ "", "Default" });

    try {
        const auto root = IconRoot();
        if (!std::filesystem::is_directory(root)) return out;
        const auto labels = ManifestLabels(root);

        for (const auto& entry : std::filesystem::directory_iterator(root)) {
            if (out.size() >= kMaxIcons + 1) break;
            if (!entry.is_regular_file()) continue;

            const auto name = entry.path().filename().string();
            if (!SafeLeaf(name)) continue;

            std::string label = entry.path().stem().string();
            if (auto it = labels.find(name); it != labels.end()) label = it->second;
            out.push_back({ name, std::move(label) });
        }

        std::sort(out.begin() + 1, out.end(), [](const Choice& a, const Choice& b) {
            const auto al = Lower(a.label);
            const auto bl = Lower(b.label);
            if (al != bl) return al < bl;
            return Lower(a.file) < Lower(b.file);
        });
    } catch (...) {
    }

    return out;
}

bool IsAllowed(std::string_view file)
{
    if (file.empty()) return true;
    if (!SafeLeaf(file)) return false;

    try {
        const auto candidate = IconRoot() / std::string(file);
        return std::filesystem::is_regular_file(candidate);
    } catch (...) {
        return false;
    }
}
}
