# mwb-client-linux

![Status: Early Development](https://img.shields.io/badge/status-early%20development-red)
![License: MIT](https://img.shields.io/badge/license-MIT-blue)

> **⚠️ Early Development — Expect Bugs**
>
> This project is in early, active development. Core functionality (connection, cursor movement, keyboard input) works in testing but there are known issues and many rough edges. Cursor crossing from Windows to Linux is not yet reliably working. Use at your own risk and expect things to break. Contributions and bug reports are very welcome.

A native C++17 client that connects a Linux machine to a Windows machine running [Microsoft PowerToys "Mouse Without Borders"](https://learn.microsoft.com/en-us/windows/powertoys/mouse-without-borders), enabling seamless cursor and keyboard sharing between the two systems.

Works on both X11 and Wayland via Linux's `uinput` kernel interface.

## Features

- Full cursor movement and click injection (left, right, middle buttons, scroll wheel)
- Keyboard injection via Virtual Key Code translation to Linux `EV_KEY` codes
- Automatic reconnect on network drop
- Bidirectional TCP connection (connects out to Windows and accepts Windows's inbound connection)
- AES-256-CBC encrypted protocol matching PowerToys MWB exactly

## Project Structure

```
mwb-client-linux/
├── CMakeLists.txt
├── Dockerfile
├── README.md
└── src/
    ├── main.cpp
    ├── Protocol.h
    ├── CryptoHelper.h / .cpp
    ├── InputManager.h / .cpp
    ├── NetworkManager.h / .cpp
    └── (no generated files — clean build tree)
```

## Build

### Prerequisites (Ubuntu / Debian)

```bash
sudo apt-get install -y build-essential cmake pkg-config libssl-dev libevdev-dev
```

### Compile

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### uinput permissions (non-root)

```bash
sudo usermod -aG input $USER
echo 'KERNEL=="uinput", GROUP="input", MODE="0660"' | sudo tee /etc/udev/rules.d/99-uinput.rules
sudo udevadm control --reload-rules && sudo udevadm trigger
```

Log out and back in for group membership to take effect.

## Usage

```
./build/mwb_client <WINDOWS_IP> <SECURITY_KEY> [PORT]
```

- `WINDOWS_IP` — IP address of the Windows machine running PowerToys MWB
- `SECURITY_KEY` — The security key shown in **PowerToys → Mouse Without Borders → Security key**
- `PORT` — Optional, defaults to `15101` (keyboard/mouse channel). Do **not** use `15100` (clipboard only).

Example:

```bash
./build/mwb_client 192.168.1.10 MySecurityKey123
```

### Setup in PowerToys

1. Open **PowerToys → Mouse Without Borders** on Windows.
2. Enable the feature and note your **Security key**.
3. Add your Linux machine name (e.g. `mylinuxbox`) to the machine list.
4. Configure which screen edge leads to the Linux machine.
5. Run the client on Linux as shown above.

## Docker

```bash
docker build -t mwb-linux .
docker run --rm -it --device /dev/uinput:/dev/uinput mwb-linux \
    <WINDOWS_IP> <SECURITY_KEY>
```

## Protocol Notes

The PowerToys MWB protocol uses:

- **Transport:** TCP on port 15101 (mouse/keyboard) and 15100 (clipboard)
- **Encryption:** AES-256-CBC, no padding, streaming mode
- **Key derivation:** PBKDF2-HMAC-SHA512, 50 000 iterations, fixed salt derived from `ulong.MaxValue` encoded as UTF-16LE
- **IV:** Fixed string `"1844674407370955"` (ASCII, 16 bytes)
- **Magic number:** 24-bit hash of the security key via 50 000 rounds of SHA-512
- **Packet sizes:** 32 bytes (small: mouse, keyboard, small heartbeat) or 64 bytes (big: handshake, identity, matrix)
- **Handshake:** Both sides exchange 10× type-126 challenge packets; each responds with a type-127 acknowledgement carrying the bitwise-NOT of the received challenge fields
- **Identity:** Type-51 heartbeat packets carry the machine name (ASCII, space-padded to 32 bytes) and screen dimensions

The machine name sent by this client must match the name configured in Windows's MWB machine list.

## Known Limitations

- Screen dimensions for coordinate scaling are currently hardcoded to 1920×1080 in `InputManager.cpp`. The actual screen size is queried via `xrandr` at startup and used for the identity broadcast, but the injection path will need updating for non-1080p screens.
- Clipboard sync is not implemented.
- Wayland cursor warping uses a REL snap-to-corner trick; absolute positioning via `EV_ABS` is ignored by most compositors for `INPUT_PROP_POINTER` devices.

## License

MIT — see [LICENSE](LICENSE).

This project is independent and is not affiliated with or endorsed by Microsoft. It interoperates with the PowerToys Mouse Without Borders protocol by studying the published open-source implementation at [microsoft/PowerToys](https://github.com/microsoft/PowerToys).
