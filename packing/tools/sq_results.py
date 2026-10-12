#!/usr/bin/env python3
"""Copy a squares campaign's records into the repository and write squares/RESULTS.md.

  python3 sq_results.py --root CAMPAIGN [--root CAMPAIGN2 ...] --polished DIR

The records are squares/records.txt: The Squares Project register
(https://github.com/jlevy/squares), the current table of best known packings.
A packing whose side s is below the register's goes to squares/best/N.txt after
sq_verify.py has proven it (exact rational arithmetic). RESULTS.md tells two
kinds apart: a "refinement" is no better than local optimization of the
register's own packing (DIR/N.txt holds those polished packings), an
"improvement" is a packing the search found beyond that.

squares/superseded/ keeps the packings of 2026-10-12 that beat only David
Ellsworth's page as of 2026-09-24 (records_ellsworth_2026-09-24.txt); the
register already had better ones for every such n.
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


def table(path):
    t = {}
    for line in open(path):
        n, s = line.split()
        t[int(n)] = float(s)
    return t


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", action="append", required=True)
    ap.add_argument("--polished", required=True)
    args = ap.parse_args()
    rec = table(os.path.join(PACK, "squares", "records.txt"))
    old = table(os.path.join(PACK, "squares", "records_ellsworth_2026-09-24.txt"))
    dst = os.path.join(PACK, "squares", "best")
    os.makedirs(dst, exist_ok=True)
    rows, failed = [], []
    for n in sorted(rec):
        found = [(side(p), p) for p in (os.path.join(r, "best", "%d.txt" % n) for r in args.root) if side(p) is not None]
        if not found:
            continue
        s, src = min(found)
        if s >= rec[n] - TINY:
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
    sup = os.path.join(PACK, "squares", "superseded")
    sup_rows = []
    if os.path.isdir(sup):
        for f in sorted(os.listdir(sup), key=lambda x: int(x[:-4])):
            n = int(f[:-4])
            sup_rows.append((n, side(os.path.join(sup, f)), old.get(n), rec.get(n)))
    lines = ["# Squares in squares: results", "",
             "Updated %s. Records: The Squares Project register (`records.txt`, imported from" %
             datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%d %H:%M UTC"),
             "https://github.com/jlevy/squares at its 2026-10-12 state). Every packing listed is proven valid by",
             "`tools/sq_verify.py` (exact rational arithmetic).", "",
             "## Below the register (%d)" % len(rows), ""]
    if rows:
        lines += ["- *refinement*: local optimization of the register's packing itself (same arrangement);",
                  "- *improvement*: an arrangement found by the search that beats the polished register packing too.",
                  "", "| n | s (this search) | s (register) | gain | kind |", "|---|---|---|---|---|"]
        for n, s, r, kind in rows:
            lines.append("| %d | %.13f | %.13f | %.3e | %s |" % (n, s, r, r - s, kind))
    else:
        lines.append("None so far.")
    if sup_rows:
        lines += ["", "## Superseded: below David Ellsworth's page only (%d)" % len(sup_rows), "",
                  "On 2026-10-12 these were compared with Ellsworth's page (last changed 2026-09-24,",
                  "`records_ellsworth_2026-09-24.txt`) and reported as record improvements by mistake: the",
                  "register already held better packings for every one of them. Kept in `superseded/` as valid",
                  "packings only.", "",
                  "| n | s (this search) | s (Ellsworth page) | s (register) |", "|---|---|---|---|"]
        for n, s, o, r in sup_rows:
            lines.append("| %d | %.13f | %.13f | %s |" % (n, s, o, "%.13f" % r if r else "- (n > 324)"))
    if failed:
        lines += ["", "Failed verification (not copied): %s" % failed]
    open(os.path.join(PACK, "squares", "RESULTS.md"), "w").write("\n".join(lines) + "\n")
    print("\n".join(lines[:12 + len(rows)]))


if __name__ == "__main__":
    main()
