#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
parent_dir="$(dirname "${repo_root}")"
session_name="${SESSION_NAME:-mwb-agents}"
max_agent_windows="${MAX_AGENT_WINDOWS:-10}"
default_router_model="${ROUTER_MODEL:-gpt-5.3-codex-spark}"

usage() {
    cat <<EOF
Usage: route-agent-task.sh [--model MODEL] [--name NAME] TASK...

Routes TASK to the cheapest suitable Codex model and launches it in a new
worktree/tmux window. Defaults to a maximum of ${max_agent_windows} agent
windows, excluding the usage monitor.

Examples:
  ./scripts/route-agent-task.sh "review Rust crypto parity"
  ./scripts/route-agent-task.sh --model gpt-5.5 "deep protocol review"
EOF
}

slugify() {
    printf '%s' "$1" \
        | tr '[:upper:]' '[:lower:]' \
        | sed -E 's/[^a-z0-9]+/-/g; s/^-+//; s/-+$//' \
        | cut -c1-48
}

classify_model() {
    local task_lc="$1"

    if [[ "${task_lc}" =~ (crypto|cryptography|aes|pbkdf|security|vulnerability|exploit|auth|protocol|powertoys|handshake|packet|wire|interop|architecture|design) ]]; then
        printf 'gpt-5.5'
    elif [[ "${task_lc}" =~ (rust|c\+\+|cpp|network|uinput|input|refactor|multi-file|debug|bug|implement|feature) ]]; then
        printf 'gpt-5.4'
    elif [[ "${task_lc}" =~ (test|package|rpm|flatpak|tui|script|shell|docs|readme|validate|lint|cleanup) ]]; then
        printf 'gpt-5.4-mini'
    else
        printf '%s' "${default_router_model}"
    fi
}

agent_count() {
    if ! tmux has-session -t "${session_name}" 2>/dev/null; then
        printf '0'
        return 0
    fi
    tmux list-windows -t "${session_name}" -F '#W' \
        | grep -Ev '^(usage|bootstrap)$' \
        | wc -l \
        | tr -d ' '
}

ensure_capacity() {
    local current
    current="$(agent_count)"
    if (( current >= max_agent_windows )); then
        echo "Refusing to launch: current agent windows=${current}, max=${max_agent_windows}" >&2
        echo "Attach with: tmux attach -t ${session_name}" >&2
        exit 1
    fi
}

ensure_tmux_session() {
    if ! tmux has-session -t "${session_name}" 2>/dev/null; then
        tmux new-session -d -s "${session_name}" -n usage -c "${repo_root}"
        tmux send-keys -t "${session_name}:usage" "./scripts/watch-agent-usage.sh '${session_name}'" C-m
    fi
}

snapshot_usage() {
    local report_dir="${repo_root}/agent-workstreams/reports"
    mkdir -p "${report_dir}"
    {
        printf 'time=%s\n' "$(date '+%Y-%m-%d %H:%M:%S')"
        printf 'session=%s\n' "${session_name}"
        printf 'agent_windows=%s/%s\n' "$(agent_count)" "${max_agent_windows}"
        ps -ww -u "$USER" -o pid,pcpu,pmem,etime,comm,args \
            | awk 'NR == 1 || /(^|[[:space:]])(claude|codex|agy)([[:space:]]|$)/ {print}'
    } > "${report_dir}/usage-last-route.txt"
}

model=""
name=""
task_parts=()

while (($#)); do
    case "$1" in
        --model)
            shift
            [[ $# -gt 0 ]] || { echo "--model needs a value" >&2; exit 2; }
            model="$1"
            ;;
        --name)
            shift
            [[ $# -gt 0 ]] || { echo "--name needs a value" >&2; exit 2; }
            name="$1"
            ;;
        --help|-h)
            usage
            exit 0
            ;;
        *)
            task_parts+=("$1")
            ;;
    esac
    shift
done

if [[ "${#task_parts[@]}" -eq 0 ]]; then
    usage >&2
    exit 2
fi

task="${task_parts[*]}"
task_lc="$(printf '%s' "${task}" | tr '[:upper:]' '[:lower:]')"
if [[ -z "${model}" ]]; then
    model="$(classify_model "${task_lc}")"
fi

if [[ -z "${name}" ]]; then
    name="$(slugify "${task}")"
fi
if [[ -z "${name}" ]]; then
    name="task-$(date +%s)"
fi

branch="agents/routed-${name}"
worktree="${parent_dir}/mwb-routed-${name}"
window="codex-${name}"
if [[ "${#window}" -gt 30 ]]; then
    window="${window:0:30}"
fi

cd "${repo_root}"
if ! git diff --quiet || ! git diff --cached --quiet; then
    echo "Main worktree is dirty; commit or stash before routing a new agent." >&2
    exit 1
fi

ensure_tmux_session
ensure_capacity
snapshot_usage

if [[ -d "${worktree}/.git" || -f "${worktree}/.git" ]]; then
    echo "Reusing worktree: ${worktree}"
else
    git worktree add -B "${branch}" "${worktree}" HEAD
fi

if tmux list-windows -t "${session_name}" -F '#W' | grep -qx "${window}"; then
    echo "Window already exists: ${session_name}:${window}" >&2
    exit 1
fi

prompt_file="${worktree}/agent-workstreams/routed-task.md"
cat > "${prompt_file}" <<EOF
# Routed Codex Task

## Task

${task}

## Routing

- selected model: ${model}
- selected by: scripts/route-agent-task.sh
- policy: agent-workstreams/model-routing.md

## Constraints

- You are not alone in the codebase; do not revert other agents' work.
- Keep edits scoped to this task.
- Do not touch user secrets or real user config.
- Run relevant validation and report commands executed.
- Commit only if explicitly instructed by the main coordinator.
EOF

tmux new-window -t "${session_name}" -n "${window}" -c "${worktree}"
tmux send-keys -t "${session_name}:${window}" \
    "codex --cd '${worktree}' --model '${model}' --ask-for-approval never --sandbox workspace-write \"\$(cat agent-workstreams/routed-task.md)\"" C-m

cat <<EOF
Routed task to Codex.

model:    ${model}
branch:   ${branch}
worktree: ${worktree}
window:   ${session_name}:${window}
usage:    ${repo_root}/agent-workstreams/reports/usage-last-route.txt
EOF
