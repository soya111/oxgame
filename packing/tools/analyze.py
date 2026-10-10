#!/usr/bin/env python3
"""Describe the structure of a packing: which pieces sit on the unit lattice
anchored at each container corner, which are tilted, and how many contacts
the layout has.

  python3 analyze.py FILE

The lattice anchored at the left corner has up-triangle centroids at
(a + b/2 + 1/2, b*h + h/3) and down-triangle centroids at (a + b/2 + 1,
b*h + 2h/3), h = sqrt(3)/2. The right- and top-corner lattices are its
translates by (S - k)*(1, 0) and (S - k)*(1/2, h) for k = ceil(S).
"""
import itertools
import math
import os
import sys
from collections import Counter

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from verify import read  # noqa: E402

H = math.sqrt(3) / 2
TOL = 1e-5


def orientation(t):
    m = math.degrees(t) % 120.0
    if min(m, 120 - m) < 1e-4:
        return "up"
    if abs(m - 60) < 1e-4:
        return "down"
    return "tilt %+.2f" % (((m + 60) % 120) - 60)


def on_left_lattice(x, y, kind):
    off = H / 3 if kind == "up" else 2 * H / 3
    b = (y - off) / H
    if abs(b - round(b)) > TOL:
        return False
    a = x - round(b) / 2 - (0.5 if kind == "up" else 1.0)
    return abs(a - round(a)) < TOL


def main():
    S, P = read(sys.argv[1])
    k = math.ceil(S - 1e-9)
    shifts = {"left": (0.0, 0.0), "right": (S - k, 0.0), "top": ((S - k) / 2, (S - k) * H)}
    groups = {}
    for i, (x, y, t) in enumerate(P):
        o = orientation(t)
        if o in ("up", "down"):
            hits = [c for c, (dx, dy) in shifts.items() if on_left_lattice(x - dx, y - dy, o)]
            key = "lattice:" + "+".join(hits) if hits else "aligned, off-lattice"
        else:
            key = o
        groups.setdefault(key, []).append(i)
    print("n=%d S=%.10f (k=%d, shrink %.6f)" % (len(P), S, k, k - S))
    for key in sorted(groups, key=lambda g: (not g.startswith("lattice"), g)):
        print("  %-24s %2d  %s" % (key, len(groups[key]), groups[key]))

    R, r3 = 1 / math.sqrt(3), math.sqrt(3)
    V = [[(x + R * math.cos(t + math.pi / 2 + 2 * math.pi * j / 3),
           y + R * math.sin(t + math.pi / 2 + 2 * math.pi * j / 3)) for j in range(3)] for x, y, t in P]

    def gap(A, B):
        best = -1e9
        for T in (A, B):
            for j in range(3):
                a, b = T[j], T[(j + 1) % 3]
                d = math.dist(a, b)
                n = ((a[1] - b[1]) / d, (b[0] - a[0]) / d)
                pa = [p[0] * n[0] + p[1] * n[1] for p in A]
                pb = [p[0] * n[0] + p[1] * n[1] for p in B]
                best = max(best, min(pb) - max(pa), min(pa) - max(pb))
        return best

    contacts = sum(1 for A, B in itertools.combinations(V, 2) if gap(A, B) < 1e-6)
    walls = Counter()
    for T in V:
        for name, f in (("bottom", lambda x, y: y), ("left", lambda x, y: (r3 * x - y) / 2),
                        ("right", lambda x, y: (r3 * (S - x) - y) / 2)):
            if min(f(x, y) for x, y in T) < 1e-6:
                walls[name] += 1
    print("  contacts: %d piece-piece, walls %s" % (contacts, dict(walls)))


if __name__ == "__main__":
    main()
