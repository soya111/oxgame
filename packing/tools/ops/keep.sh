#!/bin/bash
# Keep-alive: for $1 seconds (default 6900) make sure the driver and autosync run, once a minute, quietly.
REPO=$(cd "$(dirname "$0")/../.." && pwd)
END=$((SECONDS + ${1:-6900}))
while [ $SECONDS -lt $END ]; do
  bash "$REPO/tools/ops/start.sh" >/dev/null 2>&1
  sleep 60
done
