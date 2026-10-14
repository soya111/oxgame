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
記録表は `circles/records/` に保存してある（容器は外接円半径 1、底辺が水平）。

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

## 有名問題への挑戦（2026-10-12〜）

### 正方形を正方形に詰める（Squares in Squares）

一辺 1 の正方形 n 個を入れる最小の正方形の一辺 s。現在の記録表は The Squares Project の登録簿
（https://github.com/jlevy/squares、2026 年 8 月から、n = 1..324 の最良既知配置を厳密な座標つきで登録）。
Erich Friedman のページを David Ellsworth が引き継いだ表（https://kingbird.myphotos.cc/packing/squares_in_squares.html）は
2026-09-24 で更新が止まっている。

**訂正（2026-10-12）**: 最初は Ellsworth のページだけを記録とみなして比較し、n = 180, 236, 237, 241, 263, 305 の
新配置などを「記録更新」と報告したが、登録簿にはそれらすべてより良い配置が既に登録されていた。
それらは `squares/superseded/` に移し、以後は登録簿を基準にしている（`squares/RESULTS.md`）。

- `squares/records.txt` — 登録簿の s（`tools/sq_register.py` で 2026-10-12 の状態から取り込み、n = 1..324）
- `squares/records_ellsworth_2026-09-24.txt` — Ellsworth のページの s（図の SVG を Ellsworth の
  `parse_svg_packing.py`（https://github.com/Davidebyzero/packing_squares_in_squares__tools）で座標に変換）
- `src/sqpack.cpp` — ソルバ。正方形 i, j の重なりは分離軸 4 本の上での射影の重なり
  `1/2 + (|cos Δ| + |sin Δ|)/2 − max|d·u|`（Δ は角度差）で、その二乗をペナルティにする。容器は
  `|x| + (|cos t| + |sin t|)/2 ≤ s/2`。圧縮（s + μP の最小化）→ 二分法で s を詰める研磨 → basin hopping
  （ゆすり・局所変形・穴への移動・回転・傾きの解消/付与）。`search --in` で既存解から、`--n` でランダム初期配置から
- `tools/sq_campaign.py` — n ごとに時間を区切って回す司令塔（改善の出た n に時間を寄せる）
- `tools/sq_verify.py` — 厳密検証。各正方形の回転を有理数 u（tan(t/2) の 1e-30 以内）で
  `cos = (1−u²)/(1+u²)`, `sin = 2u/(1+u²)` と表すと、ちょうど一辺 1 の正方形で頂点も有理数になる。
  壁 `|x| + (|cos|+|sin|)/2 ≤ s/2` と、中心間が √2 未満の全組の分離軸判定 `max|d·u| ≥ 1/2 + (|cos Δ|+|sin Δ|)/2`
  （cos Δ, sin Δ も有理数）を有理数演算だけで判定する
- `tools/sq_export.py` — Ellsworth のツール（`check_packing.py`）の形式で書き出す
- `tools/sq_register.py` — 登録簿の配置（4 種類の表現）を sqpack の形式と `records.txt` に変換する
- `tools/sq_results.py` — 登録簿を下回った解を厳密検証して `squares/best/` に入れ、`squares/RESULTS.md` を書く

結果の区別: 登録簿の配置をそのまま局所最適化（研磨）しただけで記録を下回るものを「refinement」、
探索で研磨後の値よりさらに良い配置が見つかったものを「improvement」と呼ぶ。

```bash
g++ -O3 -march=native -std=c++17 -o build/sqpack src/sqpack.cpp
./build/sqpack search --n 17 --out /tmp/sq17.txt --time 120          # ランダム初期配置から
./build/sqpack search --in rec/301.txt --out /tmp/sq301.txt --time 600  # 記録の配置から
python3 tools/sq_campaign.py --root /tmp/sqcamp --records squares/records.txt --workers 3 --slice 300 --hours 10
python3 tools/sq_verify.py /tmp/sqcamp/best/301.txt
```

### 円を円・正方形に詰める（Packomania cci / csq）

`circpack` は `--k 0` で円の容器を扱う。正方形は正 4 角形で、Packomania の csq は一辺 1（外接円半径 √2/2）
なので `--rc 0.7071067811865476` を渡す。記録の座標は Packomania の `cci_coords.tar.gz` / `csq_coords.tar.gz` から
`tools/pm_seeds.py` で種に変換する。`tools/exact_verify.py` は K=0（単位円）と K=4（単位正方形 [−1/2, 1/2]²）を
有理数演算だけで判定する。司令塔は `tools/campaign.py --jobs "cci:401-500,..."` で範囲を指定し、
`--only-dir DIR`（DIR/cci.txt に訪れる N を列挙）で対象を絞れる。

結果（2026-10-13、4 コアで約 36 時間、`circles/RESULTS.md`、全件 `exact_verify.py` で証明済み）:
- cci（記録表は 2024-12-25 以降不変）で 333 サイズが記録を上回った。うち 111 件は探索で見つけた新しい配置
  （改善幅の中央値 5e-6、最大 N=1248 の +3.0e-5）、222 件は公開配置の研磨だけで +1e-7 以上上回るもの。
- 上回った記録の 330 件は Packomania 自身のプログラム（表の参照 [31]）によるもの。2024 年の研究グループの記録を
  上回ったのは N=411, 572, 704 の 3 件だけで、N≤400 は 1 件も上回れていない。
- csq（N≤400）は改善なし。正方形を正方形には上記の通り登録簿を一つも超えられていない。

### 球を球・立方体に、超球を超球に詰める（Packomania ssp / scu / hsp4〜hsp6）

- `src/spack.cpp` — d 次元のソルバー（単位球を半径 R の球、または [−R, R]^d の立方体に詰める）。円の
  `circpack` と同じ罰則・圧縮・二分法・basin hopping・N±1 の種に加えて、D_d 格子（3 次元では面心立方格子）を
  ランダムに回転・平行移動して容器で切り抜いた初期配置を使う
- `tools/pm_seeds_nd.py` — Packomania の座標を種に変換する。公開ファイルの欠陥：ssp は最後の球の座標が空欄
  （spack が最も広い隙間に補う）、hsp5 / hsp6 は 4 列しか座標がない（種にできない）
- `tools/sphere_results.py` — `verify`（書き出した値を有理数だけで判定：球 x·x ≤ (1−r)²、立方体 |x_k| ≤ 1/2−r、
  全組 |x_i−x_j|² ≥ (2r)²）と `sync`（記録を上回った解を `spheres/best/` に入れ `spheres/RESULTS.md` を書く）
- `tools/campaign.py` は `--jobs "hsp4:1-100,ssp:101-200,..."` でこれらの表も回せる。`--until` で終了時刻を指定
- `tools/campaign.py --jobs-file FILE` はジョブ一覧をスライスごとに読み直す（走らせたまま追加・削除・重み変更できる）
- `tools/target_sizes.py` — 記録の弱そうなサイズを選んで `--only-dir` 用の一覧を書く。記録の密度 n·r^d を
  前後 8 サイズの中央値と比べ、低い順に一定割合を残す。cci の N=401〜1300 では密度が低い 5 分の 1 のサイズは
  180 件中 38 件が探索で上回れたのに対し、高い 5 分の 1 は 11 件だった
