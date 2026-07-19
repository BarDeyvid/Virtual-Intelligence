'use strict';

const net = require('net');
const mineflayer = require('mineflayer');
const { pathfinder, Movements, goals } = require('mineflayer-pathfinder');

const { buildState } = require('./state');
const { executeAction } = require('./actions');

const MC_HOST = process.env.MC_HOST || 'localhost';
const MC_PORT = Number(process.env.MC_PORT || 25565);
const MC_USERNAME = process.env.MC_USERNAME || 'Alyssa';
// Unset -> mineflayer pings the server first to auto-detect the version
// (minecraft-protocol's autoVersion). That ping is a separate status-query
// handshake, not the real login, and a paused singleplayer world (e.g. the
// game window alt-tabbed/menu open, which freezes its network processing)
// answers it slower or not at all — every createBot() call, including every
// automatic reconnect, redoes this ping and can hang/ETIMEDOUT on it even
// though the actual server is fine. Setting MC_VERSION skips it entirely.
const MC_VERSION = process.env.MC_VERSION || false;
const BRIDGE_PORT = Number(process.env.BRIDGE_PORT || 8765);
const RECONNECT_DELAY_MS = 10000;

let bot = null;
let spawned = false;
let lastHealth = null;
let actionInFlight = null; // verbo da ação física em execução (serialização)
const sockets = new Set();

function broadcastEvent(event, data) {
  const line = JSON.stringify({ type: 'event', event, data }) + '\n';
  for (const socket of sockets) {
    socket.write(line);
  }
}

function createBot() {
  spawned = false;
  lastHealth = null;

  bot = mineflayer.createBot({
    host: MC_HOST,
    port: MC_PORT,
    username: MC_USERNAME,
    version: MC_VERSION,
  });

  bot.loadPlugin(pathfinder);

  bot.once('spawn', () => {
    const movements = new Movements(bot);
    // Default pathfinder behavior digs through obstacles to reach a goal —
    // "mover" is supposed to be walking, not excavating. Without this, a
    // GoalNear that requires going through a block silently turns into the
    // bot digging a hole and getting stuck, which reads as "did nothing" from
    // the action log (no "minerar" was ever issued, so nothing gets reported).
    movements.canDig = false;
    bot.pathfinder.setMovements(movements);
    lastHealth = bot.health;
    spawned = true;
    console.log(`[minecraft-bridge] spawned as ${MC_USERNAME} on ${MC_HOST}:${MC_PORT}`);
  });

  bot.on('health', () => {
    if (lastHealth !== null && bot.health < lastHealth) {
      broadcastEvent('damage', { amount: lastHealth - bot.health, health: bot.health });
    }
    lastHealth = bot.health;
  });

  bot.on('death', () => {
    broadcastEvent('death', { position: bot.entity ? bot.entity.position : null });
  });

  bot.on('chat', (username, message) => {
    if (username === bot.username) return;
    broadcastEvent('chat', { username, message });
  });

  bot.on('kicked', (reason) => console.error('[minecraft-bridge] kicked:', reason));
  bot.on('error', (err) => console.error('[minecraft-bridge] bot error:', err));

  // Server restart / kick / network drop: the old bot object is dead for
  // good — recreate it. Without this, the sidecar keeps running with a
  // zombie bot and every action fails until someone manually restarts it.
  bot.once('end', (reason) => {
    spawned = false;
    console.error(`[minecraft-bridge] disconnected from server (${reason}); reconnecting in ${RECONNECT_DELAY_MS / 1000}s`);
    setTimeout(createBot, RECONNECT_DELAY_MS);
  });
}

createBot();

// --- Reflex layer (plano C1): sistema nervoso autônomo -----------------------
//
// O tick do LLM leva ~1-3s; um slime mata em menos. Este loop de 250ms reage
// a ameaças imediatas SEM esperar o modelo: comportamento determinístico,
// zero VRAM, prioridade sobre a ação em curso (cancela o pathfinder). Cada
// reflexo vira um `event reflex` — o C++ loga e conta pro estrategista (E2B)
// no prompt do tick seguinte, pra ele não decidir às cegas.

