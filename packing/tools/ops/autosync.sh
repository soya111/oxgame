#!/bin/bash
# Every 3 h: refresh the Packomania tables (daily), copy improvements into the repository (circles/best,
# spheres/best), rebuild the visit lists, commit and push. Ends 4 h after the campaign's end time.
SP=${SP:-/tmp/claude-0/-home-user-oxgame/4a497322-6915-5b7d-b930-62ce754faa84/scratchpad}
REPO=$(cd "$(dirname "$0")/../.." && pwd)
W=$SP/week
BRANCH=claude/inspiring-albattani-0i32qu
cd "$REPO" || exit 1
while :; do
  END=$(date -u -d "$(cat "$W/until.txt"):00" +%s)
  if [ "$(date -u +%s)" -gt $((END + 14400)) ]; then echo "$(date -u +%FT%TZ) autosync: finished"; exit 0; fi
  echo "$(date -u +%FT%TZ) autosync cycle"
  if [ ! -f "$W/tables.stamp" ] || [ -n "$(find "$W/tables.stamp" -mmin +1380)" ]; then
    python3 tools/fetch_records.py --apply >> "$W/tables.log" 2>&1 && touch "$W/tables.stamp"
  fi
  python3 tools/circ_results.py --root "$SP/camp" --root "$SP/ccamp" 2>&1 | tail -2
  python3 tools/sphere_results.py sync --root "$SP/ccamp" 2>&1 | tail -2
  python3 tools/ops/ops.py refresh 2>&1 | tail -1
  (
    cd "$REPO/.." && git add packing
    if ! git diff --cached --quiet; then
      git commit -q -m "packing: results sync (extended campaign, automatic)" -m "$(cat "$W/trailer.txt")" \
        && echo "committed $(git rev-parse --short HEAD)"
      for i in 1 2 3 4; do
        git push -q -u origin "$BRANCH" && { echo pushed; break; }
        sleep $((2 ** i))
      done
    fi
  )
  bash "$REPO/tools/ops/start.sh"
  sleep 10800
done
