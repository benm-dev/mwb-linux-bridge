#include "InputManager.h"
#include <iostream>
#include <algorithm>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <linux/uinput.h>
#include <unordered_map>

namespace mwb {

static std::unordered_map<uint16_t, uint16_t> s_vkToKey = {
    {0x41, 30}, {0x42, 48}, {0x43, 46}, {0x44, 32}, {0x45, 18}, {0x46, 33},
    {0x47, 34}, {0x48, 35}, {0x49, 23}, {0x4A, 36}, {0x4B, 37}, {0x4C, 38},
    {0x4D, 50}, {0x4E, 49}, {0x4F, 24}, {0x50, 25}, {0x51, 16}, {0x52, 19},
    {0x53, 31}, {0x54, 20}, {0x55, 22}, {0x56, 47}, {0x57, 17}, {0x58, 45},
    {0x59, 21}, {0x5A, 44},
    {0x30, 11}, {0x31, 2}, {0x32, 3}, {0x33, 4}, {0x34, 5}, {0x35, 6},
    {0x36, 7}, {0x37, 8}, {0x38, 9}, {0x39, 10},
    {0x0D, 28}, {0x1B, 1}, {0x08, 14}, {0x09, 15}, {0x20, 57},
    {0x25, 105}, {0x26, 103}, {0x27, 106}, {0x28, 108},
    {0x10, 42}, {0xA0, 42}, {0xA1, 54}, // Shift
    {0x11, 29}, {0xA2, 29}, {0xA3, 97}, // Ctrl
    {0x12, 56}, {0xA4, 56}, {0xA5, 100}, // Alt
    {0x2E, 111}, {0x2D, 110}, {0x24, 102}, {0x23, 107},
    {0x21, 104}, {0x22, 109},
    {0x70, 59}, {0x71, 60}, {0x72, 61}, {0x73, 62}, {0x74, 63}, {0x75, 64},
    {0x76, 65}, {0x77, 66}, {0x78, 67}, {0x79, 68}, {0x7A, 87}, {0x7B, 88}
};

InputManager::InputManager() : m_fd(-1) {}

InputManager::~InputManager() {
    if (m_fd >= 0) {
        ioctl(m_fd, UI_DEV_DESTROY);
        close(m_fd);
    }
}

bool InputManager::Initialize() {
    m_fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (m_fd < 0) {
        std::cerr << "ERR: Failed to open /dev/uinput. Ensure udev rules are correct or run as root." << std::endl;
        return false;
    }

    // Tell the input stack this device moves the cursor.
    // Without INPUT_PROP_POINTER, libinput/X11/Wayland ignores EV_ABS for cursor movement.
    ioctl(m_fd, UI_SET_PROPBIT, INPUT_PROP_POINTER);

    ioctl(m_fd, UI_SET_EVBIT, EV_KEY);
    ioctl(m_fd, UI_SET_EVBIT, EV_REL);
    ioctl(m_fd, UI_SET_EVBIT, EV_ABS);

    ioctl(m_fd, UI_SET_RELBIT, REL_X);
    ioctl(m_fd, UI_SET_RELBIT, REL_Y);
    ioctl(m_fd, UI_SET_RELBIT, REL_WHEEL);

    ioctl(m_fd, UI_SET_ABSBIT, ABS_X);
    ioctl(m_fd, UI_SET_ABSBIT, ABS_Y);

    struct uinput_user_dev uidev;
    memset(&uidev, 0, sizeof(uidev));
    strcpy(uidev.name, "Mouse Without Borders Virtual Client");
    uidev.id.bustype = BUS_USB;
    uidev.id.vendor  = 0x045E;
    uidev.id.product = 0x0001;

    uidev.absmin[ABS_X] = 0;
    uidev.absmax[ABS_X] = 65535;
    uidev.absmin[ABS_Y] = 0;
    uidev.absmax[ABS_Y] = 65535;

    for (int i = 0; i < 256; i++) {
        ioctl(m_fd, UI_SET_KEYBIT, i);
    }

    ioctl(m_fd, UI_SET_KEYBIT, BTN_LEFT);
    ioctl(m_fd, UI_SET_KEYBIT, BTN_RIGHT);
    ioctl(m_fd, UI_SET_KEYBIT, BTN_MIDDLE);

    if (write(m_fd, &uidev, sizeof(uidev)) < 0) {
        std::cerr << "ERR: Failed to write uinput dev setup." << std::endl;
        return false;
    }

    if (ioctl(m_fd, UI_DEV_CREATE) < 0) {
        std::cerr << "ERR: Failed to create uinput device." << std::endl;
        return false;
    }

    return true;
}

void InputManager::WarpCursor(int px, int py) {
    // Warp cursor to pixel position (px, py) using two-step REL injection.
    // ABS events are ignored by libinput for INPUT_PROP_POINTER devices, so we
    // use REL: first snap to (0,0) with a very large negative delta, then
    // apply the target offset. The compositor clamps REL movement at screen bounds.
    const int LARGE = 100000;
    struct input_event ev[4];

    // Step 1: snap to top-left corner
    memset(ev, 0, sizeof(ev));
    ev[0].type = EV_REL; ev[0].code = REL_X; ev[0].value = -LARGE;
    ev[1].type = EV_REL; ev[1].code = REL_Y; ev[1].value = -LARGE;
    ev[2].type = EV_SYN; ev[2].code = SYN_REPORT;
    write(m_fd, ev, sizeof(struct input_event) * 3);

    // Step 2: move to target (px, py) from (0, 0)
    memset(ev, 0, sizeof(ev));
    ev[0].type = EV_REL; ev[0].code = REL_X; ev[0].value = px;
    ev[1].type = EV_REL; ev[1].code = REL_Y; ev[1].value = py;
    ev[2].type = EV_SYN; ev[2].code = SYN_REPORT;
    write(m_fd, ev, sizeof(struct input_event) * 3);

    printf("[WARP] Cursor snapped to pixel (%d, %d)\n", px, py);
    fflush(stdout);
}

void InputManager::InjectMouse(const MouseData& data) {
    if (m_fd < 0) return;

    // Wire format (from real server observation):
    //   data.x     = absolute x (0-65535 across virtual desktop)
    //   data.flags = movement flags (usually 0; standard MOUSEEVENTF bits when buttons pressed)
    //   data.y     = absolute y (0-65535 across virtual desktop)
    //   data.wheel = scroll delta
    //
    // Coordinates span the VIRTUAL DESKTOP (all monitors combined).
    // Both screens 1920-wide: 0-32767=Windows (or Carbon), 32768-65535=Carbon (or Windows).
    // We scale the full range to our screen and track deltas.

    const int SCREEN_W = 1920;
    const int SCREEN_H = 1080;

    // Reset tracking if cursor has been away for >500ms (returned from Windows)
    auto now = std::chrono::steady_clock::now();
    if (m_lastAbsX >= 0) {
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastMouseTime).count();
        if (ms > 500) m_lastAbsX = -1;
    }
    m_lastMouseTime = now;

