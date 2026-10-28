#!/usr/bin/env python3
"""Exact verification and repository sync for spack packings (spheres in a ball or cube).

  python3 sphere_results.py verify FILE            # a spack file, exact check of its exported form
  python3 sphere_results.py sync --root CAMPAIGN [--root ...] [--min-gain 1e-7]

Packomania's conventions: ssp / hsp4..hsp6 put spheres of radius r in the unit
ball, scu in the unit cube [-1/2, 1/2]^3. A spack file (unit spheres, container
size R) is exported like circ.py does it: r = 1/R (ball) or 1/(2R) (cube), stated
1e-15 relatively smaller, and radius and coordinates rounded to 1e-18. The check
then uses exact rationals only:
- ball: x . x <= (1 - r)^2 for every center (r <= 1),
- cube: |x_k| <= 1/2 - r for every coordinate,
- pairs: |x_i - x_j|^2 >= (2r)^2, in integers over a common denominator, for
  every pair (pruned by the first coordinate, exactly).
sync copies campaign packings that beat the record by at least --min-gain and pass
that proof to spheres/best/CODE/N.txt and writes spheres/RESULTS.md.
"""
import argparse
import datetime
import glob
import math
import os
import shutil
import sys
from decimal import Decimal as D, getcontext
from fractions import Fraction as F

PACK = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CODES = {"ssp": (3, 0), "scu": (3, 1), "hsp4": (4, 0), "hsp5": (5, 0), "hsp6": (6, 0)}


def read(path):
    nums = []
    for line in open(path):
        if not line.startswith("#"):
            nums += line.split()
    d, kind, n, R = int(nums[0]), int(nums[1]), int(nums[2]), nums[3]
    pts = [nums[4 + d * i: 4 + d * (i + 1)] for i in range(n)]
    return d, kind, n, R, pts


def export(path):
    """Radius and centers as Packomania would get them (exact rationals)."""
    getcontext().prec = 50
    d, kind, n, R, pts = read(path)
    scale = (D(1) / 2 if kind else D(1)) / D(R)
    r = (scale * (1 - D("1e-15"))).quantize(D("1e-18"))
    centers = [[F(str((D(v) * scale).quantize(D("1e-18")))) for v in p] for p in pts]
    return d, kind, n, F(str(r)), centers


def verify(path):
    d, kind, n, r, xs = export(path)
    if kind:
        wall = min(F(1, 2) - r - max(abs(v) for v in x) for x in xs)
    else:
        wall = min((1 - r) ** 2 - sum(v * v for v in x) for x in xs) if r <= 1 else F(-1)
    den = r.denominator
    for x in xs:
        for v in x:
            den = math.lcm(den, v.denominator)
    X = sorted([int(v * den) for v in x] for x in xs)
    two_r = 2 * int(r * den)
    need, worst = two_r * two_r, None
    for a in range(len(X)):
        for b in range(a + 1, len(X)):
            if X[b][0] - X[a][0] > two_r:  # exact: farther apart in x alone than 2r
                break
            e = sum((p - q) ** 2 for p, q in zip(X[a], X[b])) - need
            if worst is None or e < worst:
                worst = e
    ok = wall >= 0 and (worst is None or worst >= 0)
    return ok, float(r), wall, worst


def records(code):
    rec = {}
    for line in open(os.path.join(PACK, "circles", "records", code + ".txt")):
        p = line.split()
        if len(p) == 2:
            rec[int(p[0])] = float(p[1])
    return rec


def radius_of(path):
    d, kind, n, R, _ = read(path)
    return (0.5 if kind else 1.0) / float(R)


def sync(roots, min_gain):
    rows, failed = {}, []
    for code in CODES:
        rec = records(code) if os.path.exists(os.path.join(PACK, "circles", "records", code + ".txt")) else {}
        dst = os.path.join(PACK, "spheres", "best", code)
        for f in [f for root in roots for f in glob.glob(os.path.join(root, code, "*.txt"))]:
            n = int(os.path.basename(f)[:-4])
            r = radius_of(f)
            if n not in rec or r <= rec[n] + min_gain:
                continue
            target = os.path.join(dst, "%d.txt" % n)
            if os.path.exists(target) and radius_of(target) >= r:
                continue
            ok = verify(f)[0]
            if not ok:
                failed.append((code, n))
                continue
            os.makedirs(dst, exist_ok=True)
            shutil.copyfile(f, target)
        for f in glob.glob(os.path.join(dst, "*.txt")):
            n = int(os.path.basename(f)[:-4])
            first = open(f).readline()
            rows.setdefault(code, []).append((n, radius_of(f), rec[n], "polish" if "seed from" in first else "search"))
    lines = ["# Spheres in a ball or cube: results", "",
             "Updated %s. Records: Packomania's ssp, scu and hsp4-hsp6 tables (`circles/records/`, fetch dates in its README)." %
             datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%d %H:%M UTC"),
             "Every packing listed is proven valid by `tools/sphere_results.py` (exact rational arithmetic).",
             "Gains below %.0e are not counted. *polish*: local optimization of the published packing only;" % min_gain,
             "*search*: an arrangement found by the search.", ""]
    for code in CODES:
        items = sorted(rows.get(code, []))
        if not items:
            continue
        ahead = [t for t in items if t[1] > t[2] + min_gain]
        behind = [t for t in items if t[1] <= t[2] + min_gain]
        lines += ["## %s (%d)" % (code, len(ahead)), "", "| N | r (this search) | r (record) | gain | kind |",
                  "|---|---|---|---|---|"]
        lines += ["| %d | %.12f | %.12f | %.3e | %s |" % (n, r, rr, r - rr, k) for n, r, rr, k in ahead]
        lines.append("")
        if behind:
            lines += ["Matched or passed by a later update of the record table (kept in `spheres/best/%s/`, not counted):" % code,
                      "", "| N | r (this search) | r (record) | difference | kind |", "|---|---|---|---|---|"]
            lines += ["| %d | %.12f | %.12f | %.3e | %s |" % (n, r, rr, r - rr, k) for n, r, rr, k in behind]
            lines.append("")
    if failed:
        lines += ["Failed verification (not copied): %s" % failed]
    os.makedirs(os.path.join(PACK, "spheres"), exist_ok=True)
    open(os.path.join(PACK, "spheres", "RESULTS.md"), "w").write("\n".join(lines) + "\n")
    print("spheres:", {c: len(v) for c, v in rows.items()}, "failed:", failed)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("cmd", choices=["verify", "sync"])
    ap.add_argument("files", nargs="*")
    ap.add_argument("--root", action="append", default=[])
    ap.add_argument("--min-gain", type=float, default=1e-7)
    args = ap.parse_args()
    if args.cmd == "verify":
        bad = 0
        for f in args.files:
            ok, r, wall, worst = verify(f)
            bad += not ok
            print("%s r=%.15f wall clearance %.3e, pairs %s  %s" % (
                os.path.basename(f), r, float(wall), "-" if worst is None else ("clear" if worst >= 0 else "OVERLAP"),
                "VALID (proof)" if ok else "NOT VERIFIED"))
        sys.exit(1 if bad else 0)
    sync(args.root, args.min_gain)


if __name__ == "__main__":
    main()
