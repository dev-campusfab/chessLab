#pragma once

// Page web servie sur "/" (extraite telle quelle du firmware d'origine).
// Elle se connecte au WebSocket du port 81 et redessine le plateau à chaque message.
static const char INDEX_HTML[] = R"rawliteral(
<!DOCTYPE html>
<html lang="fr">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>ChessLab</title>
<style>
  body {
    font-family: sans-serif;
    background: #1e1e1e;
    color: #eee;
    display: flex;
    flex-direction: column;
    align-items: center;
    padding: 24px;
  }
  h1 { margin-bottom: 4px; }
  #status { margin-bottom: 16px; font-size: 1.1em; min-height: 1.4em; text-align: center; }
  #board {
    display: grid;
    grid-template-columns: 20px repeat(8, 44px);
    gap: 3px;
  }
  .cell {
    width: 44px; height: 44px; background: #3a3a3a; border-radius: 4px;
    display: flex; align-items: center; justify-content: center;
    font-size: 30px; line-height: 1;
  }
  .cell.piece { background: #2ecc40; }
  .cell.legal { background: #00e0e0; }
  .cell.capture { background: #ff8c00; }
  .cell.bad { background: #ff3b30; }
  .cell.missing { background: #ff3b30; }
  .cell.check { background: #9400d3; }
  #board.impossible .cell { background: #ff3b30 !important; }
  .cell.white-piece { color: #f5f5f5; text-shadow: 0 0 2px #000, 0 0 4px #000; }
  .cell.black-piece { color: #101010; text-shadow: 0 0 2px #fff, 0 0 4px #fff; }
  .label { display: flex; align-items: center; justify-content: center; font-size: 0.8em; color: #999; }
</style>
</head>
<body>
<h1>ChessLab</h1>
<div id="status">Connexion...</div>
<div id="board"></div>
<script>
const files = "ABCDEFGH";
const ranks = "87654321";
const boardEl = document.getElementById("board");
const statusEl = document.getElementById("status");

const fileLabels = [];
boardEl.appendChild(document.createElement("div"));
for (const f of files) {
  const l = document.createElement("div");
  l.className = "label";
  l.textContent = f;
  boardEl.appendChild(l);
  fileLabels.push(l);
}

const cells = [];
const rankLabels = [];
for (let r = 0; r < 8; r++) {
  const rowLabel = document.createElement("div");
  rowLabel.className = "label";
  rowLabel.textContent = ranks[r];
  boardEl.appendChild(rowLabel);
  rankLabels.push(rowLabel);

  for (let c = 0; c < 8; c++) {
    const cell = document.createElement("div");
    cell.className = "cell";
    boardEl.appendChild(cell);
    cells.push(cell);
  }
}

const codeToClass = { E: "", P: "piece", L: "legal", C: "capture", B: "bad", M: "missing", H: "check" };
const glyphs = {
  K: "♔", Q: "♕", R: "♖", B: "♗", N: "♘", P: "♙",
  k: "♚", q: "♛", r: "♜", b: "♝", n: "♞", p: "♟"
};

let flipped = false;

function setFlipped(next) {
  if (next === flipped) return;
  flipped = next;
  const orderedFiles = flipped ? [...files].reverse() : [...files];
  const orderedRanks = flipped ? [...ranks].reverse() : [...ranks];
  fileLabels.forEach((el, i) => { el.textContent = orderedFiles[i]; });
  rankLabels.forEach((el, i) => { el.textContent = orderedRanks[i]; });
}

function connect() {
  const ws = new WebSocket("ws://" + location.hostname + ":81/");
  ws.onopen = () => { statusEl.textContent = "Connecte"; };
  ws.onclose = () => { statusEl.textContent = "Deconnecte, reconnexion..."; setTimeout(connect, 2000); };
  ws.onerror = () => ws.close();
  ws.onmessage = (event) => {
    const data = JSON.parse(event.data);
    statusEl.textContent = data.message || "";
    boardEl.classList.toggle("impossible", !!data.impossible);
    setFlipped(data.mode === "playing" && data.turn === "black");

    for (let i = 0; i < data.status.length; i++) {
      const idx = flipped ? data.status.length - 1 - i : i;
      const pieceChar = data.pieces[idx];
      let className = "cell " + (codeToClass[data.status[idx]] || "");
      if (pieceChar && pieceChar !== " ") {
        className += pieceChar === pieceChar.toUpperCase() ? " white-piece" : " black-piece";
      }
      cells[i].className = className;
      cells[i].textContent = glyphs[pieceChar] || "";
    }
  };
}
connect();
</script>
</body>
</html>
)rawliteral";
