# Agent Workstreams

This directory coordinates parallel Claude, Codex, and optional Antigravity work on MWB Linux Bridge.

## Branches and Worktrees

| Agent | Branch | Worktree | Scope |
| --- | --- | --- | --- |
| Claude | `agents/claude-tui-packaging` | `../mwb-claude-tui-packaging` | TUI polish, native install docs, RPM packaging, validation notes |
| Codex | `agents/codex-rust-core` | `../mwb-codex-rust-core` | Rust native bridge implementation preserving the existing `mwb-client` CLI/config contract |
| Codex | `agents/codex-validation` | `../mwb-codex-validation` | Cross-branch validation, test planning, and integration risk checks |
| Antigravity | `agents/antigravity-verification` | `../mwb-antigravity-verification` | Independent validation, risk review, and integration checklist |

## Coordination Rules

- Agents must not edit each other's owned paths unless explicitly asked.
- Agents must not revert existing changes from another agent.
- Agents should keep commits small and include validation output in their final report.
- The current C++ bridge remains the compatibility reference until the Rust bridge is verified against Windows PowerToys.
- The Electron GUI is parked. Do not invest more work in GUI packaging unless a task explicitly asks for it.

## Owned Paths

Claude owns:

- `tui/`
- `packaging/`
- `README.md`
- `CMakeLists.txt`
- `scripts/`
- `agent-workstreams/`

Codex owns:

- `Cargo.toml`
- `Cargo.lock`
- `rust/`
- `src-rust/`
- Rust-related build wiring in `CMakeLists.txt` and `packaging/mwb-linux-bridge.spec`

Second Codex validation owns:

- `agent-workstreams/reports/`
- validation scripts under `scripts/` only if needed
- no production code unless explicitly assigned

Antigravity owns:

- `agent-workstreams/antigravity-verification.md`
- `agent-workstreams/reports/`
- validation notes only unless explicitly assigned an implementation task

Shared files require care:

- `README.md`
- `CMakeLists.txt`
- `packaging/mwb-linux-bridge.spec`

When touching shared files, agents must describe the exact section they changed.

## Integration Order

1. Merge Claude TUI/package polish first.
2. Use Antigravity's validation report as a gate before merging larger implementation branches.
3. Use the second Codex validation report to catch integration drift between TUI packaging and Rust work.
4. Merge Codex Rust core once `cargo test` and a local dry-run CLI validation pass.
5. Keep C++ source in the repo as `legacy` or `cpp` until real Windows PowerToys interoperability is confirmed.
