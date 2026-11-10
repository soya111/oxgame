#!/usr/bin/env python3
"""Bookkeeping of the long campaign that has to survive a lost scratchpad ($SP).

  python3 ops.py init      # rebuild $SP/week, $SP/only_base, $SP/only and $SP/ccamp from the repository
  python3 ops.py refresh   # recompute the sizes per code (current tables), the visit lists $SP/only/CODE.txt (sizes where
                           # circles/best or spheres/best is not above the table by more than 1e-7) and the jobs

The visit lists keep the campaign on the sizes where a record can still be beaten. The best packings found so far
live in the repository (circles/best, spheres/best), so `init` copies them back as the starting points of the next
sweeps. Search state for sizes that are still below their record exists only in $SP and is lost with it.
"""
import glob
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PACK = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, os.path.join(PACK, "tools"))
import circ  # noqa: E402
import sphere_results as sr  # noqa: E402

SP = os.environ.get("SP", "/tmp/claude-0/-home-user-oxgame/4a497322-6915-5b7d-b930-62ce754faa84/scratchpad")
W = os.path.join(SP, "week")
GAIN = 1e-7
K = {v: k for k, v in circ.CODE.items()}
CODES = sorted(K) + list(sr.CODES)
EDGES = [2, 61, 151, 301, 401, 501, 601, 701, 1001, 1201, 1401, 1601, 1801, 2001, 2501, 10 ** 9]


def trailer():
    """The attribution lines of the newest commit that carries them (so their text is not repeated in this file)."""
    out = subprocess.run(["git", "-C", PACK, "log", "-100", "--format=%(trailers:only,unfold)%x00"],
                         capture_output=True, text=True).stdout
    for block in out.split("\0"):
        if "Claude-Session:" in block:
            return block.strip() + "\n"
    return ""


def best_dir(code):
    return os.path.join(PACK, "spheres" if code in sr.CODES else "circles", "best", code)


def records(code):
    return sr.records(code) if code in sr.CODES else circ.records(code)


def radius(code, path):
    """(r, n) from the header line of a campaign file: 'k n R' (circpack) or 'd kind n R' (spack)."""
    with open(path) as f:
        for line in f:
            if line.strip() and not line.startswith("#"):
                h = line.split()
                break
    if code in sr.CODES:
        return (0.5 if int(h[1]) else 1.0) / float(h[3]), int(h[2])
    return circ.RC[K[code]] / float(h[2]), int(h[1])


def universe(code):
    s = {n for n in records(code) if n >= 2}
    s |= {int(os.path.basename(f)[:-4]) for f in glob.glob(os.path.join(best_dir(code), "*.txt"))}
    if code in K and 5 <= K[code] <= 17:  # the polygons were searched up to N = 100 even where the table stops earlier
        s |= set(range(2, 101))
    return s


def beaten(code):
    rec, out = records(code), set()
    for f in glob.glob(os.path.join(best_dir(code), "*.txt")):
        r, n = radius(code, f)
        if n in rec and r > rec[n] + GAIN:
            out.add(n)
    return out


def jobs_for(code, sizes):
    out = []
    for lo, hi in zip(EDGES, EDGES[1:]):
        part = [n for n in sizes if lo <= n < hi]
        if not part:
            continue
        a, b = min(part), max(part)
        w = len(part) / 48.0 * (1.0 if b <= 300 else 0.5 if b <= 700 else 0.25)
        if code.startswith("hsp") and a >= 300:
            w = 0.3
        if b > 1000:  # one visit takes minutes there; keep the share small
            w = min(w, 0.4)
        out.append("%s:%d-%d:%.3f" % (code, a, b, max(w, 0.02)))
    return out


def write_if_missing(path, text):
    if not os.path.exists(path):
        with open(path, "w") as f:
            f.write(text)


def refresh():
    """Recompute the universe (table sizes, so new table sizes join automatically), the visit lists and the jobs."""
    total, jobs = 0, []
    for code in CODES:
        sizes = sorted(universe(code))
        with open(os.path.join(SP, "only_base", code + ".txt"), "w") as f:
            f.write("".join("%d\n" % n for n in sizes))
        gone = beaten(code)
        todo = [n for n in sizes if n not in gone]
        with open(os.path.join(SP, "only", code + ".txt"), "w") as f:
            f.write("".join("%d\n" % n for n in todo))
        total += len(todo)
        jobs += jobs_for(code, sizes)
    tmp = os.path.join(W, "jobs.txt.tmp")
    with open(tmp, "w") as f:
        f.write(",".join(jobs) + "\n")
    os.replace(tmp, os.path.join(W, "jobs.txt"))
    print("refresh: %d sizes to visit, %d jobs" % (total, len(jobs)))


def init():
    for d in ("week", "ccamp", "only", "only_base", "camp_seeds"):
        os.makedirs(os.path.join(SP, d), exist_ok=True)
    write_if_missing(os.path.join(W, "until.txt"), "2026-12-02T00:20\n")
    write_if_missing(os.path.join(W, "trailer.txt"), trailer())
    restored = 0
    for code in CODES:
        dst = os.path.join(SP, "ccamp", code)
        os.makedirs(dst, exist_ok=True)
        for f in glob.glob(os.path.join(best_dir(code), "*.txt")):
            t = os.path.join(dst, os.path.basename(f))
            if not os.path.exists(t):
                shutil.copyfile(f, t)
                restored += 1
    print("init: %d best packings restored" % restored)
    refresh()


if __name__ == "__main__":
    {"init": init, "refresh": refresh}[sys.argv[1] if len(sys.argv) > 1 else "init"]()
