#!/bin/bash
# Per-polygon tally of a campaign directory against the Packomania records.
# usage: circ_overview.sh CAMPAIGN_ROOT
root=$1
here=$(dirname "$0")
for code in cpt chp cno cdc chd cdd ctd crd cpd cxd ced; do
  [ -d "$root/$code" ] || continue
  python3 "$here/circ.py" status --code $code --dir "$root/$code" | awk -v c=$code '
    /BETTER/ {b++} /marginal/ {g++} /matches/ {m++} /behind/ {w++} /new \(no record\)/ {x++}
    END {printf "%s  better %3d  marginal %3d  match %3d  behind %3d  new %3d\n", c, b, g, m, w, x}'
done