const REFLEX_INTERVAL_MS = 250;
const HOSTILE_RANGE = 3;      // blocos: mob hostil mais perto que isso = ameaça
const FLEE_DISTANCE = 8;      // blocos: pra onde correr
const LOW_HEALTH = 6;         // <6/20: fugir de briga
const LOW_FOOD = 6;           // <6/20: comer se der
let reflexBusy = false;       // comer/reagir em andamento — não empilha reflexos
let lastAttackAt = 0;         // cooldown de ataque (espada ~600ms)

const HOSTILE_NAMES = new Set([
  'zombie', 'skeleton', 'creeper', 'spider', 'cave_spider', 'witch', 'slime',
  'drowned', 'husk', 'stray', 'phantom', 'pillager', 'vindicator', 'enderman',
]);

function nearestHostile() {
  let best = null;
  let bestDist = Infinity;
  for (const e of Object.values(bot.entities)) {
    if (e === bot.entity || !e.position || !e.name) continue;
    if (e.isValid === false) continue; // morto/removido: atacar = kick do servidor
    const hostile = HOSTILE_NAMES.has(e.name) || e.kind === 'Hostile mobs';
    if (!hostile) continue;
    const d = bot.entity.position.distanceTo(e.position);
    if (d < bestDist) { best = e; bestDist = d; }
  }
  return best ? { entity: best, dist: bestDist } : null;
}

function fleeFrom(entity) {
  // Direção oposta ao mob, FLEE_DISTANCE blocos — pathfinder cuida do terreno.
  const away = bot.entity.position.minus(entity.position).normalize().scaled(FLEE_DISTANCE);
  const target = bot.entity.position.plus(away);
  bot.pathfinder.setGoal(new goals.GoalXZ(target.x, target.z));
}

// Reflexo assumiu o corpo: aborta o dig em andamento (senão o mineflayer
// segue um dig fantasma — resolve num timer client-side e marca o bloco
// como quebrado localmente MESMO com o servidor rejeitando porque o bot
// saiu andando/olhando pro mob; run inteira de "mined ok" sem quebrar nada
// observada ao vivo 2026-07-16) e sinaliza pro actions.js não INICIAR ação
// física nova enquanto o corpo está em modo sobrevivência.
function reflexTakeover() {
  bot.stopDigging();
  bot.alyssaReflexBusyUntil = Date.now() + 1500;
}

async function reflexTick() {
  if (!spawned || !bot || !bot.entity || reflexBusy) return;

  const threat = nearestHostile();

  // 1. Vida baixa + ameaça por perto: fugir SEMPRE (sem heroísmo).
  if (bot.health < LOW_HEALTH && threat && threat.dist < HOSTILE_RANGE * 3) {
    reflexBusy = true;
    reflexTakeover();
    fleeFrom(threat.entity);
    broadcastEvent('reflex', {
      action: 'fugir', reason: `vida ${bot.health}/20 e ${threat.entity.name} a ${threat.dist.toFixed(1)} blocos`,
    });
    reflexBusy = false;
    return;
  }

  // 2. Hostil colado: atacar (com vida) ou fugir (sem).
  if (threat && threat.dist <= HOSTILE_RANGE) {
    reflexBusy = true;
    if (bot.health >= LOW_HEALTH + 2) {
      const now = Date.now();
      if (now - lastAttackAt > 600) {
        lastAttackAt = now;
        reflexTakeover();
        bot.pathfinder.setGoal(null); // reflexo tem prioridade sobre o plano do LLM
        try {
          await bot.lookAt(threat.entity.position.offset(0, 1, 0));
          // Revalida DEPOIS do lookAt (await): o mob pode ter morrido no meio
          // — atacar entidade inválida = kick do servidor.
          if (threat.entity.isValid !== false && bot.entities[threat.entity.id]) {
            bot.attack(threat.entity);
            broadcastEvent('reflex', {
              action: 'atacar', reason: `${threat.entity.name} a ${threat.dist.toFixed(1)} blocos`,
            });
          }
        } catch (_) { /* mob sumiu no meio do gesto — reflexo aborta em paz */ }
      }
    } else {
      reflexTakeover();
      fleeFrom(threat.entity);
      broadcastEvent('reflex', {
        action: 'fugir', reason: `${threat.entity.name} colado e vida ${bot.health}/20`,
      });
    }
    reflexBusy = false;
    return;
  }

  // 3. Fome crítica, sem ameaça: comer (consume() leva ~1.6s — busy segura).
  if (bot.food < LOW_FOOD && !threat) {
    reflexBusy = true;
    try {
      reflexTakeover(); // comer troca o item da mão — corrompe dig em andamento igual
      const result = await executeAction(bot, 'comer', []);
      if (result.ok) broadcastEvent('reflex', { action: 'comer', reason: `fome ${bot.food}/20` });
    } finally {
      reflexBusy = false;
    }
  }
}

