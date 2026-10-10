#!/usr/bin/env python3
"""Render a tripack packing file to SVG (and PNG when headless Chromium is available).

  python3 render.py FILE OUT.svg [--png OUT.png] [--labels]
  python3 render.py FILE OUT.gif --friedman 175x152   (picture in the Packing Center style)

Pieces on the unit-lattice orientations are blue (pointing up) or light blue
(pointing down); tilted pieces are orange.
"""
import math
import os
import shutil
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from verify import read  # noqa: E402


def color(t):
    m = math.degrees(t) % 120.0
    if min(m, 120 - m) < 1e-4:
        return "#7f9cc9"
    if abs(m - 60) < 1e-4:
        return "#b9cbe6"
    return "#f2a65a"


def render(S, P, labels=False, k=90, pad=20):
    H = S * math.sqrt(3) / 2
    R = 1 / math.sqrt(3)
    W, Ht = S * k + 2 * pad, H * k + 2 * pad + 24

    def X(x):
        return pad + x * k

    def Y(y):
        return pad + 24 + (H - y) * k

    out = ['<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" font-family="sans-serif">' % (W, Ht),
           '<rect width="100%" height="100%" fill="white"/>',
           '<text x="%d" y="%d" font-size="16">n=%d, S=%.7f</text>' % (pad, pad + 4, len(P), S),
           '<polygon points="%f,%f %f,%f %f,%f" fill="none" stroke="black" stroke-width="2"/>'
           % (X(0), Y(0), X(S), Y(0), X(S / 2), Y(H))]
    for i, (x, y, t) in enumerate(P):
        pts = [(x + R * math.cos(t + math.pi / 2 + 2 * math.pi * j / 3),
                y + R * math.sin(t + math.pi / 2 + 2 * math.pi * j / 3)) for j in range(3)]
        out.append('<polygon points="%s" fill="%s" stroke="#333" stroke-width="1"/>'
                   % (" ".join("%f,%f" % (X(a), Y(b)) for a, b in pts), color(t)))
        if labels:
            out.append('<text x="%f" y="%f" font-size="12" text-anchor="middle" dominant-baseline="middle">%d</text>'
                       % (X(x), Y(y), i))
    out.append("</svg>")
    return "\n".join(out), int(W), int(Ht)


def render_friedman(S, P, W, Hpx):
    """Picture in the style of the Packing Center: container filling W x Hpx,
    grey pieces with black outlines, no text."""
    H = S * math.sqrt(3) / 2
    R = 1 / math.sqrt(3)
    inset = 1.0
    k = min((W - 2 * inset) / S, (Hpx - 2 * inset) / H)

    def X(x):
        return inset + x * k

    def Y(y):
        return Hpx - inset - y * k

    out = ['<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d">' % (W, Hpx),
           '<rect width="100%" height="100%" fill="white"/>']
    for x, y, t in P:
        pts = [(x + R * math.cos(t + math.pi / 2 + 2 * math.pi * j / 3),
                y + R * math.sin(t + math.pi / 2 + 2 * math.pi * j / 3)) for j in range(3)]
        out.append('<polygon points="%s" fill="#bfbfbf" stroke="black" stroke-width="1.2" stroke-linejoin="round"/>'
                   % " ".join("%f,%f" % (X(a), Y(b)) for a, b in pts))
    out.append('<polygon points="%f,%f %f,%f %f,%f" fill="none" stroke="black" stroke-width="2"/>'
               % (X(0), Y(0), X(S), Y(0), X(S / 2), Y(H)))
    out.append("</svg>")
    return "\n".join(out)


def to_png(svg, w, h, png):
    chrome = os.environ.get("CHROME") or shutil.which("chromium") or "/opt/pw-browsers/chromium"
    if not os.path.exists(chrome):
        print("no chromium found; skipped PNG")
        return
    with tempfile.TemporaryDirectory() as d:
        page = os.path.join(d, "page.html")
        with open(page, "w") as f:
            f.write('<!doctype html><html><body style="margin:0">%s</body></html>' % svg)
        subprocess.run([chrome, "--headless", "--no-sandbox", "--disable-gpu", "--hide-scrollbars",
                        "--screenshot=" + os.path.abspath(png), "--window-size=%d,%d" % (w, h + 160),
                        "file://" + page], check=True, capture_output=True)
    if shutil.which("convert"):  # headless window chrome eats part of the height; pad above, trim here
        subprocess.run(["convert", png, "-trim", "-bordercolor", "white", "-border", "10", png], check=True)


def to_gif_exact(svg, w, h, gif):
    """Rasterize at exactly w x h pixels and save as GIF (needs Chromium and ImageMagick)."""
    chrome = os.environ.get("CHROME") or shutil.which("chromium") or "/opt/pw-browsers/chromium"
    with tempfile.TemporaryDirectory() as d:
        page, png = os.path.join(d, "page.html"), os.path.join(d, "shot.png")
        with open(page, "w") as f:
            f.write('<!doctype html><html><body style="margin:0">%s</body></html>' % svg)
        subprocess.run([chrome, "--headless", "--no-sandbox", "--disable-gpu", "--hide-scrollbars",
                        "--screenshot=" + png, "--window-size=%d,%d" % (w + 40, h + 200), "file://" + page],
                       check=True, capture_output=True)
        subprocess.run(["convert", png, "-crop", "%dx%d+0+0" % (w, h), "+repage", gif], check=True)


def main():
    args = sys.argv[1:]
    if len(args) < 2:
        print(__doc__)
        sys.exit(2)
    S, P = read(args[0])
    if "--friedman" in args:  # e.g. --friedman 175x152 for tt44.gif
        w, h = map(int, args[args.index("--friedman") + 1].split("x"))
        to_gif_exact(render_friedman(S, P, w, h), w, h, args[1])
        return
    svg, w, h = render(S, P, labels="--labels" in args)
    with open(args[1], "w") as f:
        f.write(svg)
    if "--png" in args:
        to_png(svg, w, h, args[args.index("--png") + 1])


if __name__ == "__main__":
    main()
