'use strict';

const { goals, Movements } = require('mineflayer-pathfinder');
const { Vec3 } = require('vec3');

const MAX_REACH = 32; // blocks; refuse actions targeting anything farther than this
const AIR_LIKE = new Set(['air', 'cave_air', 'void_air', 'water']);

function withinReach(bot, x, y, z) {
  return bot.entity.position.distanceTo({ x, y, z }) <= MAX_REACH;
}

async function moveTo(bot, [x, y, z]) {
  const [xi, yi, zi] = [x, y, z].map(Number);
  if (!withinReach(bot, xi, yi, zi)) {
    return { ok: false, message: `target (${xi},${yi},${zi}) is farther than ${MAX_REACH} blocks` };
  }
  bot.pathfinder.setGoal(new goals.GoalNear(xi, yi, zi, 1));
  return { ok: true, message: `moving toward (${xi},${yi},${zi})` };
}

// Best tool in inventory for this block, worst-to-best tier order. Digging
// stone bare-handed works but takes ~7x longer and drops nothing for ores —
// the model never asks to equip, so do it implicitly.
const TIER_ORDER = ['wooden', 'golden', 'stone', 'iron', 'diamond', 'netherite'];
const MATERIAL_TOOL = {
  'mineable/pickaxe': '_pickaxe',
  'mineable/axe': '_axe',
  'mineable/shovel': '_shovel',
  'mineable/hoe': '_hoe',
};

async function equipBestTool(bot, block) {
  const suffix = MATERIAL_TOOL[block.material];
  if (!suffix) return;
  let best = null;
  let bestTier = -1;
  for (const item of bot.inventory.items()) {
    if (!item.name.endsWith(suffix)) continue;
    const tier = TIER_ORDER.indexOf(item.name.slice(0, -suffix.length));
    if (tier > bestTier) { best = item; bestTier = tier; }
  }
  if (best) await bot.equip(best, 'hand');
}

// Vanilla survival dig reach; digging from farther both looks like cheating
// and gets rejected by any strict server/anti-cheat.
const DIG_RANGE = 4;
const APPROACH_MS = 8000;

// Waits (bounded) until the bot is on solid ground. dig()'s break speed is
// fixed the instant it's called (mineflayer reads bot.entity.onGround once,
// synchronously, before the swing even starts) — if a pathfinder goal is
// still active, the bot can be mid-hop at that exact tick, and Minecraft
// applies a flat 5x break-speed penalty for the ENTIRE dig whenever onGround
// reads false at that moment (prismarine-block's digTime, notOnGround
// branch). A birch_log that should take ~3s was measured taking ~15s live on
// 2026-07-12 — exactly 5x. Bounded: falls through past the deadline rather
// than hanging forever on e.g. knockback keeping the bot airborne.
async function settleOnGround(bot, timeoutMs = 500) {
  const deadline = Date.now() + timeoutMs;
  while (!bot.entity.onGround && Date.now() < deadline) {
    await new Promise((resolve) => bot.once('physicsTick', resolve));
  }
}

// Corpo em modo sobrevivência? O reflexTick (index.js) marca isso quando
// assume o controle (fuga/ataque/comer). Digar nesse estado é inútil: o
// reflexo vai puxar a mira/posição no meio e o servidor descarta o dig.
function reflexActive(bot) {
  return bot.alyssaReflexBusyUntil && Date.now() < bot.alyssaReflexBusyUntil;
}

