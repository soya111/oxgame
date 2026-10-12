#!/usr/bin/env python3
"""Write sqpack packings in the text format of David Ellsworth's tools.

  python3 sq_export.py IN.txt OUT.txt

The format is the one of parse_svg_packing.py / check_packing.py from
https://github.com/Davidebyzero/packing_squares_in_squares__tools:
"Final s: S", then "Square i: x=X, y=Y, deg=D" lines, the container being
[-S/2, S/2]^2. The angle written is the one sq_verify.py proves (the rational
rotation nearest to the file's angle), in degrees to 40 digits.
"""
import sys
from decimal import Decimal, getcontext

import sq_verify

getcontext().prec = 60


def dec_atan(u):
    """atan(u) for |u| <= 1 (Euler's series, converges for every u)."""
    getcontext().prec += 10
    u = Decimal(u)
    y = u * u / (1 + u * u)
    term = u / (1 + u * u)
    total, k = term, 0
    while abs(term) > Decimal(10) ** -(getcontext().prec + 2):
        k += 1
        term = term * y * (2 * k) / (2 * k + 1)
        total += term
    getcontext().prec -= 10
    return +total


def main():
    src, dst = sys.argv[1], sys.argv[2]
    n, s, sq = sq_verify.read(src)
    pi = 4 * dec_atan(Decimal(1))
    lines = ["Final s: %s" % s, ""]
    for i, (x, y, t) in enumerate(sq, 1):
        u, _, _ = sq_verify.rational_rotation(t)
        deg = 2 * dec_atan(Decimal(u.numerator) / Decimal(u.denominator)) * 180 / pi
        lines.append("Square %d: x=%s, y=%s, deg=%s" % (i, x, y, format(deg, ".40f")))
    open(dst, "w").write("\n".join(lines) + "\n")


if __name__ == "__main__":
    main()
