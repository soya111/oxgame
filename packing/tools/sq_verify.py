#!/usr/bin/env python3
"""Exact verification of unit squares packed in a square (sqpack files).

  python3 sq_verify.py FILE [FILE ...] [--write-exact OUT_DIR]

A sqpack file holds "n s" and then one "x y t" line per square (center and angle
in radians); the container is [-s/2, s/2]^2.

No floating point is used for the decision. Each angle t is replaced by the
rotation with cos = (1 - u^2)/(1 + u^2), sin = 2u/(1 + u^2) for a rational u
within 1e-30 of tan(t/2), so every square is exactly a unit square with rational
vertices; centers and s are read as exact decimals. Then
- walls: |x| + (|cos| + |sin|)/2 <= s/2 for both coordinates, exactly;
- pairs: by the separating axis theorem two unit squares have disjoint interiors
  iff on one of the four edge directions u (rational unit vectors here) the
  projections satisfy |d . u| >= 1/2 + (|cos D| + |sin D|)/2, D the angle
  difference, with cos D and sin D rational -- checked exactly for every pair
  whose centers are closer than sqrt(2) (farther pairs cannot overlap).
"VALID" is a proof for the rationalized packing, whose angles differ from the
file's by less than 1e-29.
"""
import math
import os
import sys
from decimal import Decimal, getcontext
from fractions import Fraction as F

getcontext().prec = 60
DEN = 10**30


def read(path):
    nums = []
    for line in open(path):
        if not line.startswith("#"):
            nums += line.split()
    n, s = int(nums[0]), nums[1]
    sq = [(nums[2 + 3 * i], nums[3 + 3 * i], nums[4 + 3 * i]) for i in range(n)]
    return n, s, sq


def rational_rotation(t_str):
    """(u, cos, sin) with u rational ~ tan(t/2): an exact rotation close to t."""
    t = Decimal(t_str)
    # tan(t/2) to 50 digits via math on Decimal: use a float start and Newton-free series-free route:
    # tan(x) = sin(x)/cos(x) with Taylor series in Decimal (|x| <= pi/4 after reduction below).
    half = t / 2
    sn, cs = dec_sin_cos(half)
    u = F(int((sn / cs * DEN).to_integral_value()), DEN)
    den = 1 + u * u
    return u, (1 - u * u) / den, 2 * u / den


def dec_sin_cos(x):
    getcontext().prec += 10
    s = c = Decimal(0)
    term, n = Decimal(1), 0
    while abs(term) > Decimal(10) ** -(getcontext().prec + 2) or n < 3:
        if n % 4 == 0:
            c += term
        elif n % 4 == 1:
            s += term
        elif n % 4 == 2:
            c -= term
        else:
            s -= term
        n += 1
        term = term * x / n
    getcontext().prec -= 10
    return +s, +c


def verify(s_str, sq):
    H = F(Decimal(s_str)) / 2
    items = []
    for x, y, t in sq:
        _, c, s = rational_rotation(t)
        items.append((F(Decimal(x)), F(Decimal(y)), c, s))
    wall = min(H - (abs(c) + abs(s)) / 2 - max(abs(x), abs(y)) for x, y, c, s in items)
    order = sorted(range(len(items)), key=lambda i: items[i][0])
    pair = None
    for a in range(len(order)):
        xi, yi, ci, si = items[order[a]]
        for b in range(a + 1, len(order)):
            xj, yj, cj, sj = items[order[b]]
            dx, dy = xj - xi, yj - yi
            if dx * dx >= 2:  # sorted by x: all later squares are farther still
                break
            if dx * dx + dy * dy >= 2:
                continue
            cD, sD = ci * cj + si * sj, si * cj - ci * sj
            need = F(1, 2) + (abs(cD) + abs(sD)) / 2
            M = max(abs(dx * ci + dy * si), abs(-dx * si + dy * ci), abs(dx * cj + dy * sj), abs(-dx * sj + dy * cj))
            gap = M - need
            if pair is None or gap < pair:
                pair = gap
    ok = wall >= 0 and (pair is None or pair >= 0)
    return ok, wall, pair


def main():
    files = [a for a in sys.argv[1:] if not a.startswith("--")]
    if not files:
        print(__doc__)
        sys.exit(2)
    bad = 0
    for path in files:
        n, s, sq = read(path)
        ok, wall, pair = verify(s, sq)
        bad += not ok
        print("%s  n=%d s=%s  wall gap >= %.3e  pair gap >= %s  %s" % (
            os.path.basename(path), n, s, float(wall), "-" if pair is None else "%.3e" % float(pair),
            "VALID (proof)" if ok else "NOT VERIFIED"))
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
