#!/usr/bin/env python3
"""Rigorous verification of equal circles in a regular polygon (.pck files).

  python3 exact_verify.py K FILE.pck [FILE.pck ...]

The polygon is the regular K-gon of circumradius 1 centered at the origin with a
horizontal bottom side (Packomania's convention). Two containers follow
Packomania's other conventions instead: K = 4 is the unit square [-1/2, 1/2]^2
(table csq) and K = 0 is the unit circle (table cci); both are checked with
rationals only. A .pck file holds the radius on line 1, the authors on line 2
and one "x y" center per following line.

No floating point is used:
- the radius and the coordinates are decimal strings, read as exact rationals;
- circle-circle: dx^2 + dy^2 >= (2r)^2 is decided in exact integer arithmetic;
- circle-side: side j has outward normal (sin(2*pi*j/K), -cos(2*pi*j/K)) and lies
  at distance cos(pi/K) from the center. These irrational constants are enclosed
  in rational intervals that are proven to contain them -- pi from Machin's
  formula (alternating series: consecutive partial sums bracket the value),
  cos/sin from Taylor polynomials with the Lagrange bound |remainder| <=
  |t|^n / n!, plus |t - t0| for the uncertainty of the angle -- and the
  condition cos(pi/K) - r - (x*nx + y*ny) >= 0 is checked at the worst end of
  those intervals.
"VALID" is therefore a proof that every circle lies in the closed polygon and
that no two circles overlap (touching allowed). The reported clearances are
rigorous lower bounds.
"""
import functools
import math
import sys
from fractions import Fraction as F

EPS = F(1, 10**50)  # target width of the enclosures; packings carry ~1e-12 slack
GRID = 10**60  # enclosure endpoints are rounded outward to this grid to keep numbers small


def outward(lo, hi):
    """[lo, hi] widened to endpoints with denominator GRID (still contains [lo, hi])."""
    return F(math.floor(lo * GRID), GRID), F(math.ceil(hi * GRID), GRID)


def arctan_inv(m):
    """Rational interval containing arctan(1/m), m >= 2."""
    s, n, prev = F(0), 0, None
    while True:
        term = F((-1) ** n, (2 * n + 1) * m ** (2 * n + 1))
        prev, s = s, s + term
        n += 1
        if abs(term) < EPS * EPS:
            return (min(prev, s), max(prev, s))  # alternating, decreasing terms


A5, A239 = arctan_inv(5), arctan_inv(239)
PI = outward(16 * A5[0] - 4 * A239[1], 16 * A5[1] - 4 * A239[0])  # Machin: 16 atan(1/5) - 4 atan(1/239)


def cos_sin(q):
    """Rational intervals for cos(q*pi) and sin(q*pi), q rational."""
    lo, hi = sorted((q * PI[0], q * PI[1]))
    t0 = F(round((lo + hi) / 2 * 10**60), 10**60)  # rational point of the angle interval
    spread = max(hi - t0, t0 - lo)  # |t - t0| for the true angle t; cos, sin are 1-Lipschitz
    c, s, term, n = F(0), F(0), F(1), 0  # term = t0^n / n!
    while True:
        if n % 4 == 0:
            c += term
        elif n % 4 == 1:
            s += term
        elif n % 4 == 2:
            c -= term
        else:
            s -= term
        n += 1
        term = term * t0 / n
        if n > abs(t0) + 2 and abs(term) < EPS:
            break
    err = abs(term) + spread  # Lagrange remainder of degree n plus the angle uncertainty
    return outward(c - err, c + err), outward(s - err, s + err)


def read_pck(path):
    lines = [l for l in open(path).read().splitlines()]
    r = F(lines[0].strip())
    pts = [tuple(F(v) for v in l.split()[:2]) for l in lines[2:] if l.strip()]
    return r, pts


@functools.lru_cache(maxsize=None)
def polygon(k):
    """Enclosures of the apothem cos(pi/k) and of every outward side normal."""
    apo = cos_sin(F(1, k))[0]
    sides = []
    for j in range(k):
        cj, sj = cos_sin(F(2 * j, k))
        sides.append(((sj[0], sj[1]), (-cj[1], -cj[0])))  # normal = (sin, -cos) of 2*pi*j/k
    return apo, tuple(sides)


def verify(k, r, pts):
    """Returns (ok, min pair excess, min wall clearance lower bound)."""
    # circle-circle, exact: scale every value to integers with a common denominator
    den = r.denominator
    for x, y in pts:
        den = math.lcm(den, x.denominator, y.denominator)
    X = [(int(x * den), int(y * den)) for x, y in pts]
    need = (2 * int(r * den)) ** 2
    order = sorted(range(len(X)), key=lambda i: X[i][0])
    reach = 2 * int(r * den) + 1
    min_excess = None
    for a in range(len(order)):
        xi, yi = X[order[a]]
        for b in range(a + 1, len(order)):
            xj, yj = X[order[b]]
            if xj - xi > reach:  # exact: farther apart in x alone than 2r
                break
            excess = (xi - xj) ** 2 + (yi - yj) ** 2 - need
            if min_excess is None or excess < min_excess:
                min_excess = excess
    pair_ok = min_excess is None or min_excess >= 0
    if k in (0, 4):  # unit circle / unit square: the walls are rational too
        if k == 0:
            # x^2 + y^2 <= (1 - r)^2 with 1 - r >= 0; half the slack is a lower bound on the clearance
            min_wall = min((1 - r) ** 2 - x * x - y * y for x, y in pts) / 2 if r <= 1 else F(-1)
        else:
            min_wall = min(F(1, 2) - r - max(abs(x), abs(y)) for x, y in pts)
        pair_gap = None if min_excess is None else F(min_excess, den * den)
        return pair_ok and min_wall >= 0, pair_gap, min_wall
    # circle-side, rigorous intervals
    apo, sides = polygon(k)
    min_wall = None
    for x, y in pts:
        for (nx0, nx1), (ny0, ny1) in sides:
            reach_out = max(x * nx0, x * nx1) + max(y * ny0, y * ny1)  # largest possible x*nx + y*ny
            lb = apo[0] - r - reach_out
            if min_wall is None or lb < min_wall:
                min_wall = lb
    wall_ok = min_wall >= 0
    pair_gap = None if min_excess is None else F(min_excess, den * den)
    return pair_ok and wall_ok, pair_gap, min_wall


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        sys.exit(2)
    k = int(sys.argv[1])
    bad = 0
    for path in sys.argv[2:]:
        r, pts = read_pck(path)
        ok, pair, wall = verify(k, r, pts)
        bad += not ok
        print("%s  n=%d r=%s  pair: min (d^2-(2r)^2) = %s  wall: clearance >= %s  %s"
              % (path, len(pts), lines0(r), "%.3e" % float(pair) if pair is not None else "-",
                 "%.3e" % float(wall), "VALID (proof)" if ok else "NOT VERIFIED"))
    sys.exit(1 if bad else 0)


def lines0(r):
    return str(r.numerator * 10**18 // r.denominator / 10**18)[:20]


if __name__ == "__main__":
    main()
