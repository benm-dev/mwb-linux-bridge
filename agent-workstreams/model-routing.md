# Model Routing Policy

Use the cheapest capable agent by default. Escalate only when the task risk justifies it.

## Codex Model Ladder

| Tier | Model | Use |
| --- | --- | --- |
| cheapest | `gpt-5.3-codex-spark` | routing, status checks, docs, shell scripts, validation reports, small isolated fixes |
| low | `gpt-5.4-mini` | routine code edits, tests, packaging polish, low-risk refactors |
| standard | `gpt-5.4` | multi-file implementation, Rust/C++ changes, moderate debugging |
| expensive | `gpt-5.5` | protocol compatibility, crypto, security review, architecture, high-risk integration |

## Routing Rules

- Start with the cheapest tier that can safely handle the work.
- Use `gpt-5.3-codex-spark` for deciding where a task should go unless the user directly requests a model.
- Use `gpt-5.5` only for compatibility-critical protocol, crypto, security, or difficult architecture work.
- Keep total agent windows at or below `MAX_AGENT_WINDOWS`, default `10`.
- Keep each routed task in its own worktree and branch.
- Do not route tasks into an existing dirty worktree unless the task explicitly belongs there.
- Prefer validation agents over implementation agents when the request is to watch, inspect, or report.

## Runtime Usage Signals

The usage monitor watches:

- active tmux agent windows
- Codex, Claude, and Antigravity process CPU and memory
- worktree dirty counts
- Codex process command lines, including selected model when present

These are local resource signals, not API billing totals. If provider token/cost telemetry becomes available, add it to `scripts/watch-agent-usage.sh` rather than guessing.
