#!/usr/bin/env node
'use strict';

// Scorecard determinístico das runs de Minecraft da Gaia (gameplayModel).
//
// Lê logs/minecraft_session.jsonl (formato do GameplayLog, incluindo os
// marcadores {"repeated": N} do colapso de linhas idênticas e o campo "tick"
// adicionado em 2026-07-12) e imprime, por sessão: progresso de goals com
// tempo por goal, taxa de sucesso por verbo, top motivos de falha, loops de
// estagnação (mesma ação+alvo falhando seguidas), latência de decisão
// (prompt→signal), tempo de execução das ações, reflexos, dano/mortes e chat.
//
// Deliberadamente SEM LLM: toda métrica aqui é contável/cronometrável.
// (Plano de telemetria discutido em 2026-07-12 — o "juiz" LLM foi cortado
// porque as métricas são quantitativas e um script é reprodutível.)
//
// Uso:
//   node tools/analyze_gameplay.js [caminho/para/minecraft_session.jsonl]

const fs = require('fs');
const path = require('path');

const file = process.argv[2] ||
  path.join(__dirname, '..', 'build', 'Release', 'logs', 'minecraft_session.jsonl');

if (!fs.existsSync(file)) {
  console.error(`arquivo não encontrado: ${file}`);
  process.exit(1);
}

// ---------------------------------------------------------------- parsing --

const entries = [];
for (const line of fs.readFileSync(file, 'utf8').split('\n')) {
  const trimmed = line.trim();
  if (!trimmed) continue;
  try {
    entries.push(JSON.parse(trimmed));
  } catch {
    // linha corrompida (crash no meio do write) — ignorar é o certo aqui
  }
}

// "repeated": N = N ocorrências ALÉM da linha original já escrita antes dela.
const occurrences = (e) => 1 + (e.repeated || 0);
const ms = (ts) => (ts ? new Date(ts).getTime() : NaN);

// Sessões: cada session_start abre uma nova. Linhas antes do primeiro
// session_start (corrida de thread no boot) caem na primeira sessão.
const sessions = [];
let current = null;
const newSession = (startEntry) => {
  current = {
    start: startEntry || null,
    entries: [],
  };
  sessions.push(current);
};
for (const e of entries) {
  if (e.event === 'session_start') {
    if (current && current.entries.length === 0 && !current.start) {
      current.start = e; // linhas órfãs do boot ficam nesta mesma sessão
    } else {
      newSession(e);
    }
  } else if (!current) {
    newSession(null);
  }
  current.entries.push(e);
}

// ---------------------------------------------------------- per-session ----

