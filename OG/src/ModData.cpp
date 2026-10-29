#include "ModData.h"
#include <algorithm>
#include <cctype>
#include <cmath>

namespace Phosphor::ModData {
namespace {
std::string Trim(std::string s) {
    const auto a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return {};
    return s.substr(a, s.find_last_not_of(" \t\r\n") - a + 1);
}
std::optional<double> Number(const std::string& text) {
    try {
        std::size_t used = 0;
        const double value = std::stod(text, &used);
        if (used == text.size() && std::isfinite(value)) return value;
    } catch (...) {}
    return std::nullopt;
}
// KARMA's title table (its Pip-Boy tab StringArray; the title AVs hold the index).
constexpr const char* kKarmaTitles[] = {
    "Kindhearted", "Drifter", "Delinquent", "Samaritan", "Scavenger", "Thief", "Honorable", "Sightseer",
    "Deciever", "Benevolent", "Opportunist", "Outlaw", "Peacekeeper", "Wanderer", "Defiler", "Righteous",
    "Adventurer", "Marauder", "Earth Angel", "Gray Stranger", "Abomination", "Messiah", "True Mortal", "Devil"};
// REPUTATION's title table (its Pip-Boy tab StringArray; the per-faction tab AVs hold the index).
constexpr const char* kRepTitles[] = {
    "Neutral", "Accepted", "Liked", "Idolized", "Shunned", "Mixed", "Smiling Troublemaker",
    "Good-Natured Rascal", "Hated", "Sneering Punk", "Unpredictable", "Dark Hero", "Vilified",
    "Merciful Thug", "Soft-Hearted Devil", "Wild Child", "Cool Cat", "Jack", "Drag"};

}
std::string Slug(std::string_view text) {
    std::string out;
    for (char c : text) {
        if (std::isalnum(static_cast<unsigned char>(c))) out += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        else if (!out.empty() && out.back() != '-') out += '-';
    }
    while (!out.empty() && out.back() == '-') out.pop_back();
    return out.empty() ? "x" : out;
}

std::optional<std::string_view> KarmaTitle(int index) {
    if (index < 0 || index >= static_cast<int>(std::size(kKarmaTitles))) return std::nullopt;
    return kKarmaTitles[index];
}
std::optional<std::string_view> ReputationTitle(int index) {
    if (index < 0 || index >= static_cast<int>(std::size(kRepTitles))) return std::nullopt;
    return kRepTitles[index];
}
std::optional<Json> ParseChallenge(std::string_view input, const GlobalResolver& resolve) {
    const std::string text(input);
    const auto open = text.rfind('(');
    const auto slash = open == std::string::npos ? std::string::npos : text.find('/', open);
    const auto close = slash == std::string::npos ? std::string::npos : text.find(')', slash);
    if (close == std::string::npos) return std::nullopt;
    const auto curText = Trim(text.substr(open + 1, slash - open - 1));
    const auto max = Number(Trim(text.substr(slash + 1, close - slash - 1)));
    if (!max || *max <= 0) return std::nullopt;
    std::optional<double> cur;
    if (curText.starts_with("<Global=") && curText.ends_with(">")) {
        const auto edid = curText.substr(8, curText.size() - 9);
        if (resolve && !edid.empty()) cur = resolve(edid);
    } else {
        cur = Number(curText);
    }
    if (!cur || !std::isfinite(*cur)) return std::nullopt;
    const auto colon = text.find(':');
    auto name = Trim(text.substr(0, colon != std::string::npos && colon < open ? colon : open));
    if (name.empty()) return std::nullopt;
    return Json{{"id", "ch-" + Slug(name)}, {"name", name}, {"cur", std::clamp(*cur, 0.0, *max)}, {"max", *max}};
}
}
