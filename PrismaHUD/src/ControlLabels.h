#pragma once
#include <cstdint>
#include <string>

namespace ControlLabels {

    enum class Device { kKeyboard, kGamepad };

    enum class Style  { kXbox, kPlayStation };

    void SetFrameworkAPI(void* v9);

    void   NoteInputDevice(RE::INPUT_DEVICE device);
    bool   UsingGamepad();

    void  SetStyle(Style s);
    Style GetStyle();

    std::string ForUserEvent(const char* userEvent);

    std::uint32_t KeyboardCodeForUserEvent(const char* userEvent);

    std::uint32_t GamepadCodeForUserEvent(const char* userEvent);

    std::uint32_t GamepadCodeForHUDAction(const char* actionId);

    std::string ForHUDAction(const char* actionId);
}
