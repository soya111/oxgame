"use client";
import { useState } from "react";
type Board = (string | null)[];
const initialBoard: Board = Array(9).fill(null);
export default function Home() {
  const [board, setBoard] = useState<Board>(initialBoard);
  const [isXNext, setIsXNext] = useState<boolean>(true);
  const handleClick = (index: number): void => {
    if (board[index] || calculateWinner(board)) {
      return;
    }
    const newBoard = [...board];
    newBoard[index] = isXNext ? "X" : "O";
    setBoard(newBoard);
    setIsXNext(!isXNext);
  };

  const renderCell = (index: number) => {
    return <button onClick={() => handleClick(index)}>{board[index]}</button>;
  };

  const winner = calculateWinner(board);
  const status = winner
    ? `Winner: ${winner}`
    : `Next player: ${isXNext ? "X" : "O"}`;
  return (
    <main>
      <div>{status}</div>
      <div className="flex">
        {renderCell(0)}
        {renderCell(1)}
        {renderCell(2)}
      </div>
      <div>
        {renderCell(3)}
        {renderCell(4)}
        {renderCell(5)}
      </div>
      <div>
        {renderCell(6)}
        {renderCell(7)}
        {renderCell(8)}
      </div>
    </main>
  );
}
function calculateWinner(board: Board): string | null {
  const lines = [
    [0, 1, 2],
    [3, 4, 5],
    [6, 7, 8],
    [0, 3, 6],
    [1, 4, 7],
    [2, 5, 8],
    [0, 4, 8],
    [2, 4, 6],
  ];
  for (let i = 0; i < lines.length; i++) {
    const [a, b, c] = lines[i];
    if (board[a] && board[a] === board[b] && board[a] === board[c]) {
      return board[a];
    }
  }
  return null;
}