    struct input_event ev[12];
    memset(ev, 0, sizeof(ev));
    int count = 0;

    if (m_lastAbsX < 0) {
        // First packet after crossing — warp to the correct entry edge pixel position.
        // x < 32768: Carbon is left of Windows, so cursor entered from the right edge.
        // x >= 32768: Carbon is right of Windows, cursor entered from the left edge.
        int entryX = (data.x < 32768) ? (SCREEN_W - 1) : 0;
        int entryY = (static_cast<int>(data.y) * SCREEN_H) / 65535;
        entryY = std::max(0, std::min(SCREEN_H - 1, entryY));
        WarpCursor(entryX, entryY);
        m_lastAbsX = static_cast<int>(data.x);
        m_lastAbsY = static_cast<int>(data.y);
    } else {
        // Subsequent packets — use relative deltas in the raw 0-65535 space,
        // then scale to screen pixels for injection.
        int rawDx = static_cast<int>(data.x) - m_lastAbsX;
        int rawDy = static_cast<int>(data.y) - m_lastAbsY;

        int dx = (rawDx * SCREEN_W) / 65535;
        int dy = (rawDy * SCREEN_H) / 65535;

        if (dx != 0) { ev[count].type = EV_REL; ev[count].code = REL_X; ev[count].value = dx; count++; }
        if (dy != 0) { ev[count].type = EV_REL; ev[count].code = REL_Y; ev[count].value = dy; count++; }

        m_lastAbsX = static_cast<int>(data.x);
        m_lastAbsY = static_cast<int>(data.y);
    }