function analyze(session, index) {
  const es = session.entries;
  const first = es.find((e) => e.ts);
  const last = [...es].reverse().find((e) => e.ts);
  const durationMs = first && last ? ms(last.ts) - ms(first.ts) : 0;

  let ticks = 0;
  let idleTicks = 0;
  const verbs = new Map(); // verb -> {ok, fail}
  const failReasons = new Map(); // normalized message -> count
  const goalDone = []; // {id, ts}
  const latencies = []; // prompt->signal ms
  const execTimes = []; // execution_time_ms
  const reflexes = new Map(); // action -> count
  let deaths = 0;
  let damageTotal = 0;
  let chatIn = 0;
  let chatReplies = 0;
  let unknownLabels = 0;
  let malformed = 0;
  let tickErrors = 0;

  // estagnação: runs de action_result com ok=false e mesma assinatura
  let stagnationRuns = 0;
  let worstStagnation = null; // {signature, length}
  let runSig = null;
  let runLen = 0;
  const closeRun = () => {
    if (runLen >= 3) {
      stagnationRuns++;
      if (!worstStagnation || runLen > worstStagnation.length) {
        worstStagnation = { signature: runSig, length: runLen };
      }
    }
    runSig = null;
    runLen = 0;
  };

  // tempo por goal: primeiro tick em que o goal aparece ativo -> goal_done
  const goalFirstSeen = new Map();

  let lastPromptTs = null;
  let lastParsed = null;

  for (const e of es) {
    const n = occurrences(e);
    switch (e.event) {
      case 'tick_start':
        ticks += n;
        if (e.goal && !goalFirstSeen.has(e.goal)) goalFirstSeen.set(e.goal, ms(e.ts));
        break;
      case 'tick_idle':
        ticks += n;
        idleTicks += n;
        break;
      case 'world_state': {
        // formato antigo (sem tick_idle): world_state com data vazio = ocioso
        const empty = !e.data || Object.keys(e.data).length === 0;
        if (empty) idleTicks += n;
        break;
      }
      case 'prompt':
        lastPromptTs = ms(e.ts);
        break;
      case 'signal':
        if (lastPromptTs != null && e.ts) {
          latencies.push(ms(e.ts) - lastPromptTs);
          lastPromptTs = null;
        }
        break;
      case 'action_parsed':
        lastParsed = e;
        break;
      case 'action_result': {
        const verb = e.verb || (lastParsed && lastParsed.verb) || '?';
        const args = e.args || (lastParsed && lastParsed.args) || [];
        const v = verbs.get(verb) || { ok: 0, fail: 0 };
        if (e.ok) v.ok += n; else v.fail += n;
        verbs.set(verb, v);
        if (typeof e.execution_time_ms === 'number') execTimes.push(e.execution_time_ms);
        if (!e.ok) {
          const norm = String(e.message || '?').replace(/-?\d+(\.\d+)?/g, '#');
          failReasons.set(norm, (failReasons.get(norm) || 0) + n);
          const sig = `${verb} ${args.join(' ')}`;
          if (sig === runSig) runLen += n;
          else { closeRun(); runSig = sig; runLen = n; }
        } else {
          closeRun();
        }
        lastParsed = null;
        break;
      }
      case 'goal_done':
        goalDone.push({ id: e.id, ts: ms(e.ts) });
        break;
      case 'sidecar_event': {
        const kind = e.kind || '';
        const data = e.data || {};
        if (kind === 'reflex') {
          const a = data.action || '?';
          reflexes.set(a, (reflexes.get(a) || 0) + n);
        } else if (kind === 'death') deaths += n;
        else if (kind === 'damage') damageTotal += (data.amount || 0) * n;
        else if (kind === 'chat') chatIn += n;
        break;
      }
      case 'chat_reply': chatReplies += n; break;
      case 'unknown_label': unknownLabels += n; break;
      case 'signal_malformed': malformed += n; break;
      case 'tick_error': tickErrors += n; break;
    }
  }
  closeRun();

  // tempo por goal (quando o log tem "goal" no tick_start; senão só a ordem)
  const goalTimings = goalDone.map((g, i) => {
    const from = goalFirstSeen.get(g.id) ??
      (i > 0 ? goalDone[i - 1].ts : (first ? ms(first.ts) : NaN));
    return { id: g.id, seconds: Number.isFinite(from) ? (g.ts - from) / 1000 : null };
  });

  const stats = (arr) => {
    if (!arr.length) return null;
    const s = [...arr].sort((a, b) => a - b);
    return {
      n: s.length,
      med: s[Math.floor(s.length / 2)],
      p90: s[Math.floor(s.length * 0.9)],
      max: s[s.length - 1],
    };
  };

  let totalOk = 0, totalFail = 0;
  for (const v of verbs.values()) { totalOk += v.ok; totalFail += v.fail; }

  // Score heurístico — só pra comparar runs entre si com UMA linha; os pesos
  // são chutes documentados, não verdade revelada. Reflexo NÃO penaliza
  // (mob spawnar perto é ambiente, não burrice do modelo).
  const score =
    goalDone.length * 100 +
    totalOk * 2 -
    totalFail * 5 -
    (unknownLabels + malformed) * 10 -
    stagnationRuns * 30 -
    deaths * 50;

  return {
    index, first, last, durationMs, ticks, idleTicks,
    verbs, failReasons, goalTimings, latencies: stats(latencies),
    execTimes: stats(execTimes), reflexes, deaths, damageTotal,
    chatIn, chatReplies, unknownLabels, malformed, tickErrors,
    stagnationRuns, worstStagnation, totalOk, totalFail, score,
    config: session.start && session.start.config,
  };
}

