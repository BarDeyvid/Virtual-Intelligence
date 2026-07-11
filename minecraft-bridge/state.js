'use strict';

const SCAN_RADIUS = 4;   // blocks in each direction around the bot
const MAX_BLOCKS = 40;   // cap payload size
const ENTITY_RADIUS = 16;

function nearbyBlocks(bot) {
  const origin = bot.entity.position.floored();
  const blocks = [];
  for (let dx = -SCAN_RADIUS; dx <= SCAN_RADIUS && blocks.length < MAX_BLOCKS; dx++) {
    for (let dy = -SCAN_RADIUS; dy <= SCAN_RADIUS && blocks.length < MAX_BLOCKS; dy++) {
      for (let dz = -SCAN_RADIUS; dz <= SCAN_RADIUS && blocks.length < MAX_BLOCKS; dz++) {
        const pos = origin.offset(dx, dy, dz);
        const block = bot.blockAt(pos);
        if (block && block.name !== 'air') {
          blocks.push({ name: block.name, x: pos.x, y: pos.y, z: pos.z });
        }
      }
    }
  }
  return blocks;
}

function nearbyEntities(bot) {
  return Object.values(bot.entities)
    .filter((e) => e !== bot.entity)
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
