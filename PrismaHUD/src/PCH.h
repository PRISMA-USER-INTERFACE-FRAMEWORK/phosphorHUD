#pragma once

#pragma warning(push)
#include <RE/Fallout.h>
#include <F4SE/F4SE.h>

#include <filesystem>
#include <fstream>
#include <shared_mutex>
#include <unordered_map>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#pragma warning(pop)

#include <nlohmann/json.hpp>

using namespace std::literals;

namespace logger = spdlog;

#define DLLEXPORT __declspec(dllexport)
