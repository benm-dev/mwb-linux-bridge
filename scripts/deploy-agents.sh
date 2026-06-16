#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
parent_dir="$(dirname "${repo_root}")"
session_name="mwb-agents"

claude_branch="agents/claude-tui-packaging"
codex_branch="agents/codex-rust-core"
claude_worktree="${parent_dir}/mwb-claude-tui-packaging"
codex_worktree="${parent_dir}/mwb-codex-rust-core"

require_cmd() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "Missing required command: $1" >&2
        exit 1
    fi
}

require_cmd git
require_cmd tmux
require_cmd claude
require_cmd codex

cd "${repo_root}"

if ! git diff --quiet || ! git diff --cached --quiet; then
    echo "Working tree has uncommitted changes. Commit or stash before deploying worktree agents." >&2
    exit 1
fi

git worktree add -B "${claude_branch}" "${claude_worktree}" HEAD
git worktree add -B "${codex_branch}" "${codex_worktree}" HEAD

tmux has-session -t "${session_name}" 2>/dev/null && tmux kill-session -t "${session_name}"
tmux new-session -d -s "${session_name}" -n claude-tui -c "${claude_worktree}"
tmux send-keys -t "${session_name}:claude-tui" \
    "claude --name mwb-claude-tui-packaging --permission-mode acceptEdits \"\$(cat agent-workstreams/claude-tui-packaging.md)\"" C-m

tmux new-window -t "${session_name}" -n codex-rust -c "${codex_worktree}"
tmux send-keys -t "${session_name}:codex-rust" \
    "codex --cd '${codex_worktree}' --ask-for-approval never --sandbox workspace-write \"\$(cat agent-workstreams/codex-rust-core.md)\"" C-m

cat <<EOF
Deployed agent tmux session: ${session_name}

Attach:
  tmux attach -t ${session_name}

Windows:
  ${session_name}:claude-tui
  ${session_name}:codex-rust

Worktrees:
  ${claude_worktree}
  ${codex_worktree}
EOF
