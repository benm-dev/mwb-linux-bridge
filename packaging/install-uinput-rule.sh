#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
rule_source="${repo_root}/packaging/90-mwb-client-uinput.rules"
rule_target="/etc/udev/rules.d/90-mwb-client-uinput.rules"

if [[ ! -f "${rule_source}" ]]; then
    echo "Missing ${rule_source}" >&2
    exit 1
fi

sudo install -Dpm 0644 "${rule_source}" "${rule_target}"
sudo udevadm control --reload-rules
sudo udevadm trigger /dev/uinput || sudo udevadm trigger

echo "Installed ${rule_target}"
echo "Log out and back in if /dev/uinput is still not writable."
