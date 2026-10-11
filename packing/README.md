# 正三角形詰め込み（Triangles in Triangles）の記録探索

一辺 1 の正三角形 n 枚を、重ならないように（回転自由）できるだけ小さい正三角形に詰める問題。
記録は [Erich Friedman's Packing Center](https://erich-friedman.github.io/packing/triintri/) にまとまっている。

当面の目標は n=44 の更新。

| 出典 | s |
|---|---|
| Yanis Gomes (2026-10, サイト掲載) | 6.93069+ |
| @ootamato (2026-10-10, [X](https://x.com/ootamato/status/2108885411510804611)) | 6.9303828（`seeds/n44_ootamato.txt`） |

## 表現

各ピースは `x y theta`（重心と回転角）。頂点 k は角度 `theta + 90° + 120°·k`、外接円半径 `1/√3`。
容器は `(0,0), (S,0), (S/2, S·√3/2)`。ファイル形式は `#` コメント行のあと `n S`、続いて n 行の `x y theta`。

## 構成

- `src/tripack.cpp` — 探索本体（C++17、依存なし）
  - 重なりは分離軸定理（SAT）で測り、二乗和をペナルティにする
  - L-BFGS で `S + μ·P` を最小化して容器を押し縮める（μ を 10 → 1e9 と上げて硬くしていく）
  - 角を丸めた三角形（blob）から硬い三角形へ変形させる段階も選べる
  - `polish`: S を二分探索し、指定した隙間（既定 1e-10）を保つ最小の容器まで締める
  - `bh`: basin hopping（容器を少し広げて摂動 → 再圧縮 → Metropolis 判定）
  - `search`: 格子から k²−n 枚抜いた配置やランダム配置から多数回スタート
- `tools/verify.py` — 倍精度 SAT と 60 桁 Decimal の 2 通りで検証。`--standalone` で配布用の検証スクリプトを出力
- `tools/render.py` — SVG/PNG 描画（格子向きのピースは青、傾いたピースは橙）
- `seeds/` — 探索の出発点
- `results/` — 見つかった配置

## 使い方

```bash
g++ -O3 -march=native -std=c++17 -o build/tripack src/tripack.cpp

./build/tripack check  --in seeds/n44_ootamato.txt
./build/tripack polish --in seeds/n44_ootamato.txt --out /tmp/p.txt
./build/tripack bh     --in seeds/n44_ootamato.txt --out results/n44_bh.txt --time 3600 --seed 1
./build/tripack search --n 44 --init lattice --out results/n44_lat.txt --time 3600 --seed 2

python3 tools/verify.py results/n44_bh.txt --standalone verify_n44.py
python3 tools/render.py results/n44_bh.txt n44.svg --png n44.png
```

`gradcheck` で解析勾配を差分と照合できる（`./build/tripack gradcheck --n 20 --r 0.1`）。

## これまでの結果（2026-10-10）

### n=44（目標 6.9303828 未満）

| 系統 | 特徴 | 最良 S | ファイル |
|---|---|---|---|
| F3 | 中ほどを横断する約 5.5° 傾いた帯 + 下段の縦向き菱形 | 6.9303828（@ootamato） | `seeds/n44_ootamato.txt` |
| F3 | 同じ帯構造の別配置（今回の最良） | 6.9304010 | `seeds/n44_band_6.930401.txt` |
| F4 | 同じ角度の帯、菱形が上部 | 6.9304097 | `seeds/n44_f4_6.930410.txt` |
| F3 | 種なしの pool が独力で発見 | 6.9304173 | `seeds/n44_f3_pool_6.930417.txt` |
| F1 | 上 4 段が格子（Gomes 解と同系統） | 6.9305411 | `seeds/n44_f1_6.930541.txt` |
| F2 | 中央の小さな乱れ領域 | 6.9307257 | `results/n44_f2_pool_6.930726.txt` |

約 20 コア時間の探索では 6.9303828 は更新できていない（自前の最良は 6.9304010、差 1.8e-5）。
@ootamato 解からの basin hopping（2 時間・約 75 万試行）でも改善なし。帯を持つ配置は
6.93038〜6.93045 に多数あり、その中で @ootamato 解が最良。

### n=31（サイト記録 5.90751, Yanis Gomes）

pool 探索と basin hopping で 5.9075215（`results/n31_5.9075215.txt`、記録まで 1.15e-5）。
basin hopping 2 本（計約 93 万試行）が同じ値に収束したので、この系統の局所最適とみられる。

### n=45（サイト記録 6.95922, Maurizio Morandi）

20 分の試行で 6.961842。

## 続け方

```bash
# 帯構造の解を種にした交叉探索（数時間単位で回す）
./build/tripack pool --n 44 --pool 12 --init mix --time 14400 --seed 1 --out results/n44_pool.txt \
  --seeds seeds/n44_ootamato.txt,seeds/n44_f3_pool_6.930417.txt,seeds/n44_f4_6.930410.txt

# 種なしの交叉探索（F3 系統はこれで独力で見つかった）
./build/tripack pool --n 44 --pool 10 --init mix --time 14400 --seed 2 --out results/n44_pool2.txt
```

`--seed` を変えたプロセスをコア数だけ並べると効率がよい。

## 円を正多角形に詰める（Packomania）

[Packomania](https://www.packomania.com/) の「circles in a regular polygon」のうち、2020 年 12 月から
更新が止まっている正 7・9・10・11・12・13・14・15・17 角形（N≤48、15 角形は N≤122）が対象。
記録表は `circles/records/` に保存してある（容器は外接円半径 rc、底辺が水平、9・10 角形だけ rc=1/2）。

- `src/circpack.cpp` — 円用ソルバ。単位円を詰める正多角形の外接円半径 R を最小化（Packomania の半径は r = rc/R）
  - `sweep`: N の範囲を巡回し、N−1 の最良解に円を 1 個足したもの・N+1 から 1 個抜いたもの・ランダム配置を
    初期解に basin hopping。改善が出たら N±1 を再訪問する
- `tools/campaign.py` — 複数の sweep を時間を区切って回し続ける司令塔。記録との比較を `summary.log` に残す
- `tools/circ.py` — 50 桁精度の検証（`verify`）、提出用 `.pck` の出力（`export`）、記録との比較（`status`）、描画（`render`）
- `tools/exact_verify.py` — 浮動小数点を使わない厳密な検証（下記）
- `tools/circ_submit.py` / `tools/circ_results.py` — 改善の書き出し・同期。どちらも厳密検証を通ったものだけを扱う

### 厳密な検証

`.pck` の半径と座標は有限小数なので、正確な有理数として読む。
- 円どうし: `dx² + dy² ≥ (2r)²` を整数演算で判定（丸めなし）
- 円と辺: 辺の法線と内接円半径は cos/sin(π/k の倍数) で無理数なので、真値を必ず含む有理区間で挟む。
  π はマチンの公式（交代級数の部分和が真値を挟む）、cos/sin はテイラー多項式とラグランジュ剰余 `|t|^n/n!`
  で上下から押さえ、端点は外向きにだけ丸める。判定は区間の不利な側の端で行う

VALID はそのまま「全円が閉じた多角形内にあり、互いに重ならない」ことの証明になる。なお Packomania に
公開されている記録の座標（小数 12 桁）をこの方法で調べると、丸めの分だけ 1e-12 程度の重なり・はみ出しがあり、
厳密には成立しないものが多い（表の値は 1e-12 程度の精度の値）。

```bash
g++ -O3 -march=native -std=c++17 -o build/circpack src/circpack.cpp
python3 tools/campaign.py --root /tmp/camp --workers 4 --slice 900 --hours 10 --ext 100
python3 tools/circ.py status --code chp --dir /tmp/camp/chp
python3 tools/circ.py export /tmp/camp/chp/40.txt --name "Your Name" --out chp40.pck
```
