#!/bin/bash
# Rebuild $SP/camp_seeds/CODE/N.txt (published packings as starting points) from Packomania's coordinate tarballs
#   https://www.packomania.com/CODE/txt/CODE_coords.tar.gz
# usage: seeds.sh [CODE ...]   (default: every table except hsp5/hsp6, whose published files lack coordinates)
SP=${SP:-/tmp/claude-0/-home-user-oxgame/4a497322-6915-5b7d-b930-62ce754faa84/scratchpad}
REPO=$(cd "$(dirname "$0")/../.." && pwd)
CODES=${*:-"ccq csc crt cpt chx chp coc cno cdc chd cdd ctd crd cpd cxd ced crc_100 crc_200 crc_300 crc_400 crc_500 crc_600 crc_700 crc_800 ssp scu hsp4 csq cci"}
for code in $CODES; do
  d=$SP/seeds_dl/$code; out=$SP/camp_seeds/$code
  if [ -n "$(ls "$out" 2>/dev/null | head -1)" ]; then echo "$code: seeds exist"; continue; fi
  rm -rf "$d"; mkdir -p "$d/x" "$out"
  name=${code}_coords.tar.gz
  case $code in crc_*) name=crc_0.${code#crc_}000000000_coords.tar.gz ;; esac  # the rectangles: 1 x 0.1 .. 0.8
  if ! curl -fsS -m 3600 -o "$d/c.tar.gz" "https://www.packomania.com/$code/txt/$name"; then
    echo "$code: no tarball"; rm -rf "$d"; continue
  fi
  tar -xzf "$d/c.tar.gz" -C "$d/x"
  f=$(find "$d/x" -type f | head -1); [ -n "$f" ] || { echo "$code: empty tarball"; continue; }
  src=$(dirname "$f")
  case $code in
    ssp|scu|hsp4) python3 "$REPO/tools/pm_seeds_nd.py" "$code" "$src" "$REPO/circles/records/$code.txt" "$out" ;;
    crc_*) python3 "$REPO/tools/pm_seeds.py" "$code" "$src" "$REPO/circles/records/$code.txt" "$out" \
             --pattern "crc{n}_0.${code#crc_}000000000.txt" ;;
    *) python3 "$REPO/tools/pm_seeds.py" "$code" "$src" "$REPO/circles/records/$code.txt" "$out" ;;
  esac 2>&1 | tail -2
  echo "$code: $(ls "$out" | wc -l) seeds"
  rm -rf "$d"
done
echo "seeds done"
