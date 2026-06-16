# Codex Workstream: Validation and Integration Watch

## Goal

Run as the second Codex agent. Validate the parallel workstreams and produce integration guidance without duplicating implementation work.

## Worktree

`../mwb-codex-validation`

## Branch

`agents/codex-validation`

## Owned Paths

- `agent-workstreams/reports/`
- validation helper scripts under `scripts/` only if needed

## Tasks

1. Watch for integration risks between the TUI/package branch and Rust-core branch.
2. Validate repo commands that are safe locally.
3. Check docs for drift after the GUI was parked.
4. Produce a report under `agent-workstreams/reports/codex-validation.md`.

## Constraints

- Do not edit Rust core implementation files.
- Do not edit TUI production code unless explicitly asked.
- Do not remove the C++ bridge.
- Do not touch user config or secrets outside temporary directories.
- Do not claim PowerToys compatibility without real Windows interoperability testing.

## Suggested Validation

```bash
git status --short --branch
bash -n tui/mwb-tui scripts/deploy-agents.sh scripts/watch-agent-usage.sh
tmp=$(mktemp -d); HOME="$tmp" XDG_CONFIG_HOME="$tmp/config" ./tui/mwb-tui --status; rm -rf "$tmp"
g++ -O2 -std=c++17 -D_FORTIFY_SOURCE=3 -fstack-protector-strong -fPIE -pie -I src src/main.cpp src/CryptoHelper.cpp src/InputManager.cpp src/NetworkManager.cpp -lssl -lcrypto -lpthread -o /tmp/mwb-client-check
git diff --check
```

If a command is unavailable, record that fact in the report.
