#!/usr/bin/env python3
"""Autonomous search campaign for squares in squares (sqpack).

Keeps --workers sqpack searches busy, one n per time slice, choosing the n with
the least time relative to its share; a slice that lowers s raises the share of
its n (halved every slice), so the effort follows wherever something is found.
The best packing of each n lives in ROOT/best/N.txt (seed it with polished
records); every slice is compared with the records file ("n s" lines) and the
results are appended to ROOT/summary.log.

  python3 sq_campaign.py --root DIR --records FILE [--workers 4] [--slice 300] [--hours 8] [--ns 11,17-19,...]
"""
import argparse
import json
import os
import random
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
PACK = os.path.dirname(HERE)


def parse_ns(spec):
    ns = []
    for part in spec.split(","):
        if "-" in part:
            a, b = part.split("-")
            ns += range(int(a), int(b) + 1)
        elif part:
            ns.append(int(part))
    return ns


def best_s(root, n):
    path = os.path.join(root, "best", "%d.txt" % n)
    if not os.path.exists(path):
        return None
    nums = [l for l in open(path) if not l.startswith("#")][0].split()
    return float(nums[1])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", required=True)
    ap.add_argument("--records", required=True)
    ap.add_argument("--workers", type=int, default=4)
    ap.add_argument("--slice", type=int, default=300)
    ap.add_argument("--hours", type=float, default=8)
    ap.add_argument("--ns", default="", help="sizes to search (default: every n in the records file)")
    args = ap.parse_args()
    binary = os.path.join(PACK, "build", "sqpack")
    rec = {}
    for line in open(args.records):
        p = line.split()
        if len(p) == 2 and not line.startswith("#"):
            rec[int(p[0])] = float(p[1])
    ns = parse_ns(args.ns) if args.ns else sorted(rec)
    os.makedirs(os.path.join(args.root, "logs"), exist_ok=True)
    os.makedirs(os.path.join(args.root, "best"), exist_ok=True)
    jobs = {n: [0.0, 1.0] for n in ns}  # n -> [seconds used, score]
    state = os.path.join(args.root, "jobs.json")
    if os.path.exists(state):
        for k, v in json.load(open(state)).items():
            if int(k) in jobs:
                jobs[int(k)] = v
    summary = open(os.path.join(args.root, "summary.log"), "a")
    running = {}
    deadline = time.time() + args.hours * 3600
    rng = random.Random()

    def log(msg):
        line = time.strftime("%Y-%m-%d %H:%M:%S ") + msg
        summary.write(line + "\n")
        summary.flush()
        print(line, flush=True)

    log("campaign start: %d sizes, %d workers, slice %ds" % (len(ns), args.workers, args.slice))
    while time.time() < deadline or running:
        for p in list(running):
            if p.poll() is None:
                continue
            n, path, start, before = running.pop(p)
            jobs[n][0] += time.time() - start
            after = best_s(args.root, n)
            gain = 0.0
            msg = "done n=%d s=%.13f" % (n, after) if after else "done n=%d (no packing)" % n
            if after and before and after < before - 1e-13:
                msg += "  improved by %.3e" % (before - after)
                gain = 1.0
            if after and n in rec:
                d = rec[n] - after
                if d > 1e-6:
                    msg += "  BETTER than record %.13f by %.3e" % (rec[n], d)
                elif d > 1e-10:
                    msg += "  refines record %.13f by %.3e" % (rec[n], d)
            log(msg)
            jobs[n][1] = 0.5 * jobs[n][1] + gain
            json.dump({str(k): v for k, v in jobs.items()}, open(state, "w"), indent=1)
        busy = {n for n, _, _, _ in running.values()}
        while len(running) < args.workers and time.time() < deadline:
            free = [n for n in ns if n not in busy]
            if not free:
                break
            n = min(free, key=lambda m: (jobs[m][0] / (0.5 + jobs[m][1]), rng.random()))
            fresh = 0.5 if n <= 60 else (0.3 if n <= 120 else 0.05)
            path = os.path.join(args.root, "logs", "%d_%d.log" % (n, int(time.time())))
            cmd = [binary, "search", "--n", str(n), "--out", os.path.join(args.root, "best", "%d.txt" % n),
                   "--time", str(args.slice), "--seed", str(rng.randrange(1 << 30)), "--fresh", str(fresh),
                   "--trials", "400" if n <= 120 else "200"]
            if n in rec:
                cmd += ["--record", repr(rec[n])]
            before = best_s(args.root, n)
            running[subprocess.Popen(cmd, stdout=open(path, "w"), stderr=subprocess.STDOUT)] = (n, path, time.time(), before)
            busy.add(n)
        time.sleep(3)
    log("campaign end")


if __name__ == "__main__":
    sys.exit(main())
