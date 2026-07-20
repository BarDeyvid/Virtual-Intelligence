// Benchmark do alyssad (v2/F1+): manda turnos em série e mede say→response.
// Uso: node tools/alyssad_bench.js [porta] -- "turno 1" "turno 2" ...
// Cada turno espera o evento `response` antes do próximo (nada de `busy`).
const net = require("net");

const args = process.argv.slice(2);
const port = args[0] && args[0] !== "--" ? parseInt(args[0], 10) : 8377;
const sep = args.indexOf("--");
const turns = sep >= 0 ? args.slice(sep + 1) : ["oi"];

const sock = net.createConnection({ host: "127.0.0.1", port }, () => {
  console.log(`[bench] conectado em 127.0.0.1:${port}`);
  send({ type: "req", id: 0, method: "status", params: {} });
});

let buf = "";
let idx = -1;          // turno atual (-1 = status inicial)
let t0 = 0;
let ttft = null;
const results = [];

function send(obj) { t0 = Date.now(); ttft = null; sock.write(JSON.stringify(obj) + "\n"); }

function nextTurn() {
  idx++;
  if (idx >= turns.length) return finish();
  // Turno especial "@consolidate": dispara o ciclo de consolidação (F3)
  // em vez de um say — termina no evento `consolidation`.
  if (turns[idx] === "@consolidate") {
    console.log(`\n→ [${idx + 1}/${turns.length}] @consolidate`);
    send({ type: "req", id: 100 + idx, method: "consolidate", params: {} });
    return;
  }
  console.log(`\n→ [${idx + 1}/${turns.length}] "${turns[idx]}"`);
  send({ type: "req", id: 100 + idx, method: "say", params: { text: turns[idx] } });
}

function finish() {
  console.log("\n=== RESULTADOS ===");
  for (const r of results) {
    console.log(`${String(r.ms).padStart(6)}ms total | ttft ${String(r.ttft ?? "—").padStart(5)}ms | "${r.turn}" → "${r.text.slice(0, 80)}"`);
  }
  sock.end();
  process.exit(0);
}

sock.on("data", (chunk) => {
  buf += chunk.toString("utf8");
  let nl;
  while ((nl = buf.indexOf("\n")) >= 0) {
    const line = buf.slice(0, nl).trim();
    buf = buf.slice(nl + 1);
    if (!line) continue;
    let msg;
    try { msg = JSON.parse(line); } catch { continue; }

    if (msg.type === "res" && msg.id === 0) {
      console.log("[bench] status:", JSON.stringify(msg.data).slice(0, 200));
      nextTurn();
    } else if (msg.type === "res" && msg.id === 100 + idx) {
      if (!msg.ok) { console.log(`← ERRO: ${msg.error}`); results.push({ turn: turns[idx], ms: -1, ttft: null, text: `ERRO ${msg.error}` }); nextTurn(); }
      // ok:accepted → espera o evento `response`
    } else if (msg.type === "event" && msg.event === "token" && ttft === null) {
      ttft = Date.now() - t0;
    } else if (msg.type === "event" && msg.event === "response") {
      const ms = Date.now() - t0;
      console.log(`← (${ms}ms, ttft ${ttft ?? "—"}ms) ${String(msg.data.text).slice(0, 120)}`);
      results.push({ turn: turns[idx], ms, ttft, text: String(msg.data.text) });
      nextTurn();
    } else if (msg.type === "event" && msg.event === "consolidation") {
      const ms = Date.now() - t0;
      const stats = JSON.stringify(msg.data);
      console.log(`← consolidação (${ms}ms): ${stats}`);
      results.push({ turn: "@consolidate", ms, ttft: null, text: stats });
      nextTurn();
    }
  }
});

sock.on("error", (e) => { console.error("[bench] erro:", e.message); process.exit(1); });
// Watchdog com unref: não segura o processo vivo depois do finish().
const wd = setTimeout(() => { console.error("[bench] timeout global (300s)"); process.exit(2); }, 300000);
wd.unref();
