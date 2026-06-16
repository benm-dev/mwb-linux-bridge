#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
parent_dir="$(dirname "${repo_root}")"
session_name="${1:-mwb-agents}"
interval="${WATCH_INTERVAL:-10}"

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

    printf 'MWB agent usage monitor  session=%s  time=%s\n\n' "${session_name}" "$(date '+%Y-%m-%d %H:%M:%S')"

    printf 'tmux windows:\n'
    tmux list-windows -t "${session_name}" -F '  #I:#W #{pane_current_command} #{pane_active}' 2>/dev/null || printf '  session not running\n'

    printf '\nagent processes:\n'
    ps -u "$USER" -o pid,pcpu,pmem,etime,stat,comm,args \
        | awk 'NR == 1 || /(^|[[:space:]])(claude|codex|agy)([[:space:]]|$)/ {print}' \
        | sed -n '1,40p'

    printf '\nworktree status:\n'
    for tree in "${worktrees[@]}"; do
        [[ -d "${tree}" ]] || continue
        branch="$(git -C "${tree}" branch --show-current 2>/dev/null || true)"
        dirty="$(git -C "${tree}" status --short 2>/dev/null | wc -l | tr -d ' ')"
        printf '  %-34s branch=%-32s dirty=%s\n' "$(basename "${tree}")" "${branch:-unknown}" "${dirty}"
    done

    printf '\nRefresh: %ss. Ctrl+C to stop monitor.\n' "${interval}"
    sleep "${interval}"
done
