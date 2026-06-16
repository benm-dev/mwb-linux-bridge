# Flatpak Packaging

Flatpak is the preferred universal Linux packaging target for MWB Linux Bridge.
The Flatpak bundles both the Electron GUI and the native `mwb-client` binary.

The host still needs `/dev/uinput` access. Flatpak can request device access,
but it cannot install host udev rules by itself. Run the uinput helper once on
the host before using the Flatpak.

## Build

Install Flatpak Builder:

```bash
# Fedora
sudo dnf install flatpak-builder

# Debian / Ubuntu
sudo apt install flatpak flatpak-builder
```

Build a local bundle:

```bash
# Run from the repository root.
./flatpak/build-flatpak.sh
```

The bundle is written to:

```text
dist/dev.benm.MwbLinuxBridge.flatpak
```

## Install

```bash
./packaging/install-uinput-rule.sh
flatpak install --user ./dist/dev.benm.MwbLinuxBridge.flatpak
flatpak run dev.benm.MwbLinuxBridge
```

## Permissions

The Flatpak requests:

- network access for the PowerToys MWB TCP connection
- Wayland and fallback X11 sockets for broad desktop/window-manager support
- device access so the bundled native client can open `/dev/uinput`

This is broader than a normal GUI-only Flatpak because the app injects local
keyboard and mouse events.
