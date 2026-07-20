// phone-bridge — a porta LAN da Alyssa (F5.1 v0, docs/plano-alyssa-v2.md).
//
// O alyssad continua SEMPRE em loopback; esta ponte é a única cara na rede:
//   celular ──HTTPS/WSS (cert auto-assinado + token)──▶ bridge ──TCP 127.0.0.1──▶ alyssad
//
// - Serve a PWA (public/) e faz proxy WebSocket <-> NDJSON.
// - Token obrigatório no WS (?token=...); gerado uma vez em bridge-token.txt.
// - QR code no console: escaneia com o celular e cai direto logado.
// - Depois do Tailscale, a MESMA ponte serve o tailnet — nada muda aqui.

const https = require("https");
const fs = require("fs");
const path = require("path");
const net = require("net");
const os = require("os");
const crypto = require("crypto");
const { WebSocketServer } = require("ws");
const selfsigned = require("selfsigned");
const qrcode = require("qrcode-terminal");

const PORT = Number(process.env.BRIDGE_PORT || 8443);
const ALYSSAD_PORT = Number(process.env.ALYSSAD_PORT || 8377);
const ALYSSAD_TOKEN = process.env.ALYSSAD_TOKEN || "";

// ---- token da ponte (persistido; BRIDGE_TOKEN sobrescreve) ----
const tokenFile = path.join(__dirname, "bridge-token.txt");
let TOKEN = process.env.BRIDGE_TOKEN || "";
if (!TOKEN) {
  if (fs.existsSync(tokenFile)) TOKEN = fs.readFileSync(tokenFile, "utf8").trim();
  if (!TOKEN) {
    TOKEN = crypto.randomBytes(8).toString("hex");
    fs.writeFileSync(tokenFile, TOKEN);
  }
}

// ---- cert auto-assinado (persistido em certs/) ----
const certDir = path.join(__dirname, "certs");
const keyPath = path.join(certDir, "key.pem");
const certPath = path.join(certDir, "cert.pem");
if (!fs.existsSync(keyPath) || !fs.existsSync(certPath)) {
  fs.mkdirSync(certDir, { recursive: true });
  const pems = selfsigned.generate(
    [{ name: "commonName", value: "alyssa-bridge" }],
    { days: 3650, keySize: 2048 });
  fs.writeFileSync(keyPath, pems.private);
  fs.writeFileSync(certPath, pems.cert);
  console.log("[bridge] cert auto-assinado gerado em certs/ (aceite o aviso no celular uma vez)");
}

// ---- estáticos da PWA ----
const PUBLIC = path.join(__dirname, "public");
const MIME = {
  ".html": "text/html; charset=utf-8",
  ".js": "text/javascript; charset=utf-8",
  ".css": "text/css; charset=utf-8",
  ".svg": "image/svg+xml",
  ".webmanifest": "application/manifest+json",
  ".png": "image/png",
};

const server = https.createServer(
  { key: fs.readFileSync(keyPath), cert: fs.readFileSync(certPath) },
  (req, res) => {
    let file = (req.url || "/").split("?")[0];
    if (file === "/") file = "/index.html";
    const full = path.join(PUBLIC, path.normalize(file));
    if (!full.startsWith(PUBLIC) || !fs.existsSync(full) || fs.statSync(full).isDirectory()) {
      res.writeHead(404); res.end("404"); return;
    }
    res.writeHead(200, { "Content-Type": MIME[path.extname(full)] || "application/octet-stream" });
    fs.createReadStream(full).pipe(res);
  });

// ---- WebSocket <-> NDJSON ----
const wss = new WebSocketServer({ server, path: "/ws" });

wss.on("connection", (ws, req) => {
  const url = new URL(req.url, "https://x");
  if (url.searchParams.get("token") !== TOKEN) {
    ws.close(4001, "token inválido");
    return;
  }

  const sock = net.createConnection({ host: "127.0.0.1", port: ALYSSAD_PORT }, () => {
    // Se o alyssad exigir auth (ALYSSAD_TOKEN), a ponte autentica por ela.
    if (ALYSSAD_TOKEN) {
      sock.write(JSON.stringify({ type: "req", id: "_bridge_auth", method: "auth",
                                  params: { token: ALYSSAD_TOKEN } }) + "\n");
    }
  });

  let buf = "";
  sock.on("data", (chunk) => {
    buf += chunk.toString("utf8");
    let nl;
    while ((nl = buf.indexOf("\n")) >= 0) {
      const line = buf.slice(0, nl).trim();
      buf = buf.slice(nl + 1);
      if (line && ws.readyState === ws.OPEN) ws.send(line);
    }
  });
  sock.on("error", () => ws.close(4002, "alyssad indisponível"));
  sock.on("close", () => ws.close(4002, "alyssad desconectou"));

  ws.on("message", (data) => {
    // uma mensagem WS = uma linha NDJSON (valida que é JSON antes de repassar)
    try { JSON.parse(data.toString()); } catch { return; }
    sock.write(data.toString() + "\n");
  });
  ws.on("close", () => sock.destroy());
});

// ---- endereço LAN + QR ----
function lanIPs() {
  const out = [];
  for (const [name, addrs] of Object.entries(os.networkInterfaces())) {
    for (const a of addrs || []) {
      if (a.family === "IPv4" && !a.internal) out.push({ name, address: a.address });
    }
  }
  // Wifi de casa primeiro (o QR usa o topo da lista): 192.168.x > 10.x >
  // resto (Radmin/Hyper-V/afins ficam por último).
  const rank = (ip) => ip.startsWith("192.168.") ? 0 : ip.startsWith("10.") ? 1 : 2;
  out.sort((a, b) => rank(a.address) - rank(b.address));
  return out;
}

server.listen(PORT, "0.0.0.0", () => {
  const ips = lanIPs();
  const ip = ips[0]?.address || "SEU-IP-LAN";
  const link = `https://${ip}:${PORT}/?t=${TOKEN}`;
  console.log(`\n[bridge] Alyssa na rede local — alyssad segue só em loopback`);
  console.log(`[bridge] token: ${TOKEN} (bridge-token.txt)`);
  for (const i of ips) console.log(`[bridge]   https://${i.address}:${PORT}/?t=${TOKEN}  (${i.name})`);
  console.log(`\n[bridge] escaneia com o celular (mesma wifi):\n`);
  qrcode.generate(link, { small: true });
  console.log("[bridge] no primeiro acesso o Chrome reclama do cert auto-assinado:");
  console.log("[bridge] Avançado → Continuar. Depois: menu ⋮ → Adicionar à tela inicial.\n");
});
