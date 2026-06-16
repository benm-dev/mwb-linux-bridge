#pragma once
#include <cstdint>
#include <vector>
#include <string>

namespace mwb {

// Emulating PowerToys Mouse Without Borders message format.
// Reconstructed from Microsoft PowerToys Common.cs and Network structs.
enum class PackageType : uint8_t {
    Hello = 3,
    Heartbeat = 20,
    Heartbeat_ex = 51,
    Clipboard = 69,
    ClipboardPush = 79,
    NextMachine = 121,
    Keyboard = 122,
    Mouse = 123,
    ClipboardText = 124,
    ClipboardImage = 125,
    Handshake = 126,
    HandshakeAck = 127,
    Matrix = 128
};

inline bool isBigPackage(uint8_t type) {
    if (type == 3 ||   // Hello
        type == 21 ||  // Awake
        type == 20 ||  // Heartbeat
        type == 51 ||  // Heartbeat_ex
        type == 126 || // Handshake
        type == 127 || // HandshakeAck
        type == 79 ||  // ClipboardPush
        type == 69 ||  // Clipboard
        type == 78 ||  // ClipboardAsk
        type == 125 || // ClipboardImage
        type == 124 || // ClipboardText
        type == 76)    // ClipboardDataEnd
    {
        return true;
    }
    return (type & 128) == 128; // Matrix
}

#pragma pack(push, 1)

// Unified 64-byte memory structure.
// Small packages only use and transmit the first 32 bytes.
struct MWBPacket {
    uint8_t type;         // Offset 0
    uint8_t checksum;     // Offset 1
    uint8_t magic0;       // Offset 2
    uint8_t magic1;       // Offset 3
    uint32_t id;          // Offset 4
    uint32_t src;         // Offset 8
    uint32_t des;         // Offset 12
    uint8_t data[48];     // Offset 16 onwards
};

// 16-byte mouse structure at packet offset 16 / data[0].
// Matches PowerToys Core/MOUSEDATA.cs:
//   data[0..3]   = X          (int32)
//   data[4..7]   = Y          (int32)
//   data[8..11]  = WheelDelta (int32, usually +/-120 for wheel events)
//   data[12..15] = dwFlags    (int32, WM_* message code:
//                          0x0200=MOUSEMOVE, 0x0201=LBUTTONDOWN, 0x0202=LBUTTONUP,
//                          0x0204=RBUTTONDOWN, 0x0205=RBUTTONUP,
//                          0x0207=MBUTTONDOWN, 0x0208=MBUTTONUP,
//                          0x020A=MOUSEWHEEL, 0x020E=MOUSEHWHEEL)
struct MouseData {
    int32_t x;            // data[0..3]
    int32_t y;            // data[4..7]
    int32_t wheelDelta;   // data[8..11]
    int32_t dwFlags;      // data[12..15]
};

// 8-byte keyboard structure at packet offset 24 / data[8].
// Matches PowerToys Core/KEYBDDATA.cs. dwFlags is the low-level keyboard-hook
// flags field; key release is LLKHF_UP (0x80), not KEYEVENTF_KEYUP.
struct KeyboardData {
    int32_t wVk;          // data[8..11]
    int32_t dwFlags;      // data[12..15]
};

#pragma pack(pop)

} // namespace mwb
