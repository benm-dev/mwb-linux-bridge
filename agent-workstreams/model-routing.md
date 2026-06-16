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
- Codex local JSONL token totals from `~/.codex/sessions`
- Claude local JSONL token totals from `~/.claude/projects`
- rough USD estimates where local token totals and configured public API prices are available

These are local telemetry signals, not provider invoices. Antigravity quota appears to be cloud-account data surfaced by the logged-in app; no stable local token/credit record or `agy` command is available here yet.
