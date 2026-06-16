#include "InputManager.h"
#include <iostream>
#include <algorithm>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <linux/uinput.h>
#include <unordered_map>
#include <cstdlib>
#include <cerrno>

namespace mwb {

namespace {
constexpr int kMoveMouseRelative = 100000;
constexpr int kWmMouseMove = 0x0200;
constexpr int kWmLButtonDown = 0x0201;
constexpr int kWmLButtonUp = 0x0202;
constexpr int kWmLButtonDblClk = 0x0203;
constexpr int kWmRButtonDown = 0x0204;
constexpr int kWmRButtonUp = 0x0205;
constexpr int kWmRButtonDblClk = 0x0206;
constexpr int kWmMButtonDown = 0x0207;
constexpr int kWmMButtonUp = 0x0208;
constexpr int kWmMButtonDblClk = 0x0209;
constexpr int kWmMouseWheel = 0x020A;
constexpr int kWmMouseHWheel = 0x020E;
constexpr int kLlkHfUp = 0x80;

bool WriteAll(int fd, const void* data, size_t size) {
    const auto* bytes = static_cast<const uint8_t*>(data);
    size_t sent = 0;
    while (sent < size) {
        ssize_t n = write(fd, bytes + sent, size - sent);
        if (n < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        if (n == 0) return false;
        sent += static_cast<size_t>(n);
    }
    return true;
}
}

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
    ioctl(m_fd, UI_SET_RELBIT, REL_HWHEEL);
#ifdef REL_WHEEL_HI_RES
    ioctl(m_fd, UI_SET_RELBIT, REL_WHEEL_HI_RES);
#endif
#ifdef REL_HWHEEL_HI_RES
    ioctl(m_fd, UI_SET_RELBIT, REL_HWHEEL_HI_RES);
#endif

    ioctl(m_fd, UI_SET_ABSBIT, ABS_X);
    ioctl(m_fd, UI_SET_ABSBIT, ABS_Y);

    struct uinput_user_dev uidev;
    memset(&uidev, 0, sizeof(uidev));
    strncpy(uidev.name, "MWB Linux Bridge Virtual Client", sizeof(uidev.name) - 1);
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

    if (!WriteAll(m_fd, &uidev, sizeof(uidev))) {
        std::cerr << "ERR: Failed to write uinput dev setup." << std::endl;
        return false;
    }

    if (ioctl(m_fd, UI_DEV_CREATE) < 0) {
        std::cerr << "ERR: Failed to create uinput device." << std::endl;
        return false;
    }

