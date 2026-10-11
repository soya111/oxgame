#!/usr/bin/env python3
"""Collect a campaign's record improvements (and optionally new sizes) for Packomania.

  python3 circ_submit.py --root CAMPAIGN --name "Full Name" --out DIR [--new] [--min-gain 1e-10]

For every polygon, each packing that beats the published radius by more than
--min-gain (and, with --new, each size that has no record yet) is re-verified at
50 digits and written as CODE<N>.pck. One zip per polygon is made, named with the
submitter and the date as Packomania asks, plus a markdown table of the results.
"""
import argparse
import datetime
import glob
import os
import re
import sys
import zipfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import circ  # noqa: E402


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", required=True)
    ap.add_argument("--name", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--new", action="store_true", help="also include sizes without a published record")
    ap.add_argument("--min-gain", type=float, default=circ.SIGNIFICANT)
    args = ap.parse_args()
    os.makedirs(args.out, exist_ok=True)
    date = datetime.date.today().strftime("%Y-%m-%d")
    table = ["| polygon | N | new r | record r | gain |", "|---|---|---|---|---|"]
    for k, code in sorted(circ.CODE.items()):
        d = os.path.join(args.root, code)
        if not os.path.isdir(d):
            continue
        rec = circ.records(code)
        picked = []
        for f in glob.glob(os.path.join(d, "*.txt")):
            kk, n, R, _ = circ.read(f)
            r = circ.RC[k] / float(R)
            if (n in rec and r > rec[n] + args.min_gain) or (args.new and n not in rec):
                picked.append((n, f, r))
        files = []
        for n, f, r in sorted(picked):
            out = os.path.join(args.out, "%s%d.pck" % (code, n))
            circ.cmd_export([f, "--name", args.name, "--out", out])
            r_out, pts = read_pck(out)
            wall, pair = circ.check(k, r_out, pts, circ.RC[k])
            if wall < 0 or pair < 0:
                print("SKIP %s n=%d: failed verification" % (code, n))
                os.remove(out)
                continue
            files.append(out)
            gain = "%.3e" % (r - rec[n]) if n in rec else "new"
            table.append("| %d-gon (%s) | %d | %.12f | %s | %s |" % (k, code, n, r, "%.12f" % rec[n] if n in rec else "-", gain))
        if files:
            safe = re.sub(r"[^A-Za-z0-9]+", "_", args.name).strip("_")
            zpath = os.path.join(args.out, "%s_%s_%s.zip" % (code, safe, date))
            with zipfile.ZipFile(zpath, "w", zipfile.ZIP_DEFLATED) as z:
                for f in files:
                    z.write(f, os.path.basename(f))
            print("%s: %d packings -> %s" % (code, len(files), zpath))
    open(os.path.join(args.out, "summary.md"), "w").write("\n".join(table) + "\n")
    print("\n".join(table))


def read_pck(path):
    """Radius and coordinates of a .pck file (line 1 radius, line 2 names, then x y)."""
    lines = open(path).read().splitlines()
    return circ.D(lines[0]), [tuple(circ.D(v) for v in l.split()) for l in lines[2:] if l.strip()]


if __name__ == "__main__":
    main()
