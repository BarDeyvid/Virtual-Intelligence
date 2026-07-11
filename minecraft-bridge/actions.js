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

async function mineBlock(bot, [x, y, z]) {
  const [xi, yi, zi] = [x, y, z].map(Number);
  if (!withinReach(bot, xi, yi, zi)) {
    return { ok: false, message: `target (${xi},${yi},${zi}) is farther than ${MAX_REACH} blocks` };
  }
  const block = bot.blockAt(new Vec3(xi, yi, zi));
  if (!block || block.name === 'air') {
    return { ok: false, message: `no minable block at (${xi},${yi},${zi})` };
  }
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

async function attack(bot, [targetName]) {
  const target = Object.values(bot.entities).find(
    (e) => e.name === targetName || (e.mobType && e.mobType.toLowerCase() === targetName.toLowerCase())
  );
  if (!target) {
    return { ok: false, message: `no visible entity named ${targetName}` };
  }
  if (bot.entity.position.distanceTo(target.position) > 4) {
    return { ok: false, message: `${targetName} is out of melee range` };
  }
  bot.attack(target);
  return { ok: true, message: `attacked ${targetName}` };
}

async function craft(bot, [itemName, countRaw]) {
  const count = Number(countRaw) || 1;
  const item = bot.registry.itemsByName[itemName];
  if (!item) {
    return { ok: false, message: `unknown item ${itemName}` };
  }
  const recipe = bot.recipesFor(item.id, null, 1, null)[0];
  if (!recipe) {
    return { ok: false, message: `no known recipe for ${itemName} (missing table or ingredients)` };
  }
  await bot.craft(recipe, count, null);
  return { ok: true, message: `crafted ${count}x ${itemName}` };
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
