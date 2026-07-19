'use strict';

// Probe isolado pro bug "ela bate mas não quebra" (2026-07-16): conecta um
// bot de teste, acha um bloco alvo e compara (A) bot.dig cru do mineflayer
// com (B) o mineBlock novo do actions.js, imprimindo o que o SERVIDOR fez
// de verdade (bloco sumiu ou voltou) em cada caso.
//
// Uso: node test_dig.js [host] [port]

const mineflayer = require('mineflayer');
const { pathfinder, Movements, goals } = require('mineflayer-pathfinder');
const { Vec3 } = require('vec3');
const { executeAction } = require('./actions');

const HOST = process.argv[2] || 'localhost';
const PORT = Number(process.argv[3] || 25565);

const bot = mineflayer.createBot({ host: HOST, port: PORT, username: 'Prober' });
bot.loadPlugin(pathfinder);

const log = (...a) => console.log(new Date().toISOString().slice(11, 23), ...a);

bot.on('kicked', (r) => { log('KICKED:', JSON.stringify(r)); process.exit(1); });
bot.on('error', (e) => { log('ERROR:', e.message); });
bot.on('diggingCompleted', (b) => log('  [event] diggingCompleted', b && b.name));
bot.on('diggingAborted', (b) => log('  [event] diggingAborted', b && b.name));

function findTarget() {
  const logIds = Object.values(bot.registry.blocksByName)
    .filter((b) => /_log$/.test(b.name)).map((b) => b.id);
  let block = bot.findBlock({ matching: logIds, maxDistance: 32 });
  if (block) return block;
  // sem árvore por perto: usa um bloco de chão a 2 blocos do bot
  const p = bot.entity.position.floored().offset(2, -1, 0);
  block = bot.blockAt(p);
  return block && block.name !== 'air' ? block : null;
}

async function approach(pos, range) {
  bot.pathfinder.setGoal(new goals.GoalNear(pos.x, pos.y, pos.z, range));
  const deadline = Date.now() + 10000;
  while (Date.now() < deadline && bot.entity.position.distanceTo(pos) > range + 0.5) {
    await new Promise((r) => setTimeout(r, 150));
  }
  bot.pathfinder.setGoal(null);
}

async function serverTruth(pos, beforeName) {
  await new Promise((r) => setTimeout(r, 600));
  const now = bot.blockAt(pos);
  return `bloco agora='${now && now.name}' (antes='${beforeName}') -> ${
    now && now.name === beforeName ? 'NÃO QUEBROU (server rejeitou)' : 'QUEBROU de verdade'}`;
}

bot.once('spawn', async () => {
  log(`spawned em ${bot.entity.position} | versão negociada: ${bot.version}`);
  const movements = new Movements(bot);
  movements.canDig = false;
  bot.pathfinder.setMovements(movements);
  await new Promise((r) => setTimeout(r, 2500)); // chunks

  try {
    // ---------- Teste A: dig cru do mineflayer (fluxo antigo) ----------
    let target = findTarget();
    if (!target) { log('nenhum alvo encontrado perto do spawn'); process.exit(1); }
    log(`ALVO A: ${target.name} @ ${target.position} | dist ${bot.entity.position.distanceTo(target.position).toFixed(2)}`);
    await approach(target.position, 3);
    const distA = bot.entity.position.distanceTo(target.position);
    log(`  aproximei: dist ${distA.toFixed(2)}, onGround=${bot.entity.onGround}, held=${bot.heldItem && bot.heldItem.name}`);
    log(`  digTime previsto: ${bot.digTime(target)}ms`);
    const t0 = Date.now();
    try {
      await bot.dig(target);
      log(`  bot.dig resolveu OK em ${Date.now() - t0}ms`);
    } catch (e) {
      log(`  bot.dig LANÇOU em ${Date.now() - t0}ms: ${e.message}`);
    }
    log('  A:', await serverTruth(target.position, target.name));

    // ---------- Teste B: mineBlock novo (actions.js) ----------
    await new Promise((r) => setTimeout(r, 1000));
    target = findTarget();
    if (!target) { log('sem segundo alvo pro teste B'); process.exit(0); }
    log(`ALVO B: ${target.name} @ ${target.position} | dist ${bot.entity.position.distanceTo(target.position).toFixed(2)}`);
    const t1 = Date.now();
    const result = await executeAction(bot, 'minerar',
      [String(target.position.x), String(target.position.y), String(target.position.z)]);
    log(`  minerar retornou em ${Date.now() - t1}ms: ok=${result.ok} msg="${result.message}"`);
    log('  B:', await serverTruth(target.position, target.name));
  } catch (e) {
    log('EXCEÇÃO:', e.stack);
  }
  process.exit(0);
});
