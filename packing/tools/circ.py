#!/usr/bin/env python3
"""Tools for circpack results (equal circles in a regular k-gon, Packomania style).

  python3 circ.py verify FILE [--rc RC]
  python3 circ.py export FILE --name "Name" --out chp40.pck [--rc RC]
  python3 circ.py status --code chp --dir DIR [--rc RC]
  python3 circ.py render FILE OUT.svg [--png OUT.png]

circpack files: '#' comments, then "k n R", then n lines "x y" -- unit circles in the
regular k-gon of circumradius R centered at the origin with a horizontal bottom side
(k = 0: the circle of radius R). Packomania fixes the container at circumradius 1
and reports the radius r = 1 / R -- except for the square (csq), which has side 1,
i.e. circumradius RC = sqrt(2)/2, so that r = RC / R.
"""
import glob
import math
import os
import sys
from decimal import Decimal as D, getcontext

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

HERE = os.path.dirname(os.path.abspath(__file__))
# Gains below this are what re-polishing a published packing can give; they are
# reported as marginal rather than as improvements.
SIGNIFICANT = 1e-7
RC = {0: 1, 4: math.sqrt(0.5), 5: 1, 6: 1, 7: 1, 9: 1, 10: 1, 11: 1, 12: 1, 13: 1, 14: 1, 15: 1, 16: 1, 17: 1}
CODE = {0: "cci", 4: "csq", 5: "cpt", 6: "chx", 7: "chp", 9: "cno", 10: "cdc", 11: "chd", 12: "cdd", 13: "ctd", 14: "crd",
        15: "cpd", 16: "cxd", 17: "ced"}


def read(path):
    nums = []
    for line in open(path):
        if not line.startswith("#"):
            nums += line.split()
    k, n, R = int(nums[0]), int(nums[1]), nums[2]
    pts = [(nums[3 + 2 * i], nums[4 + 2 * i]) for i in range(n)]
    return k, n, R, pts


def opt(args, name, default=None):
    return args[args.index(name) + 1] if name in args else default


def rc_dec(k, rc):
    """rc as a 50-digit Decimal; the unit square's sqrt(2)/2 is computed, not read from a float."""
    getcontext().prec = 50
    return D(2).sqrt() / 2 if k == 4 and abs(rc - math.sqrt(0.5)) < 1e-15 else D(str(rc))


def packomania_frame(k, R, pts, rc):
    """Radius and coordinates for a container of circumradius rc, as Decimals."""
    s = rc_dec(k, rc) / D(R)
    return s, [(D(x) * s, D(y) * s) for x, y in pts]


def dec_cos_sin(t):
    getcontext().prec += 10
    c = s = D(0)
    term, n = D(1), 0
    while abs(term) >= D(10) ** -(getcontext().prec + 5):
        if n % 4 == 0:
            c += term
        elif n % 4 == 1:
            s += term
        elif n % 4 == 2:
            c -= term
        else:
            s -= term
        n += 1
        term = term * t / n
    getcontext().prec -= 10
    return +c, +s


def dec_pi():
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


def check(k, r, pts, rc):
    """Exact-ish (50 digit) check that circles of radius r at pts fit in the k-gon."""
    getcontext().prec = 50
    if k == 0:  # circle of radius rc
        wall = min(D(str(rc)) - (x * x + y * y).sqrt() for x, y in pts) - r
    elif k == 4:  # square of circumradius rc, axis-parallel
        wall = min(rc_dec(k, rc) / D(2).sqrt() - max(abs(x), abs(y)) for x, y in pts) - r
    else:
        PI = dec_pi()
        apothem = D(str(rc)) * dec_cos_sin(PI / k)[0]
        normals = [dec_cos_sin(-PI / 2 + 2 * PI * j / k) for j in range(k)]
        wall = min(apothem - (x * c + y * s) for x, y in pts for c, s in normals) - r
    pair = None
    order = sorted(range(len(pts)), key=lambda i: pts[i][0])
    for a in range(len(order)):
        xi, yi = pts[order[a]]
        for b in range(a + 1, len(order)):
            xj, yj = pts[order[b]]
            if xj - xi > 3 * r:
                break
            d = ((xi - xj) ** 2 + (yi - yj) ** 2).sqrt() - 2 * r
            pair = d if pair is None or d < pair else pair
    return wall, pair if pair is not None else D(1)


