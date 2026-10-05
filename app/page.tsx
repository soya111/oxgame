"use client";
import { useEffect, useMemo, useState } from "react";

// 五十音表。列は右から あ行→ん の順に並べ、各列は上から あ段→お段。
// null は表の空欄（マスが存在しない）。
const COLUMNS: (string | null)[][] = [
  ["あ", "い", "う", "え", "お"],
  ["か", "き", "く", "け", "こ"],
  ["さ", "し", "す", "せ", "そ"],
  ["た", "ち", "つ", "て", "と"],
  ["な", "に", "ぬ", "ね", "の"],
  ["は", "ひ", "ふ", "へ", "ほ"],
  ["ま", "み", "む", "め", "も"],
  ["や", null, "ゆ", null, "よ"],
  ["ら", "り", "る", "れ", "ろ"],
  ["わ", null, null, null, "を"],
  ["ん", null, null, null, null],
];
const ROWS = 5;
const COLS = COLUMNS.length;

// 画面上の (row, col) で引く。col 0 が左端なので右から並ぶよう反転する。
const GRID: (string | null)[][] = Array.from({ length: ROWS }, (_, r) =>
  Array.from({ length: COLS }, (_, c) => COLUMNS[COLS - 1 - c][r]),
);

// 爆弾になる文字＝答えの言葉。清音のみ・同じ文字を含まない言葉に限る。
const WORDS: { word: string; hint: string }[] = [
  { word: "さくら", hint: "花" },
  { word: "すみれ", hint: "花" },
  { word: "ひまわり", hint: "花" },
  { word: "すいか", hint: "果物" },
  { word: "みかん", hint: "果物" },
  { word: "めろん", hint: "果物" },
  { word: "かえる", hint: "生き物" },
  { word: "きつね", hint: "生き物" },
  { word: "たぬき", hint: "生き物" },
  { word: "らいおん", hint: "生き物" },
  { word: "ふくろう", hint: "鳥" },
  { word: "からす", hint: "鳥" },
  { word: "かもめ", hint: "鳥" },
  { word: "にわとり", hint: "鳥" },
  { word: "つくえ", hint: "身の回りの物" },
  { word: "とけい", hint: "身の回りの物" },
  { word: "はさみ", hint: "身の回りの物" },
  { word: "まくら", hint: "身の回りの物" },
  { word: "そうめん", hint: "食べ物" },
  { word: "たこやき", hint: "食べ物" },
  { word: "さしみ", hint: "食べ物" },
  { word: "おきなわ", hint: "都道府県" },
  { word: "あおもり", hint: "都道府県" },
  { word: "いわて", hint: "都道府県" },
  { word: "とやま", hint: "都道府県" },
  { word: "ふくい", hint: "都道府県" },
  { word: "くまもと", hint: "都道府県" },
  { word: "しまね", hint: "都道府県" },
  { word: "ひろしま", hint: "都道府県" },
  { word: "やまなし", hint: "都道府県" },
  { word: "あきた", hint: "都道府県" },
  { word: "たいふう", hint: "天気" },
  { word: "かみなり", hint: "天気" },
  { word: "ひこうき", hint: "乗り物" },
  { word: "くるま", hint: "乗り物" },
  { word: "ちかてつ", hint: "乗り物" },
  { word: "すもう", hint: "スポーツ" },
  { word: "てにす", hint: "スポーツ" },
];

type CellState = "hidden" | "open" | "flag";
type Phase = "playing" | "clear" | "giveup";

const key = (r: number, c: number) => r * COLS + c;

function neighbors(r: number, c: number): [number, number][] {
  const result: [number, number][] = [];
  for (let dr = -1; dr <= 1; dr++) {
    for (let dc = -1; dc <= 1; dc++) {
      if (!dr && !dc) continue;
      const nr = r + dr;
      const nc = c + dc;
      if (nr >= 0 && nr < ROWS && nc >= 0 && nc < COLS && GRID[nr][nc]) {
        result.push([nr, nc]);
      }
    }
  }
  return result;
}

// カタカナをひらがなに揃え、空白を除く
function normalize(s: string): string {
  return s
    .replace(/[ァ-ヶ]/g, (ch) => String.fromCharCode(ch.charCodeAt(0) - 0x60))
    .replace(/\s/g, "");
}

