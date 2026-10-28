#!/usr/bin/env python3
"""Summarize which Packomania records the repository's packings beat.

  python3 week_report.py [--out WEEK_2026-10.md]

Reads circles/best/CODE/N.txt (circpack files) and spheres/best/CODE/N.txt (spack
files) and compares each with the record table in circles/records/CODE.txt. Only
packings that passed the exact checks of exact_verify.py / sphere_results.py are in
those directories, and only gains of at least 1e-7 are counted (smaller ones are what
re-polishing a published packing gives).

A packing is *search* when the search found an arrangement of its own, *polish* when
it is the published packing locally optimized (the first line of the file says
"seed from"). collect() returns one row per table for other scripts (the figure).
"""
import argparse
import datetime
import glob
import os
import statistics
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PACK = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import circ  # noqa: E402
import sphere_results  # noqa: E402

# code, name, record table N range, last update of the record table (as fetched 2026-10-11..15)
CIRCLES = [
    ("cci", "円を円に", "1〜2600 ほか", "2024-12-25"),
    ("csq", "円を正方形に", "1〜1000 ほか", "2026-09-10"),
    ("csc", "円を半円に", "1〜250", "2026-09-09"),
    ("ccq", "円を四分円に", "1〜600", "2018-06-21"),
    ("crt", "円を直角二等辺三角形に", "1〜299", "2018-06-21"),
    ("coc", "円を正八角形に", "1〜48", "2026-09-23"),
] + [("crc_%d" % a, "円を長方形 1×0.%d に" % (a // 100), "1〜%s" % n, "2018-06-21")
     for a, n in zip(range(100, 900, 100), (200, 347, 300, 299, 300, 307, 437, 420))]
SPHERES = [
    ("ssp", "球を球に（3 次元）", "1153 サイズ", "2026-07-13"),
    ("scu", "球を立方体に", "1134 サイズ", "2026-10-08"),
    ("hsp4", "4 次元超球", "2〜300", "2025-07-06"),
    ("hsp5", "5 次元超球", "2〜300", "2025-08-01"),
    ("hsp6", "6 次元超球", "2〜259", "2025-07-31"),
]


def records(code):
    rec = {}
    with open(os.path.join(PACK, "circles", "records", code + ".txt")) as f:
        for line in f:
            p = line.split()
            if len(p) == 2:
                rec[int(p[0])] = float(p[1])
    return rec


def is_polish(path):
    with open(path) as f:
        return "seed from" in f.readline()


def row(code, name, span, updated, kind):
    rec = records(code)
    found = []  # (n, gain, polish)
    base = "circles" if kind == "circle" else "spheres"
    for f in glob.glob(os.path.join(PACK, base, "best", code, "*.txt")):
        n = int(os.path.basename(f)[:-4])
        if kind == "circle":
            k, _, R, _ = circ.read(f)
            r = circ.RC[k] / float(R)
        else:
            r = sphere_results.radius_of(f)
        if n in rec and r > rec[n] + 1e-7:
            found.append((n, r - rec[n], is_polish(f)))
    search = [g for _, g, p in found if not p]
    return {"code": code, "name": name, "span": span, "updated": updated, "kind": kind,
            "sizes": len(rec), "beaten": len(found), "search": len(search),
            "polish": len(found) - len(search),
            "max_gain": max(search, default=0.0),  # search only: the polish-only gains are a different claim
            "max_n": max((t for t in found if not t[2]), key=lambda t: t[1])[0] if search else None,
            "median_search": statistics.median(search) if search else 0.0}


def collect():
    return ([row(c, n, s, u, "circle") for c, n, s, u in CIRCLES] +
            [row(c, n, s, u, "sphere") for c, n, s, u in SPHERES])


def table(rows):
    lines = ["| 容器 | コード | 記録表の範囲 | 記録表の最終更新 | 上回った数 | うち探索 | うち研磨のみ | 探索の最大改善幅（N） | 探索の改善幅の中央値 |",
             "|---|---|---|---|---|---|---|---|---|"]
    for r in rows:
        gain = "%.1e（N=%d）" % (r["max_gain"], r["max_n"]) if r["search"] else "-"
        med = "%.1e" % r["median_search"] if r["search"] else "-"
        lines.append("| %s | `%s` | %s | %s | %d | %d | %d | %s | %s |" % (
            r["name"], r["code"], r["span"], r["updated"], r["beaten"], r["search"], r["polish"], gain, med))
    t = [sum(r[k] for r in rows) for k in ("beaten", "search", "polish")]
    lines.append("| **合計** | | | | **%d** | **%d** | **%d** | | |" % tuple(t))
    return lines


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.join(PACK, "WEEK_2026-10.md"))
    args = ap.parse_args()
    rows = collect()
    circles = [r for r in rows if r["kind"] == "circle"]
    spheres = [r for r in rows if r["kind"] == "sphere"]
    lines = ["# 探索（2026-10-14〜28、10/21 に一週間延長）の結果", "",
             "更新 %s。Packomania の記録表と比べて、半径が 1e-7 以上大きい配置をリポジトリに持つサイズの数です。" %
             datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%d %H:%M UTC"),
             "どの配置も、有理数演算だけの厳密な検証（`tools/exact_verify.py`、`tools/sphere_results.py`）を通っています。", "",
             "- **探索**: 探索が自力で見つけた配置。**研磨のみ**: 公開されている配置そのものを局所最適化しただけで",
             "  記録を上回ったもの（公開配置が収束しきっていなかった）。意味合いが違うので分けて数えています。",
             "- 記録表は 2026-10-11〜15 に取得したもので、その後の更新は反映していません。",
             "- 円を円に（`cci`）と円を正方形に（`csq`）の一部は、この週の前（10/12〜13）の探索分を含みます。",
             "  正多角形 12 表（10/11〜12 の探索、235 件）はこの一覧に含めていません（`circles/RESULTS.md` の合計にはあります）。",
             "- 実行環境が止まって探索できなかった時間が、10/14 02:04〜10/28 00:35 UTC の約 358 時間のうち約 86 時間あります",
             "  （10/17 17:45〜10/20 14:25 の約 69 時間、10/20 の約 7 時間、10/22 の約 9 時間、10/25 の約 2 時間）。", "",
             "## 円の詰め込み", ""] + table(circles) + ["", "## 球・超球の詰め込み", ""] + table(spheres) + [""]
    allrows = circles + spheres
    new = [r for r in circles if r["code"] not in ("cci", "csq")]
    lines += ["## まとめ", "",
              "- 記録を上回ったサイズは全部で %d 件（探索 %d、研磨のみ %d）。" % (
                  sum(r["beaten"] for r in allrows), sum(r["search"] for r in allrows),
                  sum(r["polish"] for r in allrows)),
              "- 今週から加えた容器（半円・四分円・直角二等辺三角形・長方形 8 種・正八角形）では %d 件（探索 %d）。" % (
                  sum(r["beaten"] for r in new), sum(r["search"] for r in new)),
              "  四分円・三角形・長方形の記録表は 2018 年から更新がなく、記録の大半は 2010 年前後に出たものです",
              "  （記録表の出典欄: 四分円は 2010 年 10〜12 月の一つの出典、三角形は 2009〜2010 年、長方形は 1999〜2010 年）。", ""]
    with open(args.out, "w") as f:
        f.write("\n".join(lines) + "\n")
    print("\n".join(lines[8:8 + len(circles) + 4]))
    print("wrote", args.out)


if __name__ == "__main__":
    main()
