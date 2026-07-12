'use strict';

const { goals } = require('mineflayer-pathfinder');
const { Vec3 } = require('vec3');

const MAX_REACH = 32; // blocks; refuse actions targeting anything farther than this

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

async function mineBlock(bot, [x, y, z]) {
  const [xi, yi, zi] = [x, y, z].map(Number);
  if (!withinReach(bot, xi, yi, zi)) {
    return { ok: false, message: `target (${xi},${yi},${zi}) is farther than ${MAX_REACH} blocks` };
  }
  const block = bot.blockAt(new Vec3(xi, yi, zi));
  if (!block || block.name === 'air') {
    return { ok: false, message: `no minable block at (${xi},${yi},${zi})` };
  }
  await equipBestTool(bot, block);
  await bot.dig(block);
  return { ok: true, message: `mined ${block.name} at (${xi},${yi},${zi})` };
}

async function placeBlock(bot, [x, y, z, itemName]) {
  const [xi, yi, zi] = [x, y, z].map(Number);
  if (!withinReach(bot, xi, yi, zi)) {
    return { ok: false, message: `target (${xi},${yi},${zi}) is farther than ${MAX_REACH} blocks` };
  }
  const item = bot.inventory.items().find((i) => i.name === itemName);
  if (!item) {
    return { ok: false, message: `no ${itemName} in inventory` };
  }
  const referenceBlock = bot.blockAt(new Vec3(xi, yi - 1, zi));
  if (!referenceBlock) {
    return { ok: false, message: `no reference block below (${xi},${yi},${zi})` };
  }
  await bot.equip(item, 'hand');
  await bot.placeBlock(referenceBlock, new Vec3(0, 1, 0));
  return { ok: true, message: `placed ${itemName} at (${xi},${yi},${zi})` };
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

async function craft(bot, [itemName, countRaw]) {
  const count = Number(countRaw) || 1;
  const item = bot.registry.itemsByName[itemName];
  if (!item) {
    return { ok: false, message: `unknown item ${itemName}` };
  }
  // Recipes that need a crafting table are invisible when passing null —
  // use a table within reach if there is one.
  const tableBlock = bot.registry.blocksByName.crafting_table
    ? bot.findBlock({ matching: bot.registry.blocksByName.crafting_table.id, maxDistance: 4 })
    : null;
  const recipe = bot.recipesFor(item.id, null, 1, tableBlock)[0];
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
