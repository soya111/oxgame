#!/usr/bin/env python3
"""Import The Squares Project register (https://github.com/jlevy/squares) into sqpack form.

  python3 sq_register.py REGISTER_CHECKOUT OUT_DIR RECORDS_FILE

Reads packing/witnesses/known-best/n-NNN.yaml (the best known packing of every
n = 1..324 with its side), writes OUT_DIR/N.txt ("n s" then "x y t" per square,
container centered at the origin) and RECORDS_FILE ("n s" lines, 25 digits).
Witnesses come in four forms -- corners (rational), center-angle (decimal,
degrees), center-basis (rational unit edge vector) and center-basis over Q(sqrt 2)
(pairs [a, b] meaning a + b sqrt 2); all are read exactly and rounded only when
written.
"""
import glob
import math
import os
import sys
from decimal import Decimal, getcontext
from fractions import Fraction

import yaml

getcontext().prec = 50
SQRT2 = Decimal(2).sqrt()


def num(v):
    """A scalar of any witness form as a 50-digit Decimal."""
    if isinstance(v, list):  # a + b sqrt 2
        return num(v[0]) + num(v[1]) * SQRT2
    f = Fraction(str(v))
    return Decimal(f.numerator) / Decimal(f.denominator)


def main():
    src, out, rec_path = sys.argv[1], sys.argv[2], sys.argv[3]
    os.makedirs(out, exist_ok=True)
    rows = []
    for path in sorted(glob.glob(os.path.join(src, "packing", "witnesses", "known-best", "n-*.yaml"))):
        w = yaml.safe_load(open(path))["witness"]
        n, rep = w["n"], w["representation"]
        side = num(w["side"])
        half = side / 2
        squares = []
        for sq in w["squares"]:
            if rep == "corners":
                pts = [(num(x), num(y)) for x, y in sq["corners"]]
                cx, cy = sum(p[0] for p in pts) / 4, sum(p[1] for p in pts) / 4
                t = math.atan2(float(pts[1][1] - pts[0][1]), float(pts[1][0] - pts[0][0]))
            elif rep == "center-angle":
                cx, cy = num(sq["center"][0]), num(sq["center"][1])
                t = float(num(sq.get("angle", "0")))
                if w["coordinates"]["angle_unit"] == "degrees":
                    t = math.radians(t)
                elif w["coordinates"]["angle_unit"] != "radians":
                    raise SystemExit("%s: angle unit %s" % (path, w["coordinates"]["angle_unit"]))
            elif rep == "center-basis":
                cx, cy = num(sq["center"][0]), num(sq["center"][1])
                c, s = num(sq["basis"][0]), num(sq["basis"][1])
                t = math.atan2(float(s), float(c))
            else:
                raise SystemExit("%s: unknown representation %s" % (path, rep))
            t %= math.pi / 2
            squares.append((cx - half, cy - half, t))
        if len(squares) != n:
            raise SystemExit("%s: %d squares for n=%d" % (path, len(squares), n))
        with open(os.path.join(out, "%d.txt" % n), "w") as f:
            f.write("# The Squares Project register, %s\n%d %s\n" % (os.path.basename(path), n, format(side, ".25f")))
            for x, y, t in squares:
                f.write("%s %s %.17g\n" % (format(x, ".25f"), format(y, ".25f"), t))
        rows.append((n, side))
    with open(rec_path, "w") as f:
        for n, side in rows:
            f.write("%d %s\n" % (n, format(side, ".25f")))
    print("%d packings written to %s" % (len(rows), out))


if __name__ == "__main__":
    main()
