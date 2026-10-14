#!/usr/bin/env python3
"""Copy a campaign's improvements into the repository and write circles/RESULTS.md.

  python3 circ_results.py --root CAMPAIGN [--root CAMPAIGN2 ...]

Packings that beat the published radius, and packings for sizes without a record,
go to circles/best/CODE/N.txt (after an exact proof of validity); RESULTS.md lists
everything in circles/best against the records.
"""
import argparse
import datetime
import glob
import os
import shutil
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import circ  # noqa: E402
import exact_verify  # noqa: E402

PACK = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def container(k):
    if k <= -1000:
        return "1 x %s rectangle" % ("%.1f" % ((-k - 1000) / 1000))
    return {0: "circle", 4: "square", -1: "half disk", -2: "quarter disk", -3: "right triangle"}.get(k, "%d-gon" % k)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", action="append", required=True)
    ap.add_argument("--min-gain", type=float, default=circ.SIGNIFICANT)
    args = ap.parse_args()
    better, new, failed = [], {}, []
    for k, code in sorted(circ.CODE.items()):
        rec = circ.records(code)
        dst = os.path.join(PACK, "circles", "best", code)
        files = [f for root in args.root for f in glob.glob(os.path.join(root, code, "*.txt"))]
        for f in sorted(files, key=lambda p: int(os.path.basename(p)[:-4])):
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
                # Proof in exact arithmetic, on the values rounded as they would be submitted.
                r_q = (rr * (1 - circ.D("1e-15"))).quantize(circ.D("1e-18"))
                proof, _, _ = exact_verify.verify(
                    k, exact_verify.F(str(r_q)),
                    [(exact_verify.F(str(x.quantize(circ.D("1e-18")))), exact_verify.F(str(y.quantize(circ.D("1e-18")))))
                     for x, y in P])
                if wall < 0 or pair < 0 or not proof:
                    failed.append((code, n))
                    continue
                os.makedirs(dst, exist_ok=True)
                shutil.copyfile(f, target)
        # The table lists the repository copies (the best over every campaign so far).
        for f in glob.glob(os.path.join(dst, "*.txt")):
            kk, n, R, pts = circ.read(f)
            r = circ.RC[k] / float(R)
            if n in rec:
                better.append((k, code, n, r, rec[n]))
            else:
                new.setdefault((k, code), []).append((n, r))
    lines = ["# Circles in regular polygons and other containers: results", "",
             "Updated %s. Records are the Packomania tables in `records/` (see records/README.md for dates)." %
             datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%d %H:%M UTC"),
             "Every packing listed here is proven valid by `tools/exact_verify.py` (exact rational arithmetic,",
             "no floating point; see the README). Gains below",
             "%.0e (what re-polishing a published packing can give) are not counted." % args.min_gain, "",
             "## Better than the published record (%d)" % len(better), "",
             "| container | N | r (this search) | r (record) | gain |", "|---|---|---|---|---|"]
    for k, code, n, r, rr in sorted(better):
        lines.append("| %s (`%s`) | %d | %.12f | %.12f | %.3e |" % (container(k), code, n, r, rr, r - rr))
    lines += ["", "## Sizes beyond the published tables", "", "| container | N | count |", "|---|---|---|"]
    for (k, code), items in sorted(new.items()):
        ns = [n for n, _ in items]
        lines.append("| %s (`%s`) | %d-%d | %d |" % (container(k), code, min(ns), max(ns), len(ns)))
    if failed:
        lines += ["", "Failed verification (not copied): %s" % failed]
    open(os.path.join(PACK, "circles", "RESULTS.md"), "w").write("\n".join(lines) + "\n")
    print("\n".join(lines[:12 + len(better)]))
    print("new sizes:", sum(len(v) for v in new.values()), "failed:", failed)


if __name__ == "__main__":
    main()