export default function Home() {
  // 乱数による水和ずれを避けるため、出題はマウント後に決める
  const [puzzle, setPuzzle] = useState<(typeof WORDS)[number] | null>(null);
  const [cells, setCells] = useState<Record<number, CellState>>({});
  const [phase, setPhase] = useState<Phase>("playing");
  const [flagMode, setFlagMode] = useState(false);
  const [misses, setMisses] = useState(0);
  const [wrongAnswers, setWrongAnswers] = useState<string[]>([]);
  const [answer, setAnswer] = useState("");

  const newGame = () => {
    setPuzzle((prev) => {
      const pool = WORDS.filter((w) => w.word !== prev?.word);
      return pool[Math.floor(Math.random() * pool.length)];
    });
    setCells({});
    setPhase("playing");
    setFlagMode(false);
    setMisses(0);
    setWrongAnswers([]);
    setAnswer("");
  };

  useEffect(newGame, []);

  const mines = useMemo(() => new Set(puzzle ? Array.from(puzzle.word) : []), [puzzle]);
  const isMine = (r: number, c: number) => mines.has(GRID[r][c]!);
  const countAround = (r: number, c: number) =>
    neighbors(r, c).filter(([nr, nc]) => isMine(nr, nc)).length;

  const openedCount = Object.entries(cells).filter(
    ([k, s]) => s === "open" && !mines.has(GRID[Math.floor(+k / COLS)][+k % COLS]!),
  ).length;
  const flagged = GRID.flatMap((row, r) =>
    row.flatMap((ch, c) => (ch && cells[key(r, c)] === "flag" ? [ch] : [])),
  );
  const exploded = GRID.flatMap((row, r) =>
    row.flatMap((ch, c) => (ch && cells[key(r, c)] === "open" && isMine(r, c) ? [ch] : [])),
  );

  // 開けたマス数がスコアになるので、連鎖して開けずに1マスずつ開ける
  const open = (r: number, c: number) => {
    if (isMine(r, c)) setMisses((m) => m + 1);
    setCells({ ...cells, [key(r, c)]: "open" });
  };

  const handleCell = (r: number, c: number, toggleFlag: boolean) => {
    if (phase !== "playing") return;
    const k = key(r, c);
    const state = cells[k] ?? "hidden";
    if (state === "open") return;
    if (toggleFlag) {
      setCells({ ...cells, [k]: state === "flag" ? "hidden" : "flag" });
    } else if (state === "hidden") {
      open(r, c);
    }
  };

  const submit = (e: React.FormEvent) => {
    e.preventDefault();
    if (!puzzle || phase !== "playing") return;
    const guess = normalize(answer);
    if (!guess) return;
    if (guess === puzzle.word) {
      setPhase("clear");
    } else {
      setWrongAnswers((w) => [...w, guess]);
    }
    setAnswer("");
  };

  const finished = phase !== "playing";

  return (
    <main className="min-h-screen flex flex-col items-center gap-5 px-4 py-8">
      <header className="text-center">
        <h1 className="text-2xl sm:text-3xl font-bold">五十音表マインスイーパ</h1>
        <p className="mt-2 text-sm opacity-70 max-w-md">
          五十音表のどこかに爆弾が隠れています。爆弾の文字を並べ替えると言葉になります。
          開けたマスの数字（周囲8マスの爆弾の数）を手がかりに、なるべく少ないマスで言葉を当てよう！
        </p>
      </header>

      {puzzle && (
        <div className="flex flex-wrap justify-center gap-x-5 gap-y-1 text-sm">
          <span>
            ヒント：<b>{puzzle.hint}</b>
          </span>
          <span>
            文字数：<b>{puzzle.word.length}</b>（爆弾 {puzzle.word.length} 個）
          </span>
          <span>
            開けたマス：<b>{openedCount}</b>
          </span>
          <span className={misses ? "text-red-500" : ""}>
            爆発：<b>{misses}</b>
          </span>
        </div>
      )}

      <div className="w-full max-w-2xl overflow-x-auto">
        <div
          className="grid gap-1 mx-auto"
          style={{ gridTemplateColumns: `repeat(${COLS}, minmax(0, 1fr))`, minWidth: 330 }}
        >
          {GRID.map((row, r) =>
            row.map((ch, c) => {
              if (!ch) return <div key={key(r, c)} />;
              const state = cells[key(r, c)] ?? "hidden";
              const mine = isMine(r, c);
              const showMine = (state === "open" && mine) || (finished && mine);
              const n = state === "open" && !mine ? countAround(r, c) : 0;
              return (
                <button
                  key={key(r, c)}
                  onClick={() => handleCell(r, c, flagMode)}
                  onContextMenu={(e) => {
                    e.preventDefault();
                    handleCell(r, c, true);
                  }}
                  aria-label={ch}
                  className={`relative aspect-square rounded-md select-none flex flex-col items-center justify-center leading-none transition
                    ${
                      showMine
                        ? state === "open" && !finished
                          ? "bg-red-500 text-white"
                          : "bg-amber-400 text-black"
                        : state === "open"
                          ? "bg-white/60 dark:bg-white/5"
                          : "bg-slate-600 text-white hover:bg-slate-500 dark:bg-slate-700 dark:hover:bg-slate-600"
                    }`}
                >
                  <span
                    className={`text-[11px] sm:text-sm ${
                      state === "open" && !mine ? "opacity-50" : ""
                    }`}
                  >
                    {ch}
                  </span>
                  <span className="text-sm sm:text-lg font-bold h-5 sm:h-6 flex items-center">
                    {showMine ? "💣" : state === "flag" ? "🚩" : n > 0 ? <Num n={n} /> : ""}
                  </span>
                </button>
              );
            }),
          )}
        </div>
      </div>

      {!finished && (
        <button
          onClick={() => setFlagMode((f) => !f)}
          className={`px-4 py-1.5 rounded-full text-sm border transition ${
            flagMode ? "bg-red-500 text-white border-red-500" : "border-current opacity-80"
          }`}
        >
          {flagMode ? "🚩 旗モード（タップで旗）" : "⛏ 開けるモード（右クリックで旗）"}
        </button>
      )}

      {(flagged.length > 0 || exploded.length > 0) && !finished && (
        <div className="text-sm flex flex-wrap gap-2 justify-center items-center">
          <span className="opacity-70">爆弾候補：</span>
          {[...exploded, ...flagged].map((ch) => (
            <span
              key={ch}
              className="px-2 py-0.5 rounded bg-amber-400/80 text-black font-semibold"
            >
              {ch}
            </span>
          ))}
        </div>
      )}

      {phase === "playing" ? (
        <form onSubmit={submit} className="flex gap-2 w-full max-w-sm">
          <input
            value={answer}
            onChange={(e) => setAnswer(e.target.value)}
            placeholder="答えをひらがなで入力"
            className="flex-1 min-w-0 px-3 py-2 rounded-lg border border-slate-300 bg-white text-black"
          />
          <button className="px-4 py-2 rounded-lg bg-black text-white dark:bg-white dark:text-black font-semibold">
            解答
          </button>
        </form>
      ) : (
        puzzle && (
          <div className="text-center">
            <p
              className={`text-2xl font-bold ${
                phase === "clear" ? "text-emerald-500" : "text-red-500"
              }`}
            >
              {phase === "clear" ? "正解！" : "答え"}：{puzzle.word}
            </p>
            {phase === "clear" && (
              <p className="text-sm mt-1 opacity-80">
                開けたマス {openedCount} ／ 爆発 {misses} ／ 誤答 {wrongAnswers.length}
              </p>
            )}
          </div>
        )
      )}

      {wrongAnswers.length > 0 && phase === "playing" && (
        <p className="text-sm text-red-500">不正解：{wrongAnswers.join("、")}</p>
      )}

      <div className="flex gap-3">
        {phase === "playing" && (
          <button
            onClick={() => setPhase("giveup")}
            className="px-4 py-2 rounded-full text-sm border border-current opacity-70"
          >
            あきらめる
          </button>
        )}
        <button
          onClick={newGame}
          className="px-5 py-2 rounded-full text-sm bg-black text-white dark:bg-white dark:text-black font-semibold"
        >
          次の問題
        </button>
      </div>
    </main>
  );
}

const NUM_COLORS = ["", "text-blue-600", "text-green-600", "text-red-600", "text-purple-700"];

function Num({ n }: { n: number }) {
  return <span className={NUM_COLORS[n] ?? "text-rose-800"}>{n}</span>;
}
