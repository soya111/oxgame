#!/usr/bin/env python3
"""Convert Packomania coordinate files into circpack seed files.

  python3 pm_seeds.py CODE COORDS_DIR RADIUS_FILE OUT_DIR [--nmin A] [--nmax B]

COORDS_DIR holds CODEn.txt ("i x y" lines, '#' comments) as found in Packomania's
CODE_coords.tar.gz; RADIUS_FILE lists "N r". Packomania's containers are the unit
square centered at the origin (csq), the unit circle (cci) and regular polygons of
circumradius 1 with a horizontal bottom side. circpack wants unit circles, so the
coordinates are divided by r and the container size becomes rc / r, where rc is
the circumradius (sqrt(2)/2 for the unit square; the circle is circpack's k = 0).
The printed digits can leave overlaps of the order of the last digit; circpack
polishes every seed before using it.
"""
import argparse
import math
import os

SIDES = {"csq": 4, "cci": 0, "cpt": 5, "chx": 6, "chp": 7, "cno": 9, "cdc": 10, "chd": 11, "cdd": 12, "ctd": 13,
         "crd": 14, "cpd": 15, "cxd": 16, "ced": 17}
RC = {"csq": math.sqrt(0.5)}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("code")
    ap.add_argument("coords")
    ap.add_argument("radius")
    ap.add_argument("out")
    ap.add_argument("--nmin", type=int, default=1)
    ap.add_argument("--nmax", type=int, default=10**9)
    args = ap.parse_args()
    k, rc = SIDES[args.code], RC.get(args.code, 1.0)
    radius = {}
    for line in open(args.radius):
        p = line.split()
        if len(p) == 2 and not line.startswith("#"):
            radius[int(p[0])] = float(p[1])
    os.makedirs(args.out, exist_ok=True)
    done = 0
    for n in sorted(radius):
        path = os.path.join(args.coords, "%s%d.txt" % (args.code, n))
        if not args.nmin <= n <= args.nmax or not os.path.exists(path):
            continue
        pts = []
        for line in open(path):
            p = line.split()
            if line.startswith("#") or len(p) < 3:
                continue
            pts.append((float(p[1]), float(p[2])))
        if len(pts) != n:
            print("skip %s n=%d: %d centers" % (args.code, n, len(pts)))
            continue
        r = radius[n]
        with open(os.path.join(args.out, "%d.txt" % n), "w") as f:
            f.write("# Packomania %s%d, r = %.15f\n%d %d %.17g\n" % (args.code, n, r, k, n, rc / r))
            for x, y in pts:
                f.write("%.17g %.17g\n" % (x / r, y / r))
        done += 1
    print("%s: %d seeds written to %s" % (args.code, done, args.out))


if __name__ == "__main__":
    main()