async function mineBlock(bot, [x, y, z]) {
  const [xi, yi, zi] = [x, y, z].map(Number);
  if (!withinReach(bot, xi, yi, zi)) {
    return { ok: false, message: `target (${xi},${yi},${zi}) is farther than ${MAX_REACH} blocks` };
  }
  if (reflexActive(bot)) {
    return { ok: false, message: 'survival reflex is controlling the body (mob nearby) - fight or flee first' };
  }
  const targetPos = new Vec3(xi, yi, zi);
  const block = bot.blockAt(targetPos);
  if (!block || block.name === 'air') {
    return { ok: false, message: `no minable block at (${xi},${yi},${zi})` };
  }

  // Walk into dig range FIRST. The old flow dug from up to MAX_REACH away
  // and only walked over afterwards to collect the drop — moving the walk
  // before the dig costs nothing extra and is what makes the line-of-sight
  // check below meaningful (from across the map everything reads blocked).
  if (bot.entity.position.distanceTo(targetPos) > DIG_RANGE + 0.5) {
    bot.pathfinder.setGoal(new goals.GoalNear(xi, yi, zi, DIG_RANGE));
    const deadline = Date.now() + APPROACH_MS;
    while (Date.now() < deadline &&
           bot.entity.position.distanceTo(targetPos) > DIG_RANGE + 0.5) {
      await new Promise((r) => setTimeout(r, 150));
    }
    bot.pathfinder.setGoal(null);
    if (bot.entity.position.distanceTo(targetPos) > DIG_RANGE + 0.5) {
      return { ok: false, message: `could not get within ${DIG_RANGE} blocks of (${xi},${yi},${zi})` };
    }
  }
  bot.pathfinder.setGoal(null); // see settleOnGround's doc comment
  await settleOnGround(bot);

  // Line of sight: the dig packet goes through walls (Paper without
  // anti-cheat accepts it), so she was visibly "mining" trunk logs straight
  // through the leaf block in front of them — observed live 2026-07-12,
  // reads as cheating. Do what a player does instead: clear the blocking
  // block(s) first, up to 3 of them; buried deeper than that, report what's
  // in the way so the model can pick a different target.
  //
  // Deliberately NOT bot.canSeeBlock(): its raycast range is the distance to
  // the block's min-CORNER while the ray aims at the CENTER, so at diagonal/
  // elevated angles the ray runs out before touching the near face and it
  // reports "hidden" with a perfectly clear line — 11 of 15 failures in the
  // first fresh-world run (2026-07-16) were this false negative, with the
  // bot refusing the same visible spruce_log 7 ticks straight. One raycast,
  // range past the center, decides visibility AND names the blocker, so the
  // check can never disagree with itself.
  const firstObstruction = () => {
    const headPos = bot.entity.position.offset(0, bot.entity.eyeHeight, 0);
    const toCenter = block.position.offset(0.5, 0.5, 0.5).minus(headPos);
    const dist = toCenter.norm();
    const hit = bot.world.raycast(headPos, toCenter.scaled(1 / dist), dist + 1);
    if (!hit || hit.position.equals(block.position)) return null; // clear line
    return hit;
  };
  // Hard budget for the WHOLE minerar. A half-buried log measured 21s live
  // (probe 2026-07-16: 3 obstruction digs, one at the 5x airborne penalty)
  // — past the C++ socket timeout (20s), which marks the bridge dead, loses
  // the reply and resends next tick, piling up overlapping digs that cancel
  // each other forever (bot.dig() stops the previous dig): visible as her
  // swinging nonstop with zero crack progress. Budget stays well under it.
  const budgetDeadline = Date.now() + 12000;
  const overBudget = (nextBlock) =>
    Date.now() + bot.digTime(nextBlock) > budgetDeadline;
  for (let cleared = 0; cleared < 3; cleared++) {
    const obstruction = firstObstruction();
    if (!obstruction) break;
    if (overBudget(obstruction)) {
      return { ok: false, message:
        `${block.name} at (${xi},${yi},${zi}) is buried too deep to dig out now - pick a more exposed target` };
    }
    await equipBestTool(bot, obstruction);
    // Timeout generoso: depois de cavar um bloco sob os pés o bot ainda está
    // CAINDO no buraco — 500ms estourava antes de aterrissar e o digTime
    // seguinte lia o 5x de airborne (15s medidos no probe), estourando o
    // orçamento à toa.
    await settleOnGround(bot, 1500);
    await bot.dig(obstruction);
  }
  const stillBlocked = firstObstruction();
  if (stillBlocked) {
    return { ok: false, message:
      `${block.name} at (${xi},${yi},${zi}) is hidden behind ${stillBlocked.name} - mine that first or pick another target` };
  }

  await equipBestTool(bot, block);
  await settleOnGround(bot, 1500); // ver comentário no loop de obstrução
  if (overBudget(block)) {
    // "equip a better tool" quando ela NÃO TEM ferramenta ensinou o loop
    // errado a noite toda (2026-07-16: 120 tentativas de equipar picareta
    // inexistente) — diferencie "não tem" de "tem uma fraca".
    const hasPickaxe = bot.inventory.items().some((i) => i.name.endsWith('_pickaxe'));
    return { ok: false, message: hasPickaxe
      ? `${block.name} at (${xi},${yi},${zi}) would take too long with your current pickaxe - upgrade it or pick another target`
      : `you have NO pickaxe - stop mining ${block.name}; get wood planks and craft one first (craftar wooden_pickaxe 1)` };
  }
  await bot.dig(block);
  // Truth-check: bot.dig() resolves on a CLIENT-SIDE timer and marks the
  // block broken locally no matter what the server thinks (digging.js
  // finishDigging -> _updateBlockState). If anything yanked the bot's
  // aim/position mid-dig (the reflex layer, knockback), the server silently
  // rejects the break and restores the block a moment later — a whole run
  // of phantom "mined ok, 100%" with zero real breaks was observed live on
  // 2026-07-16. Wait for the server's correction window, then ask the world
  // if the block is actually gone.
  await new Promise((r) => setTimeout(r, 300));
  const after = bot.blockAt(targetPos);
  if (after && after.name === block.name) {
    return { ok: false, message:
      `dig of ${block.name} did not register on the server (interrupted mid-dig?)` };
  }
  // Digging only breaks the block — the drop lands where it stood, and
  // pickup needs the bot's hitbox to touch the item (~1 block). Without
  // walking over, mined blocks routinely never reached the inventory —
  // observed live 2026-07-12: wood broke fine but never counted toward the
  // "8 troncos" goal. Cheap now that the approach already brought us close.
  bot.pathfinder.setGoal(new goals.GoalNear(xi, yi, zi, 1));
  const pickupDeadline = Date.now() + 3000;
  while (Date.now() < pickupDeadline &&
         bot.entity.position.distanceTo(targetPos) > 1.5) {
    await new Promise((r) => setTimeout(r, 150));
  }
  bot.pathfinder.setGoal(null);
  return { ok: true, message: `mined ${block.name} at (${xi},${yi},${zi})` };
}

