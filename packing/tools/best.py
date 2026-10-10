#!/usr/bin/env python3
"""List packing files by side length: python3 best.py [DIR or FILES...]"""
import glob
import os
import sys

paths = []
for a in sys.argv[1:] or ["results"]:
    paths += sorted(glob.glob(os.path.join(a, "*.txt"))) if os.path.isdir(a) else [a]
rows = []
for p in paths:
    head = [l for l in open(p) if not l.startswith("#")][:1]
    if head:
        n, S = head[0].split()[:2]
        rows.append((int(n), float(S), p))
for n, S, p in sorted(rows):
    print("n=%-3d S=%.10f  %s" % (n, S, p))
