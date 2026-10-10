#!/usr/bin/env python3
"""Verify a triangle-in-triangle packing file written by tripack.

File format: '#' comment lines, then "n S", then n lines "x y theta".
Piece = unit equilateral triangle with centroid (x, y); vertex k at angle
theta + 90deg + 120deg*k, circumradius 1/sqrt(3). Container corners
(0,0), (S,0), (S/2, S*sqrt(3)/2).

Two independent checks:
  1. double precision separating-axis test (same method as the n=44 tweet);
  2. 60-digit Decimal arithmetic with segment-crossing / point-in-triangle tests
     and exact Euclidean clearances.
Also reports the tightest container side for the layout as given.

  python3 verify.py FILE [--standalone OUT.py]
--standalone writes a self-contained verification script (stdlib only).
"""
import itertools
import math
import sys
from decimal import Decimal as D, getcontext


def read(path):
    nums = []
    for line in open(path):
        if line.startswith("#"):
            continue
        nums += [float(v) for v in line.split()]
    n, S = int(nums[0]), nums[1]
    rows = [tuple(nums[2 + 3 * i: 5 + 3 * i]) for i in range(n)]
    assert len(rows) == n
    return S, rows


def check_double(S, P):
    R, r3 = 1 / math.sqrt(3), math.sqrt(3)
    V = [[(x + R * math.cos(t + math.pi / 2 + 2 * math.pi * k / 3),
           y + R * math.sin(t + math.pi / 2 + 2 * math.pi * k / 3)) for k in range(3)] for x, y, t in P]
    cont = min(min(y, (r3 * x - y) / 2, (r3 * (S - x) - y) / 2) for T in V for x, y in T)

    def unit(a, b):
        d = math.dist(a, b)
        return ((a[1] - b[1]) / d, (b[0] - a[0]) / d)

    def proj(T, n):
        return [p[0] * n[0] + p[1] * n[1] for p in T]

    def sep(A, B, n):
        return max(min(proj(B, n)) - max(proj(A, n)), min(proj(A, n)) - max(proj(B, n)))

    def gap(A, B):
        return max(sep(A, B, unit(T[k], T[(k + 1) % 3])) for T in (A, B) for k in range(3))

    pair = min(gap(A, B) for A, B in itertools.combinations(V, 2))
    return cont, pair


def check_decimal(S, P):
    getcontext().prec = 60

    def machin_pi():
        getcontext().prec += 5

        def arctan_inv(x):
            x = D(x)
            x2, term = x * x, 1 / D(x)
            s, n, sign = term, 1, -1
            while True:
                term /= x2
                n += 2
                t = term / n
                if t < D(10) ** -(getcontext().prec + 2):
                    break
                s += sign * t
                sign = -sign
            return s

        p = 4 * (4 * arctan_inv(5) - arctan_inv(239))
        getcontext().prec -= 5
        return +p

    PI = machin_pi()

    def cos_sin(t):
        t = t - 2 * PI * (t / (2 * PI)).to_integral_value(rounding="ROUND_FLOOR")
        getcontext().prec += 10
        c = s = D(0)
        term, n = D(1), 0
        while abs(term) >= D(10) ** -(getcontext().prec + 5):
            c, s = (c + term, s) if n % 4 == 0 else (c, s + term) if n % 4 == 1 else (c - term, s) if n % 4 == 2 else (c, s - term)
            n += 1
            term = term * t / n
        getcontext().prec -= 10
        return +c, +s

    S = D(repr(S))
    R, r3 = 1 / D(3).sqrt(), D(3).sqrt()
    V = []
    for x, y, t in P:
        x, y, t = D(repr(x)), D(repr(y)), D(repr(t))
        V.append([(x + R * c, y + R * s) for c, s in (cos_sin(t + PI / 2 + 2 * PI * k / 3) for k in range(3))])
    cont = min(min(y, (r3 * x - y) / 2, (r3 * (S - x) - y) / 2) for T in V for x, y in T)

    pts = [p for T in V for p in T]
    normals = [(D(0), D(1)), (r3 / 2, D(-1) / 2), (-r3 / 2, D(-1) / 2)]
    tight = 2 * (-sum(min(nx * px + ny * py for px, py in pts) for nx, ny in normals)) / r3

    def orient(a, b, c):
        return (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])

    def crosses(p1, p2, q1, q2):
        return orient(q1, q2, p1) * orient(q1, q2, p2) <= 0 and orient(p1, p2, q1) * orient(p1, p2, q2) <= 0

    def inside(p, T):
        o = [orient(T[i], T[(i + 1) % 3], p) for i in range(3)]
        return all(v >= 0 for v in o) or all(v <= 0 for v in o)

    def seg_dist(p, a, b):
        ab = (b[0] - a[0], b[1] - a[1])
        ap = (p[0] - a[0], p[1] - a[1])
        t = max(D(0), min(D(1), (ap[0] * ab[0] + ap[1] * ab[1]) / (ab[0] ** 2 + ab[1] ** 2)))
        dx, dy = ap[0] - t * ab[0], ap[1] - t * ab[1]
        return (dx * dx + dy * dy).sqrt()

    overlaps, clear = [], None
    for i, j in itertools.combinations(range(len(V)), 2):
        A, B = V[i], V[j]
        cx = (sum(p[0] for p in A) - sum(p[0] for p in B)) / 3
        cy = (sum(p[1] for p in A) - sum(p[1] for p in B)) / 3
        if cx * cx + cy * cy > D(2):  # centroids farther than 2R + slack: cannot touch
            continue
        if (any(crosses(A[a], A[(a + 1) % 3], B[b], B[(b + 1) % 3]) for a in range(3) for b in range(3))
                or any(inside(p, B) for p in A) or any(inside(p, A) for p in B)):
            overlaps.append((i, j))
            continue
        d = min(min(seg_dist(p, B[b], B[(b + 1) % 3]) for p in A for b in range(3)),
                min(seg_dist(p, A[a], A[(a + 1) % 3]) for p in B for a in range(3)))
        clear = d if clear is None else min(clear, d)
    return cont, overlaps, clear, tight


