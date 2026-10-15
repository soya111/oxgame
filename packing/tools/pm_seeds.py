#!/usr/bin/env python3
"""Convert Packomania coordinate files into circpack seed files.

  python3 pm_seeds.py CODE COORDS_DIR RADIUS_FILE OUT_DIR [--nmin A] [--nmax B] [--pattern P]

COORDS_DIR holds CODEn.txt ("i x y" lines, '#' comments) as found in Packomania's
CODE_coords.tar.gz; RADIUS_FILE lists "N r". Packomania's containers are the unit
square centered at the origin (csq), the unit circle (cci), regular polygons of
circumradius 1 with a horizontal bottom side, the half disk x^2 + y^2 <= 1, y >= 0
(csc), the quarter disk x^2 + y^2 <= 1, x, y >= 0 (ccq) and the isosceles right
triangle x, y >= 0, x + y <= 1 (crt), the last three in circpack's frame (k = -1,
-2, -3). circpack wants unit circles, so the coordinates are divided by r and the
container size becomes rc / r, where rc is the circumradius (sqrt(2)/2 for the
unit square, 1 otherwise; the circle is circpack's k = 0). Empty coordinate files
(csc's ten sizes improved in September 2026) are skipped. The rectangles crc_100 ..
crc_800 (|x| <= 1/2, |y| <= a/2) name their files differently: --pattern
"crc{n}_0.100000000000.txt".
The printed digits can leave overlaps of the order of the last digit; circpack
polishes every seed before using it.
"""
import argparse
import math
import os

SIDES = {"csq": 4, "cci": 0, "cpt": 5, "chx": 6, "chp": 7, "coc": 8, "cno": 9, "cdc": 10, "chd": 11, "cdd": 12, "ctd": 13,
         "crd": 14, "cpd": 15, "cxd": 16, "ced": 17, "csc": -1, "ccq": -2, "crt": -3}
SIDES.update({"crc_%d" % a: -1000 - a for a in range(100, 900, 100)})
RC = {"csq": math.sqrt(0.5)}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("code")
    ap.add_argument("coords")
    ap.add_argument("radius")
    ap.add_argument("out")
    ap.add_argument("--nmin", type=int, default=1)
    ap.add_argument("--nmax", type=int, default=10**9)
    ap.add_argument("--pattern", default="", help="file name with {n} for the size (default CODE{n}.txt)")
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
        path = os.path.join(args.coords, (args.pattern or args.code + "{n}.txt").replace("{n}", str(n)))
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
