#!/usr/bin/env python3
"""Re-fetch Packomania's radius tables and keep circles/records/ up to date.

  python3 fetch_records.py [CODE ...] [--apply] [--force]

Each table is https://www.packomania.com/CODE/txt/radius.txt (lines "N r", '#' comments). Without CODE
the tables of the October 2026 search are fetched. For each one this prints how many radii changed, how
many grew by more than 1e-7, and the sizes that are new; with --apply the new table replaces
circles/records/CODE.txt and one line is appended to circles/records/UPDATES.log.

A table is applied only if it looks like the same table grown: every old size is still there (--force overrides).
A radius that became smaller is not taken over (records only improve; a smaller value means a table error, and a
packing of the old value is what this repository would otherwise count as a record). The entry N=1 is always kept
as it is here (the nonagon and decagon tables are published for another circumradius, see circles/records/README.md).
"""
import argparse
import datetime
import os
import subprocess
import sys

PACK = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REC = os.path.join(PACK, "circles", "records")
CODES = (["cci", "csq", "csc", "ccq", "crt", "coc", "ssp", "scu", "hsp4", "hsp5", "hsp6"] +
         ["crc_%d" % a for a in range(100, 900, 100)] + ["cpt", "chx", "cpd", "cxd"])
URL = "https://www.packomania.com/%s/txt/radius.txt"


def parse(text):
    rec = {}
    for line in text.splitlines():
        p = line.split()
        if len(p) == 2 and not line.startswith("#"):
            try:
                rec[int(p[0])] = (p[1], float(p[1]))
            except ValueError:
                pass
    return rec


def fetch(code):
    out = subprocess.run(["curl", "-sS", "-f", "-m", "120", URL % code], capture_output=True, text=True)
    if out.returncode:
        raise RuntimeError(out.stderr.strip() or "curl failed")
    return out.stdout


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("codes", nargs="*")
    ap.add_argument("--apply", action="store_true")
    ap.add_argument("--force", action="store_true")
    args = ap.parse_args()
    stamp = datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%MZ")
    bad = 0
    for code in args.codes or CODES:
        path = os.path.join(REC, code + ".txt")
        old = parse(open(path).read()) if os.path.exists(path) else {}
        try:
            new = parse(fetch(code))
        except RuntimeError as e:
            print("%-8s FETCH FAILED: %s" % (code, e))
            bad += 1
            continue
        if 1 in old and 1 in new:
            new[1] = old[1]
        missing = [n for n in old if n not in new]
        shrunk = sorted(n for n in old if n in new and new[n][1] < old[n][1] - 1e-9)
        for n in shrunk:  # a record does not get worse: keep the value known here (e.g. chx N=4 in the table of 2026-11-02)
            new[n] = old[n]
        grew = sorted(n for n in old if n in new and new[n][1] > old[n][1] + 1e-7)
        changed = [n for n in old if n in new and new[n][0] != old[n][0]]
        added = sorted(n for n in new if n not in old)
        gain = max((new[n][1] - old[n][1] for n in grew), default=0.0)
        note = ""
        if shrunk:
            note += "  kept the old value at %d sizes where the table is smaller (N=%s)" % (len(shrunk), ",".join(map(str, shrunk[:8])))
        if old and (missing or len(new) < 0.9 * len(old)):
            note += "  SUSPICIOUS (missing %d, %d sizes vs %d)" % (len(missing), len(new), len(old))
        print("%-8s %5d sizes: changed %4d, grew >1e-7 %4d (max %.1e), new %d%s" % (
            code, len(new), len(changed), len(grew), gain, len(added), note))
        if not (changed or added) or not args.apply:
            continue
        if "SUSPICIOUS" in note and not args.force:
            print("%-8s not applied" % code)
            bad += 1
            continue
        with open(path + ".tmp", "w") as f:
            f.write("".join("%d %s\n" % (n, new[n][0]) for n in sorted(new)))
        os.replace(path + ".tmp", path)
        with open(os.path.join(REC, "UPDATES.log"), "a") as f:
            f.write("%s %s changed %d, grew>1e-7 %d (max %.1e), new sizes %d%s\n" % (
                stamp, code, len(changed), len(grew), gain, len(added),
                ", kept old value at N=%s" % ",".join(map(str, shrunk)) if shrunk else ""))
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