async function placeBlock(bot, [x, y, z, itemName]) {
  const [xi, yi, zi] = [x, y, z].map(Number);
  if (!withinReach(bot, xi, yi, zi)) {
    return { ok: false, message: `target (${xi},${yi},${zi}) is farther than ${MAX_REACH} blocks` };
  }
  if (reflexActive(bot)) {
    return { ok: false, message: 'survival reflex is controlling the body (mob nearby) - fight or flee first' };
  }
  const item = bot.inventory.items().find((i) => i.name === itemName);
  if (!item) {
    return { ok: false, message: `no ${itemName} in inventory` };
  }
  // "colocar" targets a LABEL, and state.js's nearbyBlocks() only ever
  // reports solid, non-air blocks as labels — so (xi,yi,zi) is always
  // already-occupied ground, never the empty spot the new block should
  // land in. This used to treat (xi,yi,zi) itself as the placement target
  // and look one block BELOW that for a reference, which asks mineflayer to
  // place a block exactly where the solid labeled block already sits —
  // impossible, so blockUpdate never fires and every single "colocar"
  // timed out at 5s live on 2026-07-12 (100% failure). The label itself
  // IS the reference block; the new block goes on top of it.
  const referenceBlock = bot.blockAt(new Vec3(xi, yi, zi));
  if (!referenceBlock || referenceBlock.name === 'air') {
    return { ok: false, message: `no solid block at (${xi},${yi},${zi}) to place against` };
  }
  // The label is always solid, but the space directly ABOVE it isn't
  // guaranteed to be open — e.g. a mid-tree-trunk log usually has another
  // log or leaves right above it. bot.placeBlock() just times out silently
  // in that case (5s "blockUpdate did not fire", no reason given) — observed
  // live 2026-07-12 against a birch_log trunk block. Check first and fail
  // fast with a message that actually tells the model what to try instead.
  // boundingBox 'empty' (grama, flor, neve) NÃO conta como ocupado: o
  // servidor substitui esses blocos ao colocar, igual um jogador faria.
  const targetPos = new Vec3(xi, yi + 1, zi);
  const targetBlock = bot.blockAt(targetPos);
  if (targetBlock && !AIR_LIKE.has(targetBlock.name) && targetBlock.boundingBox !== 'empty') {
    return { ok: false, message:
      `spot above (${xi},${yi},${zi}) is occupied by ${targetBlock.name}, pick a different block to place against` };
  }

  // Anda até o alvo primeiro — colocar tinha alcance de 32 blocos enquanto o
  // servidor só aceita ~4.5, e o mineflayer não valida: só estoura o timeout
  // de 5s do blockUpdate sem dizer o motivo (7 de 16 colocar da run de
  // 2026-07-17 foram isso; colocar rodava a 0% enquanto minerar, que ganhou
  // approach, rodava a 79%).
  if (bot.entity.position.distanceTo(targetPos) > DIG_RANGE + 0.5) {
    bot.pathfinder.setGoal(new goals.GoalNear(xi, yi, zi, DIG_RANGE));
    const deadline = Date.now() + APPROACH_MS;
    while (Date.now() < deadline &&
           bot.entity.position.distanceTo(targetPos) > DIG_RANGE + 0.5) {
      await new Promise((r) => setTimeout(r, 150));
    }
    bot.pathfinder.setGoal(null);
    if (bot.entity.position.distanceTo(targetPos) > DIG_RANGE + 0.5) {
      return { ok: false, message: `could not get within ${DIG_RANGE} blocks of (${xi},${yi},${zi}) to place` };
    }
  }

  // Em pé no lugar onde o bloco vai nascer = o servidor rejeita a colocação
  // (colisão com o próprio jogador) sem resposta nenhuma — mais uma fonte
  // do timeout mudo de 5s. "Coloca aqui" do jogador quase sempre termina
  // com ela parada EXATAMENTE em cima do bloco de referência. Sai de cima
  // primeiro, com prazo; se não conseguir, explica em vez de estourar.
  const standingOnSpot = () => {
    const feet = bot.entity.position.floored();
    return feet.equals(targetPos) || feet.offset(0, 1, 0).equals(targetPos);
  };
  if (standingOnSpot()) {
    bot.pathfinder.setGoal(new goals.GoalInvert(new goals.GoalNear(xi, yi + 1, zi, 2)));
    const deadline = Date.now() + 4000;
    while (Date.now() < deadline && standingOnSpot()) {
      await new Promise((r) => setTimeout(r, 150));
    }
    bot.pathfinder.setGoal(null);
    if (standingOnSpot()) {
      return { ok: false, message:
        `you are standing exactly where the ${itemName} would go - move one block away first` };
    }
  }

  await settleOnGround(bot, 1000);
  await bot.equip(item, 'hand');
  await bot.placeBlock(referenceBlock, new Vec3(0, 1, 0));
  return { ok: true, message: `placed ${itemName} at (${xi},${yi + 1},${zi})` };
}

