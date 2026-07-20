// Alyssa PWA — lógica: WS <-> NDJSON via bridge, chat com streaming,
// push-to-talk com o reconhecimento de voz DO ANDROID (pt-BR nativo,
// zero Whisper, zero upload de áudio).

const $ = (id) => document.getElementById(id);
const chat = $("chat"), stream = $("stream"), input = $("input");

// ---- token: chega em ?t= (QR), fica no localStorage, sai da URL ----
const params = new URLSearchParams(location.search);
if (params.get("t")) {
  localStorage.setItem("alyssa_token", params.get("t"));
  history.replaceState(null, "", location.pathname);
}
const TOKEN = localStorage.getItem("alyssa_token") || "";

// ---- helpers de UI ----
function addMsg(sender, text, cls = "", who = null) {
  const el = document.createElement("div");
  el.className = `msg ${cls}`;
  el.innerHTML = `<div class="who"></div><div class="text"></div>`;
  el.querySelector(".who").textContent = who ?? sender;
  el.querySelector(".text").textContent = text;
  chat.appendChild(el);
  chat.scrollTop = chat.scrollHeight;
  return el;
}
let toastTimer = null;
function toast(text, ms = 3500) {
  const t = $("toast");
  t.textContent = text;
  t.hidden = false;
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => { t.hidden = true; }, ms);
}

// ---- WebSocket com reconexão ----
let ws = null, msgId = 0, reconnectDelay = 1000;

function connect() {
  ws = new WebSocket(`wss://${location.host}/ws?token=${encodeURIComponent(TOKEN)}`);

  ws.onopen = () => {
    $("conn").className = "conn on";
    reconnectDelay = 1000;
    send("status");
  };

  ws.onclose = (ev) => {
    $("conn").className = "conn off";
    stream.hidden = true;
    if (ev.code === 4001) { toast("token inválido — escaneia o QR de novo"); return; }
    setTimeout(connect, reconnectDelay);
    reconnectDelay = Math.min(reconnectDelay * 2, 15000);
  };

  ws.onmessage = (ev) => {
    let m; try { m = JSON.parse(ev.data); } catch { return; }
    if (m.type === "event") return onEvent(m.event, m.data ?? {});
    if (m.type === "res" && m.ok && m.data && typeof m.data.echo === "boolean") onStatus(m.data);
    if (m.type === "res" && m.ok === false) toast(String(m.error ?? "erro"));
  };
}

function send(method, extra = {}) {
  if (!ws || ws.readyState !== WebSocket.OPEN) return false;
  ws.send(JSON.stringify({ type: "req", id: `p${++msgId}`, method, params: extra }));
  return true;
}

// ---- eventos do daemon ----
function onEvent(event, d) {
  switch (event) {
    case "state":
      if (d.phase === "thinking") {
        stream.querySelector(".text").textContent = "";
        stream.hidden = false;
        chat.scrollTop = chat.scrollHeight;
      } else {
        stream.hidden = true;
        if (d.phase === "consolidating") addMsg("system", "💤 digerindo o dia...", "system");
      }
      break;
    case "token": {
      const t = stream.querySelector(".text");
      t.textContent += d.text;
      chat.scrollTop = chat.scrollHeight;
      break;
    }
    case "response":
      stream.hidden = true;
      addMsg("Alyssa", d.text, "alyssa");
      break;
    case "user_text":
      // eco de OUTROS clientes (TUI, voz do PC...); o próprio celular já ecoou local
      if (d.client !== "celular") {
        addMsg("Deyvid", d.text, "deyvid", `Deyvid · ${d.client === "voz" ? "🎤 pc" : d.client}`);
      }
      break;
    case "hormones": onHormones(d); break;
    case "consolidation":
      addMsg("system", d.ok
        ? `dia digerido: ${d.facts ?? 0} fato(s), ${d.agenda_added ?? 0} na agenda`
        : "consolidação falhou", "system");
      break;
    case "error": toast(String(d.message ?? "erro")); break;
  }
}

function onStatus(s) {
  if (s.hormones) onHormones(s.hormones);
  if (typeof s.ambient === "string") $("ambient").textContent = s.ambient;
}
function onHormones(h) {
  if (h.emotional_state) $("mood").textContent = h.emotional_state;
  const set = (id, v) => $(id).style.setProperty("--v", String(Math.max(0, Math.min(1, v ?? 0))));
  set("h-cor", h.cortisol); set("h-dop", h.dopamine); set("h-oxi", h.oxytocin);
  set("h-ser", h.serotonin); set("h-adr", h.adrenaline);
}

// status fresco de tempos em tempos (ambient/hormônios sem conversar)
setInterval(() => send("status"), 20000);

// ---- enviar texto ----
function submit() {
  const text = input.value.trim();
  if (!text) return;
  if (!send("say", { text, client: "celular" })) { toast("sem conexão"); return; }
  addMsg("Deyvid", text, "deyvid local");
  input.value = "";
}
$("send").addEventListener("click", submit);
input.addEventListener("keydown", (e) => { if (e.key === "Enter") submit(); });

// ---- push-to-talk: SpeechRecognition do Android (pt-BR) ----
const SR = window.SpeechRecognition || window.webkitSpeechRecognition;
const ptt = $("ptt");
let rec = null, recActive = false;

function startPTT() {
  if (!SR) { toast("sem reconhecimento de voz neste navegador"); return; }
  if (recActive) return;
  rec = new SR();
  rec.lang = "pt-BR";
  rec.interimResults = true;
  rec.continuous = false;

  let finalText = "";
  rec.onresult = (ev) => {
    let interim = "";
    for (const r of ev.results) {
      if (r.isFinal) finalText += r[0].transcript;
      else interim += r[0].transcript;
    }
    input.value = finalText + interim;
  };
  rec.onerror = (ev) => {
    if (ev.error === "not-allowed") toast("permita o microfone pra falar com ela");
    else if (ev.error !== "aborted" && ev.error !== "no-speech") toast(`voz: ${ev.error}`);
  };
  rec.onend = () => {
    recActive = false;
    ptt.classList.remove("listening");
    const text = input.value.trim();
    if (text) { input.value = text; submit(); }
  };

  recActive = true;
  ptt.classList.add("listening");
  input.value = "";
  rec.start();
}
function stopPTT() { if (recActive && rec) rec.stop(); }

// Segurar fala, soltar envia (toque rápido também funciona: o SR
// continua até a primeira pausa e envia sozinho no onend).
ptt.addEventListener("pointerdown", (e) => { e.preventDefault(); startPTT(); });
ptt.addEventListener("pointerup", stopPTT);
ptt.addEventListener("pointercancel", stopPTT);

// ---- service worker (instalável) ----
if ("serviceWorker" in navigator) navigator.serviceWorker.register("sw.js").catch(() => {});

addMsg("system", "conectando na Alyssa...", "system");
connect();
