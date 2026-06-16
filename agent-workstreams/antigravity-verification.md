# Antigravity Workstream: Independent Verification

## Goal

Act as the independent verifier for the parallel Claude and Codex workstreams.

## Worktree

`../mwb-antigravity-verification`

## Branch

`agents/antigravity-verification`

## Owned Paths

- `agent-workstreams/antigravity-verification.md`
- `agent-workstreams/reports/`

## Tasks

1. Review the TUI/package branch for install correctness and terminal UX risks.
2. Review the Rust native bridge branch for protocol parity risks against the current C++ implementation.
3. Produce a concise integration checklist under `agent-workstreams/reports/`.
4. Do not make broad code changes unless explicitly assigned a specific implementation task.

## Constraints

- Do not revert Claude or Codex changes.
- Do not remove the C++ bridge.
- Do not claim Windows PowerToys compatibility without a real interoperability test.
- Do not touch secrets, saved keys, or user config outside temporary test directories.

## Suggested Validation

```bash
git status --short --branch
bash -n tui/mwb-tui
tmp=$(mktemp -d); HOME="$tmp" XDG_CONFIG_HOME="$tmp/config" ./tui/mwb-tui --status; rm -rf "$tmp"
cargo test
git diff --check
```

If a command is unavailable, record that fact in the report instead of pretending it passed.
