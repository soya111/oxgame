"use client";
import { useState } from "react";

type Player = "O" | "X";
type Board = (Player | null)[];
type Mode = "vanish" | "classic";

// 消える○×ゲームで各プレイヤーが盤面に置ける最大数
const MAX_MARKS = 3;

const LINES = [
  [0, 1, 2],
  [3, 4, 5],
  [6, 7, 8],
  [0, 3, 6],
  [1, 4, 7],
  [2, 5, 8],
  [0, 4, 8],
  [2, 4, 6],
];

export default function Home() {
  const [mode, setMode] = useState<Mode>("vanish");
  // 各プレイヤーが置いたマスのインデックスを古い順に保持する
  const [moves, setMoves] = useState<Record<Player, number[]>>({ O: [], X: [] });
  const [current, setCurrent] = useState<Player>("O");
  const [score, setScore] = useState({ O: 0, X: 0, draw: 0 });

  const board = buildBoard(moves);
  const result = calculateWinner(board);
  const isDraw = !result && mode === "classic" && board.every(Boolean);
  const isOver = Boolean(result) || isDraw;

  // 次に置くと消えるマス（次の手番のプレイヤーの一番古いマーク）
  const vanishing =
    mode === "vanish" && !isOver && moves[current].length >= MAX_MARKS
      ? moves[current][0]
      : null;

  const handleClick = (index: number) => {
    if (board[index] || isOver) return;
    let mine = [...moves[current], index];
    if (mode === "vanish" && mine.length > MAX_MARKS) mine = mine.slice(1);
    const next = { ...moves, [current]: mine };
    setMoves(next);

    const nextBoard = buildBoard(next);
    const winner = calculateWinner(nextBoard);
    if (winner) {
      setScore((s) => ({ ...s, [winner.player]: s[winner.player] + 1 }));
    } else if (mode === "classic" && nextBoard.every(Boolean)) {
      setScore((s) => ({ ...s, draw: s.draw + 1 }));
    } else {
      setCurrent(current === "O" ? "X" : "O");
    }
  };

  const reset = () => {
    setMoves({ O: [], X: [] });
    setCurrent("O");
  };

  const changeMode = (m: Mode) => {
    setMode(m);
    setMoves({ O: [], X: [] });
    setCurrent("O");
    setScore({ O: 0, X: 0, draw: 0 });
  };

  const status = result
    ? `${result.player} の勝ち！`
    : isDraw
      ? "引き分け"
      : `${current} の番です`;

  return (
    <main className="min-h-screen flex flex-col items-center justify-center gap-6 p-4">
      <h1 className="text-3xl font-bold">
        {mode === "vanish" ? "消える○×ゲーム" : "○×ゲーム"}
      </h1>

      <div className="flex rounded-full bg-black/10 dark:bg-white/10 p-1 text-sm">
        {(["vanish", "classic"] as Mode[]).map((m) => (
          <button
            key={m}
            onClick={() => changeMode(m)}
            className={`px-4 py-1.5 rounded-full transition ${
              mode === m ? "bg-white text-black shadow" : "opacity-70"
            }`}
          >
            {m === "vanish" ? "消えるモード" : "通常モード"}
          </button>
        ))}
      </div>

      <div className="flex gap-6 text-lg">
        <span className="text-rose-500 font-semibold">O: {score.O}</span>
        <span className="text-sky-500 font-semibold">X: {score.X}</span>
        {mode === "classic" && <span className="opacity-70">引分: {score.draw}</span>}
      </div>

      <div
        className={`text-xl font-semibold ${
          result ? (result.player === "O" ? "text-rose-500" : "text-sky-500") : ""
        }`}
      >
        {status}
      </div>

      <div className="grid grid-cols-3 gap-2 w-[min(90vw,360px)] aspect-square">
        {board.map((cell, i) => {
          const winning = result?.line.includes(i);
          const fading = vanishing === i;
          return (
            <button
              key={i}
              onClick={() => handleClick(i)}
              aria-label={`マス ${i + 1}`}
              className={`rounded-xl text-6xl font-bold flex items-center justify-center transition
                bg-white/70 dark:bg-white/10 shadow-sm
                ${!cell && !isOver ? "hover:bg-white dark:hover:bg-white/20" : ""}
                ${winning ? "ring-4 ring-yellow-400" : ""}
                ${cell === "O" ? "text-rose-500" : "text-sky-500"}
                ${fading ? "opacity-30 animate-pulse" : ""}`}
            >
              {cell === "O" ? "○" : cell === "X" ? "×" : ""}
            </button>
          );
        })}
      </div>

      {mode === "vanish" && (
        <p className="text-sm opacity-70 text-center max-w-xs">
          置けるのは1人{MAX_MARKS}つまで。4つ目を置くと一番古いマークが消えます（薄く点滅しているマーク）。
        </p>
      )}

      <button
        onClick={reset}
        className="px-6 py-2 rounded-full bg-black text-white dark:bg-white dark:text-black font-semibold"
      >
        {isOver ? "もう一度" : "リセット"}
      </button>
    </main>
  );
}

function buildBoard(moves: Record<Player, number[]>): Board {
  const board: Board = Array(9).fill(null);
  moves.O.forEach((i) => (board[i] = "O"));
  moves.X.forEach((i) => (board[i] = "X"));
  return board;
}

function calculateWinner(board: Board): { player: Player; line: number[] } | null {
  for (const line of LINES) {
    const [a, b, c] = line;
    if (board[a] && board[a] === board[b] && board[a] === board[c]) {
      return { player: board[a] as Player, line };
    }
  }
  return null;
}
