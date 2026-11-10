#!/bin/bash
# Idempotent: build the solvers if needed, then (re)start the campaign driver and the autosync loop if they are not
# running. State lives in $SP (volatile); `python3 tools/ops/ops.py init` rebuilds it from the repository.
SP=${SP:-/tmp/claude-0/-home-user-oxgame/4a497322-6915-5b7d-b930-62ce754faa84/scratchpad}
REPO=$(cd "$(dirname "$0")/../.." && pwd)
W=$SP/week
mkdir -p "$W"
[ -f "$W/until.txt" ] && [ -f "$W/jobs.txt" ] || python3 "$REPO/tools/ops/ops.py" init
UNTIL=$(cat "$W/until.txt")
if [[ "$(date -u +%Y-%m-%dT%H:%M)" > "$UNTIL" ]]; then echo "past $UNTIL: nothing to start"; exit 0; fi
(
  flock 9
  for p in circpack spack; do
    [ -x "$REPO/build/$p" ] || { mkdir -p "$REPO/build"; g++ -O3 -march=native -std=c++17 -o "$REPO/build/$p" "$REPO/src/$p.cpp"; }
  done
) 9>"$W/build.lock"
if ! pgrep -f "campaign.py --root $SP/ccamp" >/dev/null; then
  nohup python3 "$REPO/tools/campaign.py" --root "$SP/ccamp" --workers 4 --slice 900 --until "$UNTIL" \
    --jobs-file "$W/jobs.txt" --only-dir "$SP/only" --seeds "$SP/camp_seeds" >> "$SP/ccamp/driver.log" 2>&1 &
  echo "driver started"
fi
if ! pgrep -f "ops/autosync.sh" >/dev/null; then
  nohup bash "$REPO/tools/ops/autosync.sh" >> "$W/autosync.log" 2>&1 &
  echo "autosync started"
fi