const APPROACH_TIMEOUT_MS = 6000;
const MELEE_RANGE = 3.5;

// Entidade ainda válida pra atacar? Slime que morre/divide entre a detecção
// e o pacote de ataque = kick "invalid_entity_attacked" do servidor
// (aconteceu ao vivo em 2026-07-12).
function attackable(bot, e) {
  return e && e.isValid !== false && e.position && bot.entities[e.id] === e;
}

// Só seres vivos são alvos: atacar um drop ("E1=item") ou orb de XP também
// dá kick invalid_entity_attacked — o modelo tentou isso ao vivo quando só
// havia itens por perto.
const NON_LIVING_TYPES = new Set(['object', 'orb', 'projectile', 'other', 'global']);
function isLivingTarget(e) {
  if (NON_LIVING_TYPES.has(e.type)) return false;
  if (e.name === 'item' || e.name === 'experience_orb') return false;
  return true;
}

// Antes de brigar, saca a melhor arma do inventário — ela ficava socando
// zumbi de mão vazia com uma diamond_sword no bolso.
const WEAPON_ORDER = [
  'netherite_sword', 'diamond_sword', 'iron_sword', 'stone_sword', 'golden_sword', 'wooden_sword',
  'netherite_axe', 'diamond_axe', 'iron_axe', 'stone_axe', 'golden_axe', 'wooden_axe',
];
async function equipBestWeapon(bot) {
  for (const name of WEAPON_ORDER) {
    const item = bot.inventory.items().find((i) => i.name === name);
    if (item) {
      if (!bot.heldItem || bot.heldItem.name !== name) await bot.equip(item, 'hand');
      return;
    }
  }
}

