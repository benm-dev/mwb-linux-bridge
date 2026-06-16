#pragma once
#include "Protocol.h"

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

    // Warps cursor to an absolute pixel position using relative uinput events.
    void WarpCursor(int px, int py);

    void SetScreenSize(int width, int height);

private:
    int m_fd;
    int m_screenW{1920};
    int m_screenH{1080};
    int m_wheelRemainder{0};
    int m_hwheelRemainder{0};
};

}
