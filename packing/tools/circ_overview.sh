#!/bin/bash
# Per-polygon tally of a campaign directory against the Packomania records.
# usage: circ_overview.sh CAMPAIGN_ROOT
root=$1
here=$(dirname "$0")
for code in chp cno cdc chd cdd ctd crd cpd ced; do
  [ -d "$root/$code" ] || continue
  python3 "$here/circ.py" status --code $code --dir "$root/$code" | awk -v c=$code '
    /BETTER/ {b++} /matches/ {m++} /behind/ {w++} /new \(no record\)/ {x++}
    END {printf "%s  better %3d  match %3d  behind %3d  new %3d\n", c, b, m, w, x}'
done
