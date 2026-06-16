# MWB Linux Bridge Fedora RPM Notes

The RPM installs:

- `/usr/bin/mwb-client`
- `/usr/lib/udev/rules.d/90-mwb-client-uinput.rules`

The udev rule grants the active local desktop session access to `/dev/uinput` through `TAG+="uaccess"`. After installing, reload udev rules or reboot:

```bash
sudo udevadm control --reload-rules
sudo udevadm trigger /dev/uinput
```

Run the client with the Windows machine IP and enter the PowerToys Mouse Without Borders key at the hidden prompt:

```bash
mwb-client 192.168.1.10
```

For repeat use, create the default config and edit it:

```bash
mwb-client --init-config
$EDITOR ~/.config/mwb-linux-bridge/config
mwb-client
```

The key can also be supplied through `MWB_SECURITY_KEY`, but the hidden prompt is preferred because command history and process listings are less likely to expose it.

This client interoperates with the existing PowerToys protocol. That protocol uses AES-256-CBC without a per-message authentication tag and a compatibility fixed IV, so this package should be treated as LAN-trusted software rather than a hardened internet-facing service.

## GUI

The Electron GUI lives in `gui/` and builds a Fedora RPM:

```bash
cd gui
npm install
npm run dist:rpm
```

The GUI RPM is copied to the repo-level `dist/` directory. It installs `/usr/bin/mwb-linux-bridge-gui` and still launches the `mwb-client` binary installed by this RPM or selected manually in the GUI.

For a more universal Linux GUI package, build the Flatpak bundle instead:

```bash
./packaging/install-uinput-rule.sh
npm --prefix gui run dist:linux
flatpak install --user ./dist/dev.benm.MwbLinuxBridge.flatpak
```
