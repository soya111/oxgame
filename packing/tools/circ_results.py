#!/usr/bin/env python3
"""Copy a campaign's improvements into the repository and write circles/RESULTS.md.

  python3 circ_results.py --root CAMPAIGN

Packings that beat the published radius, and packings for sizes without a record,
go to circles/best/CODE/N.txt (verified at 50 digits first); RESULTS.md lists
them against the records.
"""
import argparse
import datetime
import glob
import os
import shutil
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import circ  # noqa: E402

PACK = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", required=True)
    ap.add_argument("--min-gain", type=float, default=1e-10)
    args = ap.parse_args()
    better, new, failed = [], {}, []
    for k, code in sorted(circ.CODE.items()):
        d = os.path.join(args.root, code)
        if not os.path.isdir(d):
            continue
        rec = circ.records(code)
        dst = os.path.join(PACK, "circles", "best", code)
        for f in sorted(glob.glob(os.path.join(d, "*.txt")), key=lambda p: int(os.path.basename(p)[:-4])):
            kk, n, R, pts = circ.read(f)
            r = circ.RC[k] / float(R)
            if not ((n in rec and r > rec[n] + args.min_gain) or n not in rec):
                continue
            target = os.path.join(dst, "%d.txt" % n)
            if os.path.exists(target) and circ.RC[k] / float(circ.read(target)[2]) >= r:
                pass  # repository copy is already at least as good
            else:
                rr, P = circ.packomania_frame(k, R, pts, circ.RC[k])
                wall, pair = circ.check(k, rr, P, circ.RC[k])
                if wall < 0 or pair < 0:
                    failed.append((code, n))
                    continue
                os.makedirs(dst, exist_ok=True)
                shutil.copyfile(f, target)
            if n in rec:
                better.append((k, code, n, r, rec[n]))
            else:
                new.setdefault((k, code), []).append((n, r))
    lines = ["# Circles in regular polygons: results", "",
             "Updated %s. Records are the Packomania tables in `records/` (last changed in 2020)." %
             datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%d %H:%M UTC"),
             "Every packing listed here passed `tools/circ.py verify` (50-digit arithmetic).", "",
             "## Better than the published record (%d)" % len(better), "",
             "| polygon | N | r (this search) | r (record) | gain |", "|---|---|---|---|---|"]
    for k, code, n, r, rr in sorted(better):
        lines.append("| %d-gon (`%s`) | %d | %.12f | %.12f | %.3e |" % (k, code, n, r, rr, r - rr))
    lines += ["", "## Sizes beyond the published tables", "", "| polygon | N | count |", "|---|---|---|"]
    for (k, code), items in sorted(new.items()):
        ns = [n for n, _ in items]
        lines.append("| %d-gon (`%s`) | %d-%d | %d |" % (k, code, min(ns), max(ns), len(ns)))
    if failed:
        lines += ["", "Failed verification (not copied): %s" % failed]
    open(os.path.join(PACK, "circles", "RESULTS.md"), "w").write("\n".join(lines) + "\n")
    print("\n".join(lines[:12 + len(better)]))
    print("new sizes:", sum(len(v) for v in new.values()), "failed:", failed)


if __name__ == "__main__":
    main()
