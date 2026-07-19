'use strict';

const SCAN_RADIUS = 4;   // blocks in each direction around the bot
const MAX_BLOCKS = 40;   // cap payload size
const ENTITY_RADIUS = 16;
const MAX_PER_NAME = 3;  // dedupe: at most N entries of the same block type

// Blocks worth surfacing even when far: resources and interactables. The
// C++ prompt only shows the FIRST 5 blocks of this list, so ordering here
// directly decides what the model gets to choose from.
const INTERESTING = /_ore$|_log$|^crafting_table$|^furnace$|^chest$|^bed$|_sapling$|^wheat$|^carrots$|^potatoes$|^pumpkin$|^melon$|^bamboo$|^sugar_cane$/;

// "Interessante" depende do inventário, não só do bloco: minério sem
// picareta não dropa NADA (a run noturna de 2026-07-16 gastou 2283 ticks
// tentando deepslate_iron_ore de mão vazia porque _ore dominava os rótulos
// B1-B5), e mais madeira com o estoque cheio é acumulação sem propósito
// (observado: ela seguia derrubando árvores com troncos de sobra). O bloco
// continua listável — só perde a PRIORIDADE de rótulo.
const LOG_STOCK_ENOUGH = 12;
function isInteresting(name, inv) {
  if (!INTERESTING.test(name)) return false;
  if (/_ore$/.test(name) && !inv.hasPickaxe) return false;
  if (/_log$/.test(name) && inv.logCount >= LOG_STOCK_ENOUGH) return false;
  return true;
}

function inventorySummary(bot) {
  let hasPickaxe = false;
  let logCount = 0;
  for (const item of bot.inventory.items()) {
    if (item.name.endsWith('_pickaxe')) hasPickaxe = true;
    if (item.name.endsWith('_log')) logCount += item.count;
  }
  return { hasPickaxe, logCount };
}

const AIR_LIKE = new Set(['air', 'cave_air', 'void_air', 'water']);

// A block the bot can't act on (buried in solid rock with no exposed face)
// is noise. Exposed = at least one neighbor is air/water.
function isExposed(bot, pos) {
  const offsets = [[1, 0, 0], [-1, 0, 0], [0, 1, 0], [0, -1, 0], [0, 0, 1], [0, 0, -1]];
  for (const [dx, dy, dz] of offsets) {
    const neighbor = bot.blockAt(pos.offset(dx, dy, dz));
    if (neighbor && AIR_LIKE.has(neighbor.name)) return true;
  }
  return false;
}

function nearbyBlocks(bot) {
  const origin = bot.entity.position.floored();
  const inv = inventorySummary(bot);
  const candidates = [];
  for (let dx = -SCAN_RADIUS; dx <= SCAN_RADIUS; dx++) {
    for (let dy = -SCAN_RADIUS; dy <= SCAN_RADIUS; dy++) {
      for (let dz = -SCAN_RADIUS; dz <= SCAN_RADIUS; dz++) {
        const pos = origin.offset(dx, dy, dz);
        const block = bot.blockAt(pos);
        if (!block || AIR_LIKE.has(block.name)) continue;
        if (!isExposed(bot, pos)) continue;
        candidates.push({
          name: block.name,
          x: pos.x, y: pos.y, z: pos.z,
          dist: Math.abs(dx) + Math.abs(dy) + Math.abs(dz),
          interesting: isInteresting(block.name, inv),
        });
      }
    }
  }

  // Interesting blocks first, then by distance — and never more than
  // MAX_PER_NAME of the same type (40x "stone" tells the model nothing).
  candidates.sort((a, b) => (b.interesting - a.interesting) || (a.dist - b.dist));
  const perName = new Map();
  const blocks = [];
  for (const c of candidates) {
    const seen = perName.get(c.name) || 0;
    if (seen >= MAX_PER_NAME) continue;
    perName.set(c.name, seen + 1);
    blocks.push({ name: c.name, x: c.x, y: c.y, z: c.z });
    if (blocks.length >= MAX_BLOCKS) break;
  }
  return blocks;
}

function nearbyEntities(bot) {
  return Object.values(bot.entities)
    .filter((e) => e !== bot.entity && e.position)
    .map((e) => ({
      name: e.name || e.username || 'unknown',
      type: e.type,
      distance: bot.entity.position.distanceTo(e.position),
      x: Math.round(e.position.x),
      y: Math.round(e.position.y),
      z: Math.round(e.position.z),
    }))
    .filter((e) => e.distance <= ENTITY_RADIUS)
    .sort((a, b) => a.distance - b.distance);
}

function buildState(bot) {
  return {
    position: {
      x: bot.entity.position.x,
      y: bot.entity.position.y,
      z: bot.entity.position.z,
    },
    health: bot.health,
    food: bot.food,
    inventory: bot.inventory.items().map((i) => ({ name: i.name, count: i.count })),
    nearby_blocks: nearbyBlocks(bot),
    nearby_entities: nearbyEntities(bot),
    time_of_day: bot.time.timeOfDay,
    is_raining: bot.isRaining,
  };
}

module.exports = { buildState };