async function attack(bot, [targetName]) {
  const wanted = String(targetName || '').toLowerCase();
  // Nearest match, not first match — Object.values order is arbitrary and
  // with several zombies around, "the one 15 blocks away" fails the range
  // check while one stands next to the bot. (mobType é deprecated e cospe
  // um Trace por acesso — name/displayName cobrem os dois casos.)
  const target = Object.values(bot.entities)
    .filter((e) => e !== bot.entity && e.position && isLivingTarget(e) &&
      ((e.name && e.name.toLowerCase() === wanted) ||
       (e.displayName && String(e.displayName).toLowerCase() === wanted)))
    .sort((a, b) =>
      bot.entity.position.distanceTo(a.position) - bot.entity.position.distanceTo(b.position))[0];
  if (!target) {
    // Diferencia "não existe" de "existe mas não é atacável" (drop de item):
    // a segunda mensagem ensina o modelo a parar de tentar.
    const nonLiving = Object.values(bot.entities).some((e) =>
      e !== bot.entity && !isLivingTarget(e) &&
      ((e.name && e.name.toLowerCase() === wanted) ||
       (e.displayName && String(e.displayName).toLowerCase() === wanted)));
    return { ok: false, message: nonLiving
      ? `${targetName} is a dropped item, not a creature - walk over it to pick it up`
      : `no visible entity named ${targetName}` };
  }

  // Fora de alcance: ANDA até o alvo primeiro (o modelo manda "atacar" e
  // espera que o corpo resolva a distância — antes isso falhava 6x seguidas
  // enquanto o slime ficava parado a 14 blocos).
  if (bot.entity.position.distanceTo(target.position) > MELEE_RANGE) {
    bot.pathfinder.setGoal(new goals.GoalFollow(target, 2), true);
    const deadline = Date.now() + APPROACH_TIMEOUT_MS;
    while (Date.now() < deadline) {
      await new Promise((r) => setTimeout(r, 150));
      if (!attackable(bot, target)) {
        bot.pathfinder.setGoal(null);
        return { ok: false, message: `${targetName} disappeared while approaching` };
      }
      if (bot.entity.position.distanceTo(target.position) <= MELEE_RANGE) break;
    }
    bot.pathfinder.setGoal(null);
    if (bot.entity.position.distanceTo(target.position) > MELEE_RANGE) {
      return { ok: false, message: `${targetName} unreachable within ${APPROACH_TIMEOUT_MS / 1000}s` };
    }
  }

  if (!attackable(bot, target)) {
    return { ok: false, message: `${targetName} is no longer a valid target` };
  }
  try {
    await equipBestWeapon(bot);
    await bot.lookAt(target.position.offset(0, 1, 0));
    bot.attack(target);
  } catch (err) {
    return { ok: false, message: `attack failed: ${err.message}` };
  }
  return { ok: true, message: `attacked ${targetName}` };
}

async function equip(bot, [itemName]) {
  const item = bot.inventory.items().find((i) => i.name === itemName);
  if (!item) {
    return { ok: false, message: `no ${itemName} in inventory` };
  }
  await bot.equip(item, 'hand');
  return { ok: true, message: `equipped ${itemName}` };
}

async function craft(bot, args) {
  // gameplayModel currently runs with NO grammar constraining its output
  // (see docs/proximos-passos.md — disabled for latency on the 262K-vocab
  // E2B model), so a 2-arg verb like this arrives in whatever shape the
  // model felt like that tick: "oak_planks 4", "item 4 oak_planks", "4
  // oak_planks" were all observed live 2026-07-12. Positional [itemName,
  // countRaw] silently breaks on the extra/reordered token ("unknown item
  // item"), so instead scan every token for one that's a real item name and
  // one that's a bare integer, and ignore the rest.
  let itemName = null;
  let count = 1;
  for (const arg of args) {
    if (bot.registry.itemsByName[arg]) itemName = arg;
    else if (/^\d+$/.test(arg)) count = Number(arg);
  }
  if (!itemName) {
    return { ok: false, message: `no valid item name in craftar args: ${args.join(' ')}` };
  }
  let item = bot.registry.itemsByName[itemName];
  // Recipes that need a crafting table are invisible when passing null —
  // use a table within reach if there is one.
  const tableBlock = bot.registry.blocksByName.crafting_table
    ? bot.findBlock({ matching: bot.registry.blocksByName.crafting_table.id, maxDistance: 4 })
    : null;
  let recipe = bot.recipesFor(item.id, null, 1, tableBlock)[0];

  // Planks/sticks-style recipes are per wood species in vanilla — asking for
  // "oak_planks" while only holding birch_log has no recipe. The goal prompt
  // already says "or whichever wood type you have," but a 1B model doesn't
  // reliably self-correct after the first failure (observed retrying
  // oak_planks 3x running). Fall back to whatever species is actually in
  // the inventory instead of failing outright.
  if (!recipe && itemName.endsWith('_planks')) {
    const ownedLog = bot.inventory.items().find((i) => i.name.endsWith('_log'));
    if (ownedLog) {
      const species = ownedLog.name.slice(0, -'_log'.length);
      const altItem = bot.registry.itemsByName[`${species}_planks`];
      const altRecipe = altItem && bot.recipesFor(altItem.id, null, 1, tableBlock)[0];
      if (altRecipe) {
        item = altItem;
        recipe = altRecipe;
        itemName = altItem.name;
      }
    }
  }

  if (!recipe) {
    return { ok: false, message: `no known recipe for ${itemName} (missing table or ingredients)` };
  }
  await bot.craft(recipe, count, tableBlock);
  return { ok: true, message: `crafted ${count}x ${itemName}` };
}

