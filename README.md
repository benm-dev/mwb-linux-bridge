# MWB Linux Bridge

![Status: Early Development](https://img.shields.io/badge/status-early%20development-red)
![License: MIT](https://img.shields.io/badge/license-MIT-blue)

> **⚠️ Early Development — Expect Bugs**
>
> This project is in early, active development. Core functionality (connection, cursor movement, keyboard input) works in testing but there are known issues and many rough edges. Cursor crossing from Windows to Linux is not yet reliably working. Use at your own risk and expect things to break. Contributions and bug reports are very welcome.

A native C++17 Linux bridge for [Microsoft PowerToys "Mouse Without Borders"](https://learn.microsoft.com/en-us/windows/powertoys/mouse-without-borders), enabling cursor and keyboard sharing between Windows and Linux systems.

Works on both X11 and Wayland via Linux's `uinput` kernel interface.

## Features

- Full cursor movement and click injection (left, right, middle buttons, scroll wheel)
- Keyboard injection via Virtual Key Code translation to Linux `EV_KEY` codes
- Automatic reconnect on network drop
- Bidirectional TCP connection (connects out to Windows and accepts Windows's inbound connection)
- AES-256-CBC encrypted protocol matching PowerToys MWB exactly
- Dependency-light terminal UI for setup, status checks, key storage, and launching

## Project Structure

```
mwb-linux-bridge/
├── CMakeLists.txt
├── Dockerfile
├── flatpak/
│   ├── dev.benm.MwbLinuxBridge.yml
│   └── build-flatpak.sh
├── gui/
│   ├── package.json
│   └── src/
├── packaging/
│   ├── 90-mwb-client-uinput.rules
│   ├── config.example
│   ├── install-uinput-rule.sh
│   ├── README.Fedora.md
│   └── mwb-linux-bridge.spec
├── README.md
├── src/
│   ├── main.cpp
│   ├── Protocol.h
│   ├── CryptoHelper.h / .cpp
│   ├── InputManager.h / .cpp
│   └── NetworkManager.h / .cpp
└── tui/
    └── mwb-tui
```

## Build

### Prerequisites (Ubuntu / Debian)

```bash
sudo apt-get install -y build-essential cmake libssl-dev
```

### Compile

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

Install locally:

```bash
sudo cmake --install build
```

### uinput permissions (non-root)

```bash
sudo usermod -aG input $USER
echo 'KERNEL=="uinput", GROUP="input", MODE="0660"' | sudo tee /etc/udev/rules.d/99-uinput.rules
sudo udevadm control --reload-rules && sudo udevadm trigger
```

Log out and back in for group membership to take effect.

## Terminal UI (recommended)

`mwb-tui` is the recommended setup path while the desktop GUI is parked. It uses the same config file as `mwb-client` and works in any normal terminal or over SSH.

From a source checkout:

```bash
./tui/mwb-tui
./tui/mwb-tui --status
```

After installing:

```bash
mwb-tui
mwb-tui --status
```

The TUI can:

- configure the Windows IP/host and port
- save the PowerToys security key to `~/.config/mwb-linux-bridge/key` with private permissions, or leave the client in prompt mode
- check `/dev/uinput`, the selected client binary, session type, route to Windows, and TCP port reachability
- install the uinput udev rule on request
- start `mwb-client` with the saved config

## Experimental Flatpak GUI (parked)

Flatpak support is kept for the Electron GUI, but the current recommended path is the native client plus `mwb-tui`. The Flatpak bundles the Electron frontend and the native `mwb-client` bridge binary into one installable bundle.

Install build tools:

```bash
# Fedora
sudo dnf install flatpak flatpak-builder

# Debian / Ubuntu
sudo apt install flatpak flatpak-builder
```

Build and install the local Flatpak bundle:

```bash
./packaging/install-uinput-rule.sh
npm --prefix gui run dist:linux
flatpak install --user ./dist/dev.benm.MwbLinuxBridge.flatpak
flatpak run dev.benm.MwbLinuxBridge
```

The bundle is written to `dist/dev.benm.MwbLinuxBridge.flatpak`.

Flatpak can request device access, but it cannot install host udev rules. Run `./packaging/install-uinput-rule.sh` once on the host so the app can open `/dev/uinput`.

## Usage

```
./build/mwb_client [--config PATH] [--host WINDOWS_IP] [--port PORT]
./build/mwb_client <WINDOWS_IP> [PORT]
```

- `WINDOWS_IP` — IP address of the Windows machine running PowerToys MWB
- `PORT` — Optional, defaults to `15101` (keyboard/mouse channel). Do **not** use `15100` (clipboard only).
- Security key — enter the key shown in **PowerToys → Mouse Without Borders → Security key** at the hidden prompt, or set `MWB_SECURITY_KEY`.

Example:

```bash
./build/mwb_client 192.168.1.10
```

Do not pass the security key as a command-line argument. Command-line secrets can be exposed through shell history and process listings.

### Config file

Create the default config:

```bash
./build/mwb_client --init-config
```

Then edit `~/.config/mwb-linux-bridge/config`:

```ini
windows_ip=192.168.1.10
port=15101
# security_key_file=~/.config/mwb-linux-bridge/key
```

After that, run:

```bash
./build/mwb_client
```

Supported config keys are `windows_ip`, `host`, `port`, and `security_key_file`. If `security_key_file` is omitted, the client prompts for the key.

## Electron GUI (parked)

The Electron GUI is parked while the terminal UI is the recommended workflow. The code remains under `gui/` for later work.

The GUI is a Linux desktop frontend for the same `mwb-client` binary and config file. It uses standard Electron windows, no tray-only workflow, no global shortcuts, and no compositor-specific APIs, so it can run under common desktop environments and window managers on X11 or Wayland.

Run in development:

```bash
cd gui
npm install
npm start
```

Build the universal Flatpak bundle:

```bash
cd gui
npm run dist:linux
```

The Flatpak bundle is copied to the repo-level `dist/` directory and includes both the GUI and native bridge.

Build the Fedora-native GUI RPM instead:

```bash
cd gui
npm run dist:rpm
```

The GUI RPM is copied to the repo-level `dist/` directory. It installs the GUI as `mwb-linux-bridge-gui`, adds a desktop entry, and depends on the native `mwb-linux-bridge` package for the actual input bridge.

The GUI checks the session type, desktop name, `/dev/uinput` access, and the resolved `mwb-client` binary path. It can save connection settings to `~/.config/mwb-linux-bridge/config` and, if selected, store the key in `~/.config/mwb-linux-bridge/key` with private file permissions.

### Setup in PowerToys

1. Open **PowerToys → Mouse Without Borders** on Windows.
2. Enable the feature and note your **Security key**.
3. Add your Linux machine name (e.g. `mylinuxbox`) to the machine list.
4. Configure which screen edge leads to the Linux machine.
5. Run the client on Linux as shown above.

## Docker

```bash
docker build -t mwb-linux-bridge .
docker run --rm -it --device /dev/uinput:/dev/uinput mwb-linux-bridge \
    <WINDOWS_IP>
```

## Fedora RPM

This repository includes a local RPM spec under `packaging/`. The package installs `/usr/bin/mwb-client`, `/usr/bin/mwb-tui`, and a udev rule for active-session `/dev/uinput` access.

Build locally:

```bash
rpmbuild --define "_topdir /tmp/mwb-rpmbuild" -ba /tmp/mwb-rpmbuild/SPECS/mwb-linux-bridge.spec
```

After installing the RPM, run:

```bash
mwb-tui
```

Or run the client directly:

```bash
mwb-client 192.168.1.10
```

To build the Fedora-native GUI RPM, run:

```bash
cd gui
npm run dist:rpm
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

- Screen dimensions are queried with `xrandr` at startup. If that fails, input injection falls back to 1920x1080.
- Clipboard sync is not implemented.
- Wayland cursor warping uses a REL snap-to-corner trick; absolute positioning via `EV_ABS` is ignored by most compositors for `INPUT_PROP_POINTER` devices.
- Security is constrained by the PowerToys compatibility protocol: AES-256-CBC has no per-message authentication tag and uses a fixed compatibility IV. Run this only on trusted LANs/VPNs.

## License

MIT — see [LICENSE](LICENSE).

This project is independent and is not affiliated with or endorsed by Microsoft. It interoperates with the PowerToys Mouse Without Borders protocol by studying the published open-source implementation at [microsoft/PowerToys](https://github.com/microsoft/PowerToys).