    // Button and wheel events via WM_* message code in wParam.
    switch (data.wParam) {
        case 0x0201: case 0x0203: // WM_LBUTTONDOWN / WM_LBUTTONDBLCLK
            ev[count].type = EV_KEY; ev[count].code = BTN_LEFT;   ev[count].value = 1; count++; break;
        case 0x0202:              // WM_LBUTTONUP
            ev[count].type = EV_KEY; ev[count].code = BTN_LEFT;   ev[count].value = 0; count++; break;
        case 0x0204: case 0x0206: // WM_RBUTTONDOWN / WM_RBUTTONDBLCLK
            ev[count].type = EV_KEY; ev[count].code = BTN_RIGHT;  ev[count].value = 1; count++; break;
        case 0x0205:              // WM_RBUTTONUP
            ev[count].type = EV_KEY; ev[count].code = BTN_RIGHT;  ev[count].value = 0; count++; break;
        case 0x0207: case 0x0209: // WM_MBUTTONDOWN / WM_MBUTTONDBLCLK
            ev[count].type = EV_KEY; ev[count].code = BTN_MIDDLE; ev[count].value = 1; count++; break;
        case 0x0208:              // WM_MBUTTONUP
            ev[count].type = EV_KEY; ev[count].code = BTN_MIDDLE; ev[count].value = 0; count++; break;
        case 0x020A: {            // WM_MOUSEWHEEL
            int16_t delta = static_cast<int16_t>((data.mouseData >> 16) & 0xFFFF);
            if (delta != 0) { ev[count].type = EV_REL; ev[count].code = REL_WHEEL; ev[count].value = delta / 120; count++; }
            break;
        }
        case 0x020E: {            // WM_MOUSEHWHEEL
            int16_t delta = static_cast<int16_t>((data.mouseData >> 16) & 0xFFFF);
            if (delta != 0) { ev[count].type = EV_REL; ev[count].code = REL_HWHEEL; ev[count].value = delta / 120; count++; }
            break;
        }
        default: break;           // WM_MOUSEMOVE (0x0200) and unknowns: movement only
    }

    if (count > 0) {
        ev[count].type = EV_SYN;
        ev[count].code = SYN_REPORT;
        ev[count].value = 0;
        count++;
        write(m_fd, ev, sizeof(struct input_event) * count);
    }
}

void InputManager::InjectKeyboard(const KeyboardData& data) {
    if (m_fd < 0) return;

    uint16_t key = 0;
    auto it = s_vkToKey.find(data.vkCode);
    if (it != s_vkToKey.end()) {
        key = it->second;
    } else {
        std::cout << "[VERBOSE] Unmapped Key: VK_0x" << std::hex << data.vkCode << std::dec << std::endl;
        return;
    }

    struct input_event ev[2];
    memset(ev, 0, sizeof(ev));
    ev[0].type = EV_KEY;
    ev[0].code = key;
    ev[0].value = (data.flags & 0x0002 /*KEYEVENTF_KEYUP*/) ? 0 : 1;

    ev[1].type = EV_SYN;
    ev[1].code = SYN_REPORT;
    ev[1].value = 0;

    write(m_fd, &ev, sizeof(ev));
}

}