async function eat(bot) {
  if (bot.food >= 20) {
    return { ok: false, message: 'not hungry (food is full)' };
  }
  const foods = bot.registry.foodsByName || {};
  const item = bot.inventory.items().find((i) => foods[i.name]);
  if (!item) {
    return { ok: false, message: 'no food in inventory' };
  }
  await bot.equip(item, 'hand');
  await bot.consume();
  return { ok: true, message: `ate ${item.name} (food ${bot.food}/20)` };
}

async function chat(bot, args) {
  const message = args.join(' ');
  bot.chat(message);
  return { ok: true, message: `said: ${message}` };
}

// Céu visível direto acima? (bloco sólido nenhum entre a cabeça e o limite
// do mundo) — critério de "cheguei na superfície".
function skyAbove(bot) {
  const p = bot.entity.position.floored();
  for (let y = p.y + 1; y < 320; y++) {
    const b = bot.blockAt(new Vec3(p.x, y, p.z));
    if (b && !AIR_LIKE.has(b.name) && b.boundingBox !== 'empty') return false;
  }
  return true;
}

// "subir": volta pra superfície cavando escada pra cima se precisar — é
// exatamente como um jogador escapa de caverna. A run de 14h de 2026-07-17
// provou que essa ação faltava: a picareta quebrou no fundo da caverna, sem
// madeira lá embaixo pra fazer outra, e ela moeu ~5700 falhas de mineração
// sem NENHUM verbo que a levasse de volta pra cima. Sobe em lances de ~12
// blocos por chamada (orçamento de 20s < timeout do C++); o modelo repete
// até o céu aparecer.
async function climbToSurface(bot) {
  if (reflexActive(bot)) {
    return { ok: false, message: 'survival reflex is controlling the body (mob nearby) - fight or flee first' };
  }
  const startY = Math.floor(bot.entity.position.y);
  if (skyAbove(bot)) {
    return { ok: false, message: `already at the surface (y=${startY}) - sky is visible above you` };
  }

  // canDig LIGADO só aqui, num Movements próprio: o global fica canDig=false
  // de propósito (mover = andar, não escavar) — escapar de caverna é a única
  // exceção legítima.
  const climbMovements = new Movements(bot);
  climbMovements.canDig = true;
  const previousMovements = bot.pathfinder.movements;
  bot.pathfinder.setMovements(climbMovements);
  bot.pathfinder.setGoal(new goals.GoalY(startY + 12));

  const deadline = Date.now() + 20000;
  while (Date.now() < deadline && bot.entity.position.y < startY + 11.5) {
    await new Promise((r) => setTimeout(r, 250));
    if (reflexActive(bot)) break; // sobrevivência tem prioridade sobre a escalada
  }
  bot.pathfinder.setGoal(null);
  bot.pathfinder.setMovements(previousMovements);

  const endY = Math.floor(bot.entity.position.y);
  if (skyAbove(bot)) {
    return { ok: true, message: `reached the surface (y=${endY})` };
  }
  if (endY > startY + 2) {
    return { ok: true, message: `climbed from y=${startY} to y=${endY}, still underground - use subir again` };
  }
  return { ok: false, message: `stuck at y=${endY}, could not climb from here - try moving somewhere open first` };
}

async function wait(bot) {
  return { ok: true, message: 'waiting' };
}

const HANDLERS = {
  mover: moveTo,
  minerar: mineBlock,
  colocar: placeBlock,
  atacar: attack,
  craftar: craft,
  comer: eat,
  equipar: equip,
  falar: chat,
  subir: climbToSurface,
  esperar: wait,
};

async function executeAction(bot, verb, args) {
  const handler = HANDLERS[verb];
  if (!handler) {
    return { ok: false, message: `unknown verb: ${verb}` };
  }
  try {
    return await handler(bot, args);
  } catch (err) {
    return { ok: false, message: `${verb} failed: ${err.message}` };
  }
}

module.exports = { executeAction };