STANDALONE = '''# Verify: {n} unit equilateral triangles in an equilateral triangle of side S = {S}
# Each row: x y theta (centroid, rotation); vertices at angle theta+90deg+120deg*k, circumradius 1/sqrt(3)
# Container: (0,0), (S,0), (S/2, S*sqrt(3)/2). Python 3, standard library only.
import math, itertools
S = {S}
DATA = """
{rows}
"""
P = [tuple(map(float, l.split())) for l in DATA.strip().splitlines()]
R, r3 = 1 / math.sqrt(3), math.sqrt(3)
V = [[(x + R * math.cos(t + math.pi / 2 + 2 * math.pi * k / 3), y + R * math.sin(t + math.pi / 2 + 2 * math.pi * k / 3)) for k in range(3)] for x, y, t in P]
cont = min(min(y, (r3 * x - y) / 2, (r3 * (S - x) - y) / 2) for T in V for x, y in T)
unit = lambda a, b: ((a[1] - b[1]) / math.dist(a, b), (b[0] - a[0]) / math.dist(a, b))
proj = lambda T, n: [p[0] * n[0] + p[1] * n[1] for p in T]
sep = lambda A, B, n: max(min(proj(B, n)) - max(proj(A, n)), min(proj(A, n)) - max(proj(B, n)))
gap = lambda A, B: max(sep(A, B, unit(T[k], T[(k + 1) % 3])) for T in (A, B) for k in range(3))
pair = min(gap(A, B) for A, B in itertools.combinations(V, 2))
print("pieces:", len(P))
print("min vertex-to-side distance: %.3e" % cont)
print("min separation between pieces: %.3e" % pair)
print("VALID" if len(P) == {n} and cont > 0 and pair > 0 else "INVALID")
'''


def main():
    args = sys.argv[1:]
    if not args:
        print(__doc__)
        sys.exit(2)
    S, P = read(args[0])
    cont, pair = check_double(S, P)
    dcont, overlaps, clear, tight = check_decimal(S, P)
    print("pieces: %d   S = %.10f" % (len(P), S))
    print("[double ] min vertex-to-side %.3e   min SAT separation %.3e" % (cont, pair))
    print("[decimal] min vertex-to-side %.3e   overlapping pairs %d   min clearance %.3e"
          % (dcont, len(overlaps), clear if clear is not None else float("nan")))
    print("tight container side for this layout: %.10f" % tight)
    ok = cont > 0 and pair > 0 and dcont > 0 and not overlaps
    print("VALID" if ok else "INVALID")
    if "--standalone" in args:
        out = args[args.index("--standalone") + 1]
        rows = "\n".join("%.12f %.12f %.12f" % r for r in P)
        open(out, "w").write(STANDALONE.format(n=len(P), S=repr(S), rows=rows))
        print("wrote", out)
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
