#!/usr/bin/env python3
"""Pick the sizes whose records look weakest, for a sweep's --only-file.

  python3 target_sizes.py CODE --dim D --nmin A --nmax B [--keep 0.4] [--window 8]
                          [--within FILE] [--skip-beaten DIR] [--all-up-to N0] > LIST

A record's density n r^D (r: Packomania's radius, unit container) is compared with
the median density of its neighbours n-W..n+W: a record clearly below its
neighbours' trend is more likely to be improvable. The cci campaign of
2026-10-12/13 bears this out: of N = 401..1300, the fifth with the lowest relative
density was beaten by search 38 times in 180 sizes, the highest fifth 11 times.

Printed: the sizes of A..B whose relative density is in the lowest --keep share of
that range, restricted to the sizes listed in --within (if given), without the
sizes that DIR/N.txt already beats by search (files not marked "seed from"), plus
every size up to --all-up-to (small sizes are cheap to visit).
"""
import argparse
import os
import statistics

PACK = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("code")
    ap.add_argument("--dim", type=int, required=True)
    ap.add_argument("--nmin", type=int, required=True)
    ap.add_argument("--nmax", type=int, required=True)
    ap.add_argument("--keep", type=float, default=0.4)
    ap.add_argument("--window", type=int, default=8)
    ap.add_argument("--within", default="")
    ap.add_argument("--skip-beaten", default="")
    ap.add_argument("--all-up-to", type=int, default=0)
    args = ap.parse_args()
    rec = {}
    for line in open(os.path.join(PACK, "circles", "records", args.code + ".txt")):
        p = line.split()
        if len(p) == 2:
            rec[int(p[0])] = float(p[1])

    def rel(n):
        nb = [m * rec[m] ** args.dim for m in range(n - args.window, n + args.window + 1) if m != n and m in rec]
        return n * rec[n] ** args.dim / statistics.median(nb) - 1

    sizes = [n for n in range(args.nmin, args.nmax + 1) if n in rec]
    ranked = sorted(sizes, key=rel)
    keep = set(ranked[:round(args.keep * len(ranked))])
    if args.within:
        keep &= {int(v) for v in open(args.within).read().split()}
    if args.skip_beaten:
        for n in list(keep):
            f = os.path.join(args.skip_beaten, "%d.txt" % n)
            if os.path.exists(f) and "seed from" not in open(f).readline():
                keep.discard(n)
    keep |= {n for n in sizes if n <= args.all_up_to}
    print("\n".join(str(n) for n in sorted(keep)))


if __name__ == "__main__":
    main()
