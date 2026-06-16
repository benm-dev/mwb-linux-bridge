# Claude Workstream: TUI and Native Packaging

## Goal

Polish the terminal-first workflow so this project can be configured and launched without the parked Electron GUI.

## Worktree

`../mwb-claude-tui-packaging`

## Branch

`agents/claude-tui-packaging`

## Owned Paths

- `tui/`
- `packaging/`
- `README.md`
- `CMakeLists.txt`
- `scripts/`
- `agent-workstreams/`

## Tasks

1. Review `tui/mwb-tui` for shell robustness, terminal UX, and safe key handling.
2. Add or improve lightweight validation for `mwb-tui` without touching the user's real config.
3. Ensure native install paths include both `mwb-client` and `mwb-tui`.
4. Keep RPM packaging aligned with the TUI-first path.
5. Keep the Electron GUI marked as parked.

## Constraints

- Do not work on the Rust port.
- Do not remove the C++ bridge.
- Do not revive AppImage packaging.
- Do not require a desktop environment for setup.
- Do not store the security key in command history or process arguments.

## Validation

Run what is available:

```bash
bash -n tui/mwb-tui
tmp=$(mktemp -d); HOME="$tmp" XDG_CONFIG_HOME="$tmp/config" ./tui/mwb-tui --status; rm -rf "$tmp"
git diff --check
```

If `cmake` is available:

```bash
cmake -S . -B build
cmake --build build -j"$(nproc)"
DESTDIR="$(mktemp -d)" cmake --install build
```

Final report must list changed files and commands run.
