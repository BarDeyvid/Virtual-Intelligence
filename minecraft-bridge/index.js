'use strict';

const net = require('net');
const mineflayer = require('mineflayer');
const { pathfinder, Movements } = require('mineflayer-pathfinder');

const { buildState } = require('./state');
const { executeAction } = require('./actions');

const MC_HOST = process.env.MC_HOST || 'localhost';
const MC_PORT = Number(process.env.MC_PORT || 25565);
const MC_USERNAME = process.env.MC_USERNAME || 'Alyssa';
const BRIDGE_PORT = Number(process.env.BRIDGE_PORT || 8765);

const bot = mineflayer.createBot({
  host: MC_HOST,
  port: MC_PORT,
  username: MC_USERNAME,
});

bot.loadPlugin(pathfinder);

let lastHealth = null;
const sockets = new Set();

function broadcastEvent(event, data) {
  const line = JSON.stringify({ type: 'event', event, data }) + '\n';
  for (const socket of sockets) {
    socket.write(line);
  }
}

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
  console.log(`[minecraft-bridge] spawned as ${MC_USERNAME} on ${MC_HOST}:${MC_PORT}`);
});

bot.on('health', () => {
  if (lastHealth !== null && bot.health < lastHealth) {
    broadcastEvent('damage', { amount: lastHealth - bot.health, health: bot.health });
  }
  lastHealth = bot.health;
});

bot.on('death', () => {
  broadcastEvent('death', { position: bot.entity.position });
});

bot.on('chat', (username, message) => {
  if (username === bot.username) return;
  broadcastEvent('chat', { username, message });
});

bot.on('kicked', (reason) => console.error('[minecraft-bridge] kicked:', reason));
bot.on('error', (err) => console.error('[minecraft-bridge] bot error:', err));

// --- Local JSONL socket for the C++ core (AlyssaNet's MinecraftBridge) ---

function handleMessage(socket, msg) {
  if (msg.type === 'get_state') {
    socket.write(JSON.stringify({ type: 'state', data: buildState(bot) }) + '\n');
    return;
  }
  if (msg.type === 'action') {
    executeAction(bot, msg.verb, msg.args || []).then((result) => {
      socket.write(JSON.stringify({ type: 'action_result', ...result }) + '\n');
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
