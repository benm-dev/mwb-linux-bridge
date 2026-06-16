#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
parent_dir="$(dirname "${repo_root}")"
session_name="${1:-mwb-agents}"
interval="${WATCH_INTERVAL:-10}"
max_agent_windows="${MAX_AGENT_WINDOWS:-10}"

worktrees=(
    "${repo_root}"
    "${parent_dir}/mwb-claude-tui-packaging"
    "${parent_dir}/mwb-codex-rust-core"
    "${parent_dir}/mwb-codex-validation"
    "${parent_dir}/mwb-antigravity-verification"
)

while true; do
    if [[ -t 1 ]]; then
        printf '\033[2J\033[H'
    fi

    agent_windows="0"
    if tmux has-session -t "${session_name}" 2>/dev/null; then
        agent_windows="$(tmux list-windows -t "${session_name}" -F '#W' | grep -Ev '^(usage|bootstrap)$' | wc -l | tr -d ' ')"
    fi

    printf 'MWB agent usage monitor  session=%s  time=%s\n' "${session_name}" "$(date '+%Y-%m-%d %H:%M:%S')"
    printf 'agent capacity: %s/%s  router: ./scripts/route-agent-task.sh TASK\n\n' "${agent_windows}" "${max_agent_windows}"

    printf 'tmux windows:\n'
    tmux list-windows -t "${session_name}" -F '  #I:#W #{pane_current_command} #{pane_active}' 2>/dev/null || printf '  session not running\n'

    printf '\nagent processes:\n'
    ps -ww -u "$USER" -o pid,pcpu,pmem,etime,stat,comm,args \
        | awk 'NR == 1 || /(^|[[:space:]])(claude|codex|agy)([[:space:]]|$)/ {print}' \
        | sed -n '1,40p'

    printf '\ncodex model usage:\n'
    ps -ww -u "$USER" -o comm,args \
        | awk '
            $1 == "codex" || $0 ~ /\/codex([[:space:]]|$)/ {
                model="default";
                for (i=1; i<=NF; i++) {
                    if ($i == "--model" || $i == "-m") {
                        model=$(i+1);
                    }
                }
                count[model]++;
            }
            END {
                if (length(count) == 0) {
                    print "  no codex processes found";
                } else {
                    for (model in count) {
                        printf "  %-24s %s\n", model, count[model];
                    }
                }
            }
        '

    printf '\nprovider token/cost usage:\n'
    if [[ -x "${repo_root}/scripts/extract-agent-usage.py" ]]; then
        "${repo_root}/scripts/extract-agent-usage.py" --root "${parent_dir}" --limit "${USAGE_LINES:-8}" --check-agy \
            | sed 's/^/  /'
    else
        printf '  scripts/extract-agent-usage.py is missing or not executable\n'
    fi

    printf '\nworktree status:\n'
    for tree in "${worktrees[@]}"; do
        [[ -d "${tree}" ]] || continue
        branch="$(git -C "${tree}" branch --show-current 2>/dev/null || true)"
        dirty="$(git -C "${tree}" status --short 2>/dev/null | wc -l | tr -d ' ')"
        printf '  %-34s branch=%-32s dirty=%s\n' "$(basename "${tree}")" "${branch:-unknown}" "${dirty}"
    done

    if [[ -f "${repo_root}/agent-workstreams/reports/usage-last-route.txt" ]]; then
        printf '\nlast routed task snapshot:\n'
        sed -n '1,5p' "${repo_root}/agent-workstreams/reports/usage-last-route.txt" | sed 's/^/  /'
    fi

    printf '\nRefresh: %ss. Ctrl+C to stop monitor.\n' "${interval}"
    sleep "${interval}"
done
