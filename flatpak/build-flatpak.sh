#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
app_id="dev.benm.MwbLinuxBridge"
manifest="${repo_root}/flatpak/${app_id}.yml"
build_dir="${repo_root}/build/flatpak"
repo_dir="${repo_root}/dist/flatpak-repo"
bundle="${repo_root}/dist/${app_id}.flatpak"

if ! command -v flatpak-builder >/dev/null 2>&1; then
    cat >&2 <<'EOF'
ERROR: flatpak-builder is required.

Fedora:
  sudo dnf install flatpak-builder

Debian/Ubuntu:
  sudo apt install flatpak-builder flatpak
EOF
    exit 1
fi

if ! flatpak remotes | awk '{print $1}' | grep -qx flathub; then
    flatpak remote-add --user --if-not-exists flathub https://flathub.org/repo/flathub.flatpakrepo
fi

npm --prefix "${repo_root}/gui" ci
npm --prefix "${repo_root}/gui" run dist:dir

rm -rf "${build_dir}" "${repo_dir}" "${bundle}"
mkdir -p "${repo_root}/dist"

flatpak-builder \
    --force-clean \
    --user \
    --install-deps-from=flathub \
    --repo="${repo_dir}" \
    "${build_dir}" \
    "${manifest}"

flatpak build-bundle \
    "${repo_dir}" \
    "${bundle}" \
    "${app_id}" \
    --runtime-repo=https://flathub.org/repo/flathub.flatpakrepo

echo "Built ${bundle}"
echo "Install with: flatpak install --user ${bundle}"
echo "Run with: flatpak run ${app_id}"
