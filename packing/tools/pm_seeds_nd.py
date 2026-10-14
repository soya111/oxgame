#!/usr/bin/env python3
"""Convert Packomania sphere-packing coordinate files into spack seed files.

  python3 pm_seeds_nd.py CODE COORDS_DIR RADIUS_FILE OUT_DIR [--nmin A] [--nmax B]

CODE is ssp (spheres in the unit ball), scu (spheres in the unit cube
[-1/2, 1/2]^3) or hsp4 / hsp5 / hsp6 (hyperspheres in the unit hyperball).
Coordinate files are CODEn.txt or CODE-n.txt with lines "i x1 .. xd [r]";
RADIUS_FILE lists "N r". spack wants unit spheres, so coordinates are divided
by r and the container size becomes 1/r (ball radius) or 1/(2r) (cube
half-side). The printed digits can leave overlaps of the order of the last digit;
spack polishes every seed before using it.

Known defects of the published files: in ssp the last sphere's line carries no
coordinates (the seed then holds n - 1 spheres and spack adds the last one at
the roomiest spot), and hsp5 / hsp6 list only 4 coordinates per sphere (no seeds
are written for them).
"""
import argparse
import os
import re

KIND = {"ssp": (3, 0), "scu": (3, 1), "hsp4": (4, 0), "hsp5": (5, 0), "hsp6": (6, 0)}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("code")
    ap.add_argument("coords")
    ap.add_argument("radius")
    ap.add_argument("out")
    ap.add_argument("--nmin", type=int, default=1)
    ap.add_argument("--nmax", type=int, default=10**9)
    args = ap.parse_args()
    d, cube = KIND[args.code]
    radius = {}
    for line in open(args.radius):
        p = line.split()
        if len(p) == 2 and not line.startswith("#"):
            radius[int(p[0])] = float(p[1])
    files = {}
    for f in os.listdir(args.coords):
        m = re.fullmatch(re.escape(args.code) + r"-?(\d+)\.txt", f)
        if m:
            files[int(m.group(1))] = os.path.join(args.coords, f)
    os.makedirs(args.out, exist_ok=True)
    done = 0
    for n in sorted(radius):
        if not args.nmin <= n <= args.nmax or n not in files:
            continue
        pts = []
        for line in open(files[n]):
            p = line.split()
            if line.startswith("#") or len(p) < d + 1 or not p[0].isdigit():
                continue
            pts.append([float(v) for v in p[1:1 + d]])
        if len(pts) not in (n, n - 1) or (len(pts) == n - 1 and n == 1):
            print("skip %s n=%d: %d centers" % (args.code, n, len(pts)))
            continue
        r = radius[n]
        size = 0.5 / r if cube else 1.0 / r
        with open(os.path.join(args.out, "%d.txt" % n), "w") as f:
            f.write("# Packomania %s%d, r = %.15f%s\n%d %d %d %.17g\n" % (
                args.code, n, r, "" if len(pts) == n else ", last sphere missing", d, cube, len(pts), size))
            for x in pts:
                f.write(" ".join("%.17g" % (v / r) for v in x) + "\n")
        done += 1
    print("%s: %d seeds written to %s" % (args.code, done, args.out))


if __name__ == "__main__":
    main()
