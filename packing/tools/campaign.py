#!/usr/bin/env python3
"""Autonomous search campaign for circles in regular polygons (and in a circle or square).

Keeps --workers circpack sweeps busy, each on one (polygon, N range) job for a
time slice, then moves on to the job with the least time relative to its share.
A job's share grows with its number of sizes and with its recent success (record
improvements count 1, new sizes 1/4, halved every slice), so the effort follows
wherever the search is still finding something. Every finished slice
is compared with the Packomania records and the improvements are appended to
ROOT/summary.log.

  python3 campaign.py --root DIR [--workers 4] [--slice 900] [--hours 8] [--ext 100]
                     [--jobs CODE:N1-N2[:WEIGHT],...]

Without --jobs every polygon table is searched (plus sizes up to --ext); --jobs
names the (table, N range) jobs instead, e.g. "cci:301-400,cci:401-500".
"""
import argparse
import json
import math
import os
import random
import re
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
PACK = os.path.dirname(HERE)
SIDES = {"cpt": 5, "chx": 6, "chp": 7, "cno": 9, "cdc": 10, "chd": 11, "cdd": 12, "ctd": 13, "crd": 14,
         "cpd": 15, "cxd": 16, "ced": 17, "cci": 0, "csq": 4}
RC = {"csq": math.sqrt(0.5)}  # the unit square; every other container has circumradius 1
POLYGONS = ["cpt", "chx", "chp", "cno", "cdc", "chd", "cdd", "ctd", "crd", "cpd", "cxd", "ced"]


def records(code):
    rec = {}
    for line in open(os.path.join(PACK, "circles", "records", code + ".txt")):
        n, r = line.split()
        rec[int(n)] = float(r)
    return rec


def best_r(root, code, n):
    path = os.path.join(root, code, "%d.txt" % n)
    if not os.path.exists(path):
        return None
    nums = [l for l in open(path) if not l.startswith("#")][0].split()
    return RC.get(code, 1.0) / float(nums[2])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", required=True)
    ap.add_argument("--workers", type=int, default=4)
    ap.add_argument("--slice", type=int, default=900)
    ap.add_argument("--hours", type=float, default=8)
    ap.add_argument("--ext", type=int, default=100, help="extend each polygon up to this N")
    ap.add_argument("--seeds", default="", help="directory with CODE/N.txt starting packings (e.g. the records)")
    ap.add_argument("--jobs", default="", help="CODE:N1-N2[:WEIGHT],... instead of the polygon tables")
    args = ap.parse_args()
    binary = os.path.join(PACK, "build", "circpack")
    os.makedirs(os.path.join(args.root, "logs"), exist_ok=True)
    jobs = []  # [code, nmin, nmax, weight, seconds_used, score]
    for spec in filter(None, args.jobs.split(",")):
        part = spec.split(":")
        n1, n2 = map(int, part[1].split("-"))
        jobs.append([part[0], n1, n2, float(part[2]) if len(part) > 2 else 1.0, 0.0, 1.0])
    for code in POLYGONS if not jobs else []:
        rec = records(code)
        top = 0
        while top + 1 in rec:  # the contiguous part of the table (the hexagon also lists a few larger N)
            top += 1
        jobs.append([code, 1, top, top / 48.0, 0.0, 1.0])  # time in proportion to the number of sizes
        if args.ext > top:
            jobs.append([code, top + 1, args.ext, 0.5, 0.0, 1.0])
    state = os.path.join(args.root, "jobs.json")  # time used per job survives restarts
    if os.path.exists(state):
        used = json.load(open(state))
        for j in jobs:
            v = used.get("%s %d-%d" % (j[0], j[1], j[2]), 0.0)
            j[4], j[5] = (v, 1.0) if isinstance(v, (int, float)) else (v[0], v[1])
    summary = open(os.path.join(args.root, "summary.log"), "a")
    running = {}  # Popen -> (job, log path, start)
    deadline = time.time() + args.hours * 3600
    rng = random.Random()

    def log(msg):
        line = time.strftime("%Y-%m-%d %H:%M:%S ") + msg
        summary.write(line + "\n")
        summary.flush()
        print(line, flush=True)

    log("campaign start: %d jobs, %d workers, slice %ds" % (len(jobs), args.workers, args.slice))
    while time.time() < deadline or running:
        for p in list(running):
            if p.poll() is None:
                continue
            job, path, start = running.pop(p)
            job[4] += time.time() - start
            code, n1, n2 = job[0], job[1], job[2]
            rec = records(code)
            improved = [l.strip() for l in open(path) if l.startswith("[")]
            log("done %s %d-%d (%d improvements in slice)" % (code, n1, n2, len(improved)))
            gain = 0.0
            for l in improved:
                m = re.search(r"n=\s*(\d+) r=([0-9.]+)", l)
                n, r = int(m.group(1)), float(m.group(2))
                if n in rec and r > rec[n] + 1e-7:  # smaller gains are mere re-polishing
                    log("  BETTER %s n=%d r=%.12f record %.12f (+%.3e)" % (code, n, r, rec[n], r - rec[n]))
                    gain += 1
                elif n not in rec:
                    log("  NEW %s n=%d r=%.12f" % (code, n, r))
                    gain += 0.25
            # Adaptive share: recent success raises a job's priority, old success decays.
            job[5] = 0.5 * job[5] + gain
            json.dump({"%s %d-%d" % (j[0], j[1], j[2]): [j[4], j[5]] for j in jobs}, open(state, "w"), indent=1)
        busy = {(j[0], j[1]) for j, _, _ in running.values()}
        while len(running) < args.workers and time.time() < deadline:
            free = [j for j in jobs if (j[0], j[1]) not in busy]
            if not free:
                break
            job = min(free, key=lambda j: (j[4] / (j[3] * (0.5 + j[5])), rng.random()))
            code, n1, n2 = job[0], job[1], job[2]
            path = os.path.join(args.root, "logs", "%s_%d-%d_%d.log" % (code, n1, n2, int(time.time())))
            cmd = [binary, "sweep", "--k", str(SIDES[code]), "--rc", str(RC.get(code, 1.0)),
                   "--dir", os.path.join(args.root, code), "--records",
                   os.path.join(PACK, "circles", "records", code + ".txt"), "--nmin", str(n1), "--nmax", str(n2),
                   "--time", str(args.slice), "--seed", str(rng.randrange(1 << 30)), "--trials", "300"]
            if args.seeds:
                cmd += ["--seeds", os.path.join(args.seeds, code)]
            os.makedirs(os.path.join(args.root, code), exist_ok=True)
            running[subprocess.Popen(cmd, stdout=open(path, "w"), stderr=subprocess.STDOUT)] = (job, path, time.time())
            busy.add((code, n1))
            log("start %s %d-%d (used %.0fs so far, score %.2f)" % (code, n1, n2, job[4], job[5]))
        time.sleep(5)
    log("campaign end")


if __name__ == "__main__":
    sys.exit(main())
