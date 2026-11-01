#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace prismhud::icons
{
struct Choice
{
    std::string file;
    std::string label;
};

std::vector<Choice> Enumerate();

bool IsAllowed(std::string_view file);
}
