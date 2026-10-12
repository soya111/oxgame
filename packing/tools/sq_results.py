#!/usr/bin/env python3
"""Copy a squares campaign's improvements into the repository and write squares/RESULTS.md.

  python3 sq_results.py --root CAMPAIGN --polished DIR

A packing whose side s is below the published record goes to squares/best/N.txt
after sq_verify.py has proven it (exact rational arithmetic). RESULTS.md tells
two kinds apart: a "refinement" is no better than what local optimization of the
published packing itself gives (DIR/N.txt holds those polished records), an
"improvement" is a packing the search found beyond that.
"""
import argparse
import datetime
import os
import shutil
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sq_verify  # noqa: E402

PACK = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TINY = 1e-10  # below this a difference is rounding


def side(path):
    if not os.path.exists(path):
        return None
    return float(sq_verify.read(path)[1])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", required=True)
    ap.add_argument("--polished", required=True)
    args = ap.parse_args()
    rec = {}
    for line in open(os.path.join(PACK, "squares", "records.txt")):
        n, s = line.split()
        rec[int(n)] = float(s)
    dst = os.path.join(PACK, "squares", "best")
    os.makedirs(dst, exist_ok=True)
    rows, failed = [], []
    for n in sorted(rec):
        src = os.path.join(args.root, "best", "%d.txt" % n)
        s = side(src)
        if s is None or s >= rec[n] - TINY:
            continue
        target = os.path.join(dst, "%d.txt" % n)
        if side(target) is None or side(target) > s:
            _, s_str, sq = sq_verify.read(src)
            ok, _, _ = sq_verify.verify(s_str, sq)
            if not ok:
                failed.append(n)
                continue
            shutil.copyfile(src, target)
        s = side(target)
        pol = side(os.path.join(args.polished, "%d.txt" % n))
        kind = "improvement" if pol is None or s < pol - TINY else "refinement"
        rows.append((n, s, rec[n], kind))
    lines = ["# Squares in squares: results", "",
             "Updated %s. Records are the side lengths s on David Ellsworth's page" %
             datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%d %H:%M UTC"),
             "(https://kingbird.myphotos.cc/packing/squares_in_squares.html, fetched 2026-10-11; `records.txt`).",
             "Every packing listed is proven valid by `tools/sq_verify.py` (exact rational arithmetic).",
             "",
             "- *refinement*: local optimization of the published packing itself (same arrangement);",
             "- *improvement*: an arrangement found by the search that beats the polished record too.",
             "",
             "| n | s (this search) | s (record) | gain | kind |", "|---|---|---|---|---|"]
    for n, s, r, kind in rows:
        lines.append("| %d | %.13f | %.13f | %.3e | %s |" % (n, s, r, r - s, kind))
    lines += ["", "%d sizes below the record (%d refinements, %d improvements)." % (
        len(rows), sum(1 for r in rows if r[3] == "refinement"), sum(1 for r in rows if r[3] == "improvement"))]
    if failed:
        lines += ["", "Failed verification (not copied): %s" % failed]
    open(os.path.join(PACK, "squares", "RESULTS.md"), "w").write("\n".join(lines) + "\n")
    print("\n".join(lines))


if __name__ == "__main__":
    main()