// ------------------------------------------------------------- reporting ---

const fmtDur = (msTotal) => {
  const s = Math.round(msTotal / 1000);
  return s >= 60 ? `${Math.floor(s / 60)}m${String(s % 60).padStart(2, '0')}s` : `${s}s`;
};

for (const session of sessions) {
  const r = analyze(session, sessions.indexOf(session) + 1);
  const startTs = session.start ? session.start.ts : (r.first && r.first.ts);

  console.log('═'.repeat(64));
  console.log(`SESSÃO ${r.index}  —  ${startTs || '?'}  (duração ${fmtDur(r.durationMs)})`);
  if (r.config && Object.keys(r.config).length) {
    console.log(`config: temp=${r.config.temperatura} top_p=${r.config.top_p} ` +
      `max_tokens=${r.config.max_tokens} grammar=${r.config.grammar ? 'ON' : 'OFF'}`);
  }
  console.log(`SCORE: ${r.score}   (heurístico — bom só pra comparar runs entre si)`);
  console.log('');

  console.log(`ticks: ${r.ticks} (${r.idleTicks} ociosos/bridge morto)  ` +
    `ações: ${r.totalOk + r.totalFail} (${r.totalOk} ok, ${r.totalFail} falhas` +
    `${r.totalOk + r.totalFail ? `, ${Math.round(100 * r.totalOk / (r.totalOk + r.totalFail))}% sucesso` : ''})`);

  if (r.goalTimings.length) {
    console.log('goals concluídos:');
    for (const g of r.goalTimings) {
      console.log(`  ✔ ${g.id}${g.seconds != null ? `  (${fmtDur(g.seconds * 1000)})` : ''}`);
    }
  } else {
    console.log('goals concluídos: nenhum');
  }

  if (r.verbs.size) {
    console.log('por verbo:');
    for (const [verb, v] of [...r.verbs].sort((a, b) => (b[1].ok + b[1].fail) - (a[1].ok + a[1].fail))) {
      const total = v.ok + v.fail;
      console.log(`  ${verb.padEnd(8)} ${String(total).padStart(4)}x  ` +
        `${Math.round(100 * v.ok / total)}% ok`);
    }
  }

  if (r.failReasons.size) {
    console.log('top falhas (coordenadas normalizadas pra #):');
    const top = [...r.failReasons].sort((a, b) => b[1] - a[1]).slice(0, 5);
    for (const [msg, count] of top) console.log(`  ${String(count).padStart(3)}x  ${msg}`);
  }

  if (r.stagnationRuns) {
    console.log(`loops de estagnação (≥3 falhas idênticas seguidas): ${r.stagnationRuns}` +
      (r.worstStagnation ? `  (pior: "${r.worstStagnation.signature}" ${r.worstStagnation.length}x)` : ''));
  }

  const lat = r.latencies;
  if (lat) console.log(`latência de decisão (prompt→signal): mediana ${lat.med}ms, p90 ${lat.p90}ms, max ${lat.max}ms (${lat.n} amostras)`);
  const ex = r.execTimes;
  if (ex) console.log(`execução das ações: mediana ${ex.med}ms, p90 ${ex.p90}ms, max ${ex.max}ms`);

  const extras = [];
  if (r.reflexes.size) extras.push('reflexos: ' + [...r.reflexes].map(([a, c]) => `${a} ${c}x`).join(', '));
  if (r.deaths) extras.push(`mortes: ${r.deaths}`);
  if (r.damageTotal) extras.push(`dano sofrido: ${r.damageTotal.toFixed(1)}`);
  if (r.chatIn) extras.push(`chat do jogador: ${r.chatIn} msg (${r.chatReplies} respondidas)`);
  if (r.unknownLabels) extras.push(`rótulos inválidos: ${r.unknownLabels}`);
  if (r.malformed) extras.push(`sinais malformados: ${r.malformed}`);
  if (r.tickErrors) extras.push(`tick_errors: ${r.tickErrors}`);
  if (extras.length) console.log(extras.join('  ·  '));
  console.log('');
}