    return true;
}

void InputManager::SetScreenSize(int width, int height) {
    if (width > 0 && height > 0) {
        m_screenW = width;
        m_screenH = height;
    }
}

void InputManager::WarpCursor(int px, int py) {
    // Warp cursor to pixel position (px, py) using two-step REL injection.
    // ABS events are ignored by libinput for INPUT_PROP_POINTER devices, so we
    // use REL: first snap to (0,0) with a very large negative delta, then
    // apply the target offset. The compositor clamps REL movement at screen bounds.
    px = std::max(0, std::min(m_screenW - 1, px));
    py = std::max(0, std::min(m_screenH - 1, py));

    const int LARGE = 100000;
    struct input_event ev[4];

    // Step 1: snap to top-left corner
    memset(ev, 0, sizeof(ev));
    ev[0].type = EV_REL; ev[0].code = REL_X; ev[0].value = -LARGE;
    ev[1].type = EV_REL; ev[1].code = REL_Y; ev[1].value = -LARGE;
    ev[2].type = EV_SYN; ev[2].code = SYN_REPORT;
    if (!WriteAll(m_fd, ev, sizeof(struct input_event) * 3)) {
        std::cerr << "ERR: Failed to inject cursor snap event." << std::endl;
        return;
    }

    // Step 2: move to target (px, py) from (0, 0)
    memset(ev, 0, sizeof(ev));
    ev[0].type = EV_REL; ev[0].code = REL_X; ev[0].value = px;
    ev[1].type = EV_REL; ev[1].code = REL_Y; ev[1].value = py;
    ev[2].type = EV_SYN; ev[2].code = SYN_REPORT;
    if (!WriteAll(m_fd, ev, sizeof(struct input_event) * 3)) {
        std::cerr << "ERR: Failed to inject cursor warp event." << std::endl;
        return;
    }

    printf("[WARP] Cursor snapped to pixel (%d, %d)\n", px, py);
    fflush(stdout);
}

void InputManager::InjectMouse(const MouseData& data) {
    if (m_fd < 0) return;

    struct input_event ev[16];
    memset(ev, 0, sizeof(ev));
    int count = 0;
    auto addWheel = [&](int relCode, int hiResCode, int delta, int& remainder) {
        if (delta == 0) return;
#if defined(REL_WHEEL_HI_RES) || defined(REL_HWHEEL_HI_RES)
        if (hiResCode >= 0) {
            ev[count].type = EV_REL;
            ev[count].code = static_cast<uint16_t>(hiResCode);
            ev[count].value = delta;
            count++;
        }
#endif
        remainder += delta;
        int detents = remainder / 120;
        remainder %= 120;
        if (detents != 0) {
            ev[count].type = EV_REL;
            ev[count].code = static_cast<uint16_t>(relCode);
            ev[count].value = detents;
            count++;
        }
    };

    if (std::abs(data.x) >= kMoveMouseRelative && std::abs(data.y) >= kMoveMouseRelative) {
        if (data.dwFlags == kWmMouseMove) {
            int dx = data.x < 0 ? data.x + kMoveMouseRelative : data.x - kMoveMouseRelative;
            int dy = data.y < 0 ? data.y + kMoveMouseRelative : data.y - kMoveMouseRelative;
            if (dx != 0) { ev[count].type = EV_REL; ev[count].code = REL_X; ev[count].value = dx; count++; }
            if (dy != 0) { ev[count].type = EV_REL; ev[count].code = REL_Y; ev[count].value = dy; count++; }
        }
    } else {
        int px = static_cast<int>((static_cast<int64_t>(data.x) * (m_screenW - 1)) / 65535);
        int py = static_cast<int>((static_cast<int64_t>(data.y) * (m_screenH - 1)) / 65535);
        WarpCursor(px, py);
    }

    // Button and wheel events via WM_* message code in dwFlags.
    switch (data.dwFlags) {
        case kWmLButtonDown: case kWmLButtonDblClk:
            ev[count].type = EV_KEY; ev[count].code = BTN_LEFT;   ev[count].value = 1; count++; break;
        case kWmLButtonUp:
            ev[count].type = EV_KEY; ev[count].code = BTN_LEFT;   ev[count].value = 0; count++; break;
        case kWmRButtonDown: case kWmRButtonDblClk:
            ev[count].type = EV_KEY; ev[count].code = BTN_RIGHT;  ev[count].value = 1; count++; break;
        case kWmRButtonUp:
            ev[count].type = EV_KEY; ev[count].code = BTN_RIGHT;  ev[count].value = 0; count++; break;
        case kWmMButtonDown: case kWmMButtonDblClk:
            ev[count].type = EV_KEY; ev[count].code = BTN_MIDDLE; ev[count].value = 1; count++; break;
        case kWmMButtonUp:
            ev[count].type = EV_KEY; ev[count].code = BTN_MIDDLE; ev[count].value = 0; count++; break;
        case kWmMouseWheel: {
#ifdef REL_WHEEL_HI_RES
            addWheel(REL_WHEEL, REL_WHEEL_HI_RES, data.wheelDelta, m_wheelRemainder);
#else
            addWheel(REL_WHEEL, -1, data.wheelDelta, m_wheelRemainder);
#endif
            break;
        }
        case kWmMouseHWheel: {
#ifdef REL_HWHEEL_HI_RES
            addWheel(REL_HWHEEL, REL_HWHEEL_HI_RES, data.wheelDelta, m_hwheelRemainder);
#else
            addWheel(REL_HWHEEL, -1, data.wheelDelta, m_hwheelRemainder);
#endif
            break;
        }
        default: break;
    }

    if (count > 0) {
        ev[count].type = EV_SYN;
        ev[count].code = SYN_REPORT;
        ev[count].value = 0;
        count++;
        if (!WriteAll(m_fd, ev, sizeof(struct input_event) * static_cast<size_t>(count))) {
            std::cerr << "ERR: Failed to inject mouse event." << std::endl;
        }
    }
}

void InputManager::InjectKeyboard(const KeyboardData& data) {
    if (m_fd < 0) return;

    uint16_t key = 0;
    auto it = s_vkToKey.find(static_cast<uint16_t>(data.wVk));
    if (it != s_vkToKey.end()) {
        key = it->second;
    } else {
        std::cout << "[VERBOSE] Unmapped Key: VK_0x" << std::hex << data.wVk << std::dec << std::endl;
        return;
    }

    struct input_event ev[2];
    memset(ev, 0, sizeof(ev));
    ev[0].type = EV_KEY;
    ev[0].code = key;
    ev[0].value = (data.dwFlags & kLlkHfUp) ? 0 : 1;

    ev[1].type = EV_SYN;
    ev[1].code = SYN_REPORT;
    ev[1].value = 0;

    if (!WriteAll(m_fd, &ev, sizeof(ev))) {
        std::cerr << "ERR: Failed to inject keyboard event." << std::endl;
    }
}

}
