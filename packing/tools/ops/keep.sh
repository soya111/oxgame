#!/bin/bash
# Keep-alive: for $1 seconds (default 6480) make sure the driver and autosync run, once a minute, quietly.
# The session's container is reclaimed a few minutes after the last tracked background task ends, and a task may
# live 2 h at most. Two of these loops (108 min each) are kept running, ending 54 min apart: the session is woken
# every 54 min, while its prompt cache is still warm (a wake-up after more than 1 h re-reads the whole context),
# and a wake-up that fails (usage limit) leaves the other loop holding the container for 54 more minutes.
REPO=$(cd "$(dirname "$0")/../.." && pwd)
END=$((SECONDS + ${1:-6480}))
while [ $SECONDS -lt $END ]; do
  bash "$REPO/tools/ops/start.sh" >/dev/null 2>&1
  r=$((END - SECONDS)); [ $r -gt 60 ] && r=60
  [ $r -gt 0 ] && sleep $r
done
