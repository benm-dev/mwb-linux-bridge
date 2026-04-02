#pragma once
#include "Protocol.h"
#include <chrono>

namespace mwb {

class InputManager {
public:
    InputManager();
    ~InputManager();

    // Sets up secure uinput kernel file descriptor mirroring Input Leap capabilities
    bool Initialize();

    // Maps MWB Windows coordinates / flags to EV_REL / EV_ABS and EV_KEY sequences
    void InjectMouse(const MouseData& data);

    // Translates Virtual Key Codes to EV_KEY definitions
    void InjectKeyboard(const KeyboardData& data);

    // Warps cursor to absolute pixel position via EV_ABS
    void WarpCursor(int px, int py);

private:
    int m_fd;
    int m_lastAbsX{-1};
    int m_lastAbsY{-1};
    std::chrono::steady_clock::time_point m_lastMouseTime;
};

}
