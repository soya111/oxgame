#!/bin/bash
# One line for the routine check (it also runs start.sh): time, uptime, driver / autosync, the minutes left of
# each keep-alive loop, git state, the last autosync line.
SP=${SP:-/tmp/claude-0/-home-user-oxgame/4a497322-6915-5b7d-b930-62ce754faa84/scratchpad}
REPO=$(cd "$(dirname "$0")/../.." && pwd)
s=$(bash "$REPO/tools/ops/start.sh" 2>&1 | tr '\n' ';')
cd "$REPO/.." || exit 1
k=$(ps -eo etimes=,args= | awk '$2 == "bash" && $3 ~ /ops\/keep\.sh$/ {printf "%s%dm", (n++ ? "," : ""), ($4 - $1) / 60}')
echo "$(date -u +%FT%H:%MZ) up=$(awk '{printf "%dh%02dm", $1 / 3600, $1 % 3600 / 60}' /proc/uptime)" \
  "driver=$(pgrep -fc "campaign.py --root $SP/ccamp") autosync=$(pgrep -fc "ops/autosync.sh") keep=[$k]" \
  "dirty=$(git status --porcelain | wc -l) unpushed=$(git rev-list --count '@{u}..HEAD' 2>/dev/null)" \
  "sync=[$(grep -E 'committed|autosync cycle' "$SP/week/autosync.log" | tail -1)]${s:+ started=[$s]}"