def cmd_verify(args):
    k, n, R, pts = read(args[0])
    rc = float(opt(args, "--rc", RC.get(k, 1)))
    r, P = packomania_frame(k, R, pts, rc)
    wall, pair = check(k, r, P, rc)
    ok = wall >= 0 and pair >= 0
    print("k=%d n=%d r=%.15f  min wall clearance %.3e  min pair clearance %.3e  %s"
          % (k, n, r, wall, pair, "VALID" if ok else "INVALID"))
    return 0 if ok else 1


def cmd_export(args):
    k, n, R, pts = read(args[0])
    rc = float(opt(args, "--rc", RC.get(k, 1)))
    r, P = packomania_frame(k, R, pts, rc)
    # State a radius a hair below the computed one so rounding can never make it overlap.
    getcontext().prec = 50
    r_out = (r * (1 - D("1e-15"))).quantize(D("1e-18"))
    lines = ["%s" % r_out, opt(args, "--name", "anonymous")]
    lines += ["%s %s" % (x.quantize(D("1e-18")), y.quantize(D("1e-18"))) for x, y in P]
    out = opt(args, "--out")
    open(out, "w").write("\n".join(lines) + "\n")
    # Re-read what was written and check it again.
    vals = [l.split() for l in open(out).read().splitlines()]
    r2 = D(vals[0][0])
    P2 = [(D(a), D(b)) for a, b in vals[2:]]
    wall, pair = check(k, r2, P2, rc)
    print("wrote %s: r=%s  wall %.3e  pair %.3e  %s" % (out, r2, wall, pair,
                                                     "VALID" if wall >= 0 and pair >= 0 else "INVALID"))
    return 0


def records(code):
    rec = {}
    path = os.path.join(HERE, "..", "circles", "records", code + ".txt")
    if os.path.exists(path):
        for line in open(path):
            n, r = line.split()
            rec[int(n)] = float(r)
    return rec


def cmd_status(args):
    code, d = opt(args, "--code"), opt(args, "--dir")
    rec = records(code)
    k = {v: kk for kk, v in CODE.items()}[code]
    rc = float(opt(args, "--rc", RC[k]))
    rows = []
    for f in glob.glob(os.path.join(d, "*.txt")):
        kk, n, R, _ = read(f)
        r = rc / float(R)
        rows.append((n, r, rec.get(n)))
    better = 0
    for n, r, rr in sorted(rows):
        if rr is None:
            mark = "new (no record)"
        elif r > rr + SIGNIFICANT:
            mark = "BETTER by %.3e" % (r - rr)
            better += 1
        elif r > rr + 1e-10:
            mark = "marginal +%.1e" % (r - rr)
        elif r > rr - 1e-9:
            mark = "matches"
        else:
            mark = "behind by %.3e" % (rr - r)
        print("%s n=%3d r=%.12f  %s" % (code, n, r, mark))
    print("%s: %d sizes, %d better than the record" % (code, len(rows), better))


def cmd_render(args):
    from render import to_png
    k, n, R, pts = read(args[0])
    R = float(R)
    sc, pad = 360 / R, 10
    W = int(2 * R * sc + 2 * pad)

    def X(x):
        return pad + (x + R) * sc

    def Y(y):
        return pad + (R - y) * sc

    verts = [(R * math.cos(-math.pi / 2 + math.pi / k + 2 * math.pi * j / k),
              R * math.sin(-math.pi / 2 + math.pi / k + 2 * math.pi * j / k)) for j in range(k)]
    out = ['<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" font-family="sans-serif">' % (W, W + 24),
           '<rect width="100%" height="100%" fill="white"/>',
           '<text x="%d" y="%d" font-size="16">k=%d, n=%d, r=%.12f</text>' % (pad, W + 16, k, n, RC.get(k, 1) / R),
           '<polygon points="%s" fill="none" stroke="black" stroke-width="2"/>'
           % " ".join("%f,%f" % (X(a), Y(b)) for a, b in verts)]
    for x, y in pts:
        out.append('<circle cx="%f" cy="%f" r="%f" fill="#9db4d6" stroke="#333" stroke-width="1"/>'
                   % (X(float(x)), Y(float(y)), sc))
    out.append("</svg>")
    svg = "\n".join(out)
    open(args[1], "w").write(svg)
    if "--png" in args:
        to_png(svg, W, W + 24, opt(args, "--png"))


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(2)
    cmd, args = sys.argv[1], sys.argv[2:]
    sys.exit({"verify": cmd_verify, "export": cmd_export, "status": cmd_status,
              "render": cmd_render}[cmd](args) or 0)


if __name__ == "__main__":
    main()
