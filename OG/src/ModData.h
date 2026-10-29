#pragma once
#include "Core.h"
#include <functional>
#include <string_view>

namespace Phosphor::ModData {
std::string Slug(std::string_view text);
std::optional<std::string_view> KarmaTitle(int index);
std::optional<std::string_view> ReputationTitle(int index);
using GlobalResolver = std::function<std::optional<double>(std::string_view)>;
std::optional<Json> ParseChallenge(std::string_view text, const GlobalResolver& resolve = {});
}
