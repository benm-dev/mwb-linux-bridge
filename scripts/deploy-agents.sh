#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
parent_dir="$(dirname "${repo_root}")"
session_name="mwb-agents"
max_agent_windows="${MAX_AGENT_WINDOWS:-10}"

claude_branch="agents/claude-tui-packaging"
codex_branch="agents/codex-rust-core"
codex_validation_branch="agents/codex-validation"
antigravity_branch="agents/antigravity-verification"
claude_worktree="${parent_dir}/mwb-claude-tui-packaging"
codex_worktree="${parent_dir}/mwb-codex-rust-core"
codex_validation_worktree="${parent_dir}/mwb-codex-validation"
antigravity_worktree="${parent_dir}/mwb-antigravity-verification"

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

find_antigravity_cli() {
    if [[ -n "${ANTIGRAVITY_CLI:-}" ]]; then
        printf '%s' "${ANTIGRAVITY_CLI}"
        return 0
    fi

    local candidate
    for candidate in agy antigravity anti-gravity gravity; do
        if command -v "${candidate}" >/dev/null 2>&1; then
            command -v "${candidate}"
            return 0
        fi
    done

    return 1
}

ensure_worktree() {
    local branch="$1"
    local path="$2"

    if [[ -d "${path}/.git" || -f "${path}/.git" ]]; then
        echo "Reusing existing worktree: ${path}"
        return 0
    fi

    git worktree add -B "${branch}" "${path}" HEAD
}

ensure_tmux_session() {
    if tmux has-session -t "${session_name}" 2>/dev/null; then
        return 0
    fi
    tmux new-session -d -s "${session_name}" -n bootstrap -c "${repo_root}"
}

ensure_tmux_window() {
    local window="$1"
    local dir="$2"

    if tmux list-windows -t "${session_name}" -F '#W' | grep -qx "${window}"; then
        return 0
    fi

    if [[ "$(tmux list-windows -t "${session_name}" -F '#W' | wc -l)" -eq 1 ]] &&
       tmux list-windows -t "${session_name}" -F '#W' | grep -qx bootstrap; then
        tmux rename-window -t "${session_name}:bootstrap" "${window}"
        tmux send-keys -t "${session_name}:${window}" "cd '${dir}'" C-m
    else
        tmux new-window -t "${session_name}" -n "${window}" -c "${dir}"
    fi
}

ensure_agent_capacity() {
    local wanted="$1"
    local current=0
    if tmux has-session -t "${session_name}" 2>/dev/null; then
        current="$(tmux list-windows -t "${session_name}" -F '#W' | grep -Ev '^(usage|bootstrap)$' | wc -l | tr -d ' ')"
    fi
    if (( current + wanted > max_agent_windows )); then
        echo "Refusing to launch ${wanted} more agent window(s): current=${current}, max=${max_agent_windows}" >&2
        exit 1
    fi
}

tmux_window_exists() {
    local window="$1"
    tmux list-windows -t "${session_name}" -F '#W' | grep -qx "${window}"
}

cd "${repo_root}"

if ! git diff --quiet || ! git diff --cached --quiet; then
    echo "Working tree has uncommitted changes. Commit or stash before deploying worktree agents." >&2
    exit 1
fi

ensure_worktree "${claude_branch}" "${claude_worktree}"
ensure_worktree "${codex_branch}" "${codex_worktree}"
ensure_worktree "${codex_validation_branch}" "${codex_validation_worktree}"

antigravity_cli=""
if antigravity_cli="$(find_antigravity_cli)"; then
    ensure_worktree "${antigravity_branch}" "${antigravity_worktree}"
else
    echo "Antigravity CLI not found. Set ANTIGRAVITY_CLI=/path/to/cli to deploy that workstream."
fi

ensure_tmux_session
new_agent_windows=0
tmux_window_exists claude-tui || new_agent_windows=$((new_agent_windows + 1))
tmux_window_exists codex-rust || new_agent_windows=$((new_agent_windows + 1))
tmux_window_exists codex-validation || new_agent_windows=$((new_agent_windows + 1))
if [[ -n "${antigravity_cli}" ]] && ! tmux_window_exists antigravity-verify; then
    new_agent_windows=$((new_agent_windows + 1))
fi
ensure_agent_capacity "${new_agent_windows}"

if ! tmux_window_exists claude-tui; then
    ensure_tmux_window claude-tui "${claude_worktree}"
    tmux send-keys -t "${session_name}:claude-tui" \
        "claude --name mwb-claude-tui-packaging --permission-mode acceptEdits \"\$(cat agent-workstreams/claude-tui-packaging.md)\"" C-m
fi

if ! tmux_window_exists codex-rust; then
    ensure_tmux_window codex-rust "${codex_worktree}"
    tmux send-keys -t "${session_name}:codex-rust" \
        "codex --cd '${codex_worktree}' --ask-for-approval never --sandbox workspace-write \"\$(cat agent-workstreams/codex-rust-core.md)\"" C-m
fi

if ! tmux_window_exists codex-validation; then
    ensure_tmux_window codex-validation "${codex_validation_worktree}"
    tmux send-keys -t "${session_name}:codex-validation" \
        "codex --cd '${codex_validation_worktree}' --ask-for-approval never --sandbox workspace-write \"\$(cat agent-workstreams/codex-validation.md)\"" C-m
fi

if [[ -n "${antigravity_cli}" ]]; then
    if ! tmux_window_exists antigravity-verify; then
        ensure_tmux_window antigravity-verify "${antigravity_worktree}"
        tmux send-keys -t "${session_name}:antigravity-verify" \
            "'${antigravity_cli}' --dangerously-skip-permissions --prompt-interactive \"\$(cat agent-workstreams/antigravity-verification.md)\"" C-m
    fi
fi

if ! tmux_window_exists usage; then
    tmux new-window -t "${session_name}" -n usage -c "${repo_root}"
    tmux send-keys -t "${session_name}:usage" "./scripts/watch-agent-usage.sh '${session_name}'" C-m
fi

cat <<EOF
Deployed agent tmux session: ${session_name}

Attach:
  tmux attach -t ${session_name}

Windows:
  ${session_name}:claude-tui
  ${session_name}:codex-rust
  ${session_name}:codex-validation
$([[ -n "${antigravity_cli}" ]] && printf '  %s:antigravity-verify\n' "${session_name}")
  ${session_name}:usage

Route new work:
  ./scripts/route-agent-task.sh "TASK"

Worktrees:
  ${claude_worktree}
  ${codex_worktree}
  ${codex_validation_worktree}
$([[ -n "${antigravity_cli}" ]] && printf '  %s\n' "${antigravity_worktree}")
EOF
