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
| F4 | 同じ角度の帯、菱形が上部 | 6.9304097 | `seeds/n44_f4_6.930410.txt` |
| F3 | 種なしの pool が独力で発見 | 6.9304173 | `seeds/n44_f3_pool_6.930417.txt` |
| F1 | 上 4 段が格子（Gomes 解と同系統） | 6.9305411 | `seeds/n44_f1_6.930541.txt` |
| F2 | 中央の小さな乱れ領域 | 6.9307257 | `results/n44_f2_pool_6.930726.txt` |

約 10 コア時間の探索では 6.9303828 は更新できていない。@ootamato 解からの basin hopping
（2 時間・約 75 万試行）でも改善なし。帯を持つ配置は 6.93038〜6.93045 に多数あり、その中で最良。

### n=31（サイト記録 5.90751, Yanis Gomes）

pool 探索で 5.9075226（`seeds/n31_5.9075226.txt`、記録まで 1.3e-5）。

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