setInterval(() => { reflexTick().catch(() => {}); }, REFLEX_INTERVAL_MS);

// --- Local JSONL socket for the C++ core (AlyssaNet's MinecraftBridge) ---

function handleMessage(socket, msg) {
  // Before spawn (or between disconnect and reconnect) bot.entity doesn't
  // exist — buildState would throw inside the socket data handler and kill
  // the whole process. Answer with an error line instead.
  if (!spawned || !bot || !bot.entity) {
    if (msg.type === 'get_state') {
      socket.write(JSON.stringify({ type: 'state', data: {} }) + '\n');
    } else if (msg.type === 'action') {
      socket.write(JSON.stringify({ type: 'action_result', ok: false, message: 'bot not spawned yet' }) + '\n');
    } else {
      socket.write(JSON.stringify({ type: 'error', message: 'bot not spawned yet' }) + '\n');
    }
    return;
  }

  if (msg.type === 'get_state') {
    socket.write(JSON.stringify({ type: 'state', data: buildState(bot) }) + '\n');
    return;
  }
  if (msg.type === 'action') {
    // Um corpo, uma ação física por vez. Sem isso, quando uma ação estoura o
    // timeout do socket C++ (20s) a resposta se perde e o tick seguinte manda
    // OUTRA ação com a primeira ainda rodando — cada bot.dig() novo cancela o
    // anterior (digging.js: stopDigging no início do dig), então os digs se
    // cancelam em cadeia e o servidor nunca acumula progresso de quebra
    // (observado ao vivo 2026-07-16: swing eterno, zero rachadura).
    if (actionInFlight) {
      socket.write(JSON.stringify({
        type: 'action_result', ok: false,
        message: `still executing previous action (${actionInFlight}) - wait for it`,
      }) + '\n');
      return;
    }
    actionInFlight = msg.verb;
    executeAction(bot, msg.verb, msg.args || []).then((result) => {
      socket.write(JSON.stringify({ type: 'action_result', ...result }) + '\n');
    }).finally(() => {
      actionInFlight = null;
    });
    return;
  }
  socket.write(JSON.stringify({ type: 'error', message: `unknown message type: ${msg.type}` }) + '\n');
}

const server = net.createServer((socket) => {
  sockets.add(socket);
  let buffer = '';

  socket.on('data', (chunk) => {
    buffer += chunk.toString('utf8');
    let newlineIndex;
    while ((newlineIndex = buffer.indexOf('\n')) !== -1) {
      const line = buffer.slice(0, newlineIndex);
      buffer = buffer.slice(newlineIndex + 1);
      if (!line.trim()) continue;
      try {
        handleMessage(socket, JSON.parse(line));
      } catch (err) {
        socket.write(JSON.stringify({ type: 'error', message: `bad JSON: ${err.message}` }) + '\n');
      }
    }
  });

  socket.on('close', () => sockets.delete(socket));
  socket.on('error', () => sockets.delete(socket));
});

server.listen(BRIDGE_PORT, '127.0.0.1', () => {
  console.log(`[minecraft-bridge] listening for the C++ core on 127.0.0.1:${BRIDGE_PORT}`);
});
