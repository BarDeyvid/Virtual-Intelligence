# alyssad — protocolo do daemon (v0.2)

`alyssad` é o cérebro da Alyssa rodando headless: CoreIntegration + (opcional)
Kokoro TTS atrás de um socket TCP local. Frontends (TUI em TS/Ink, web, o que
vier) conectam nele e conversam por **NDJSON** — uma linha, um objeto JSON —
o mesmo idioma do `minecraft-bridge`, com os papéis invertidos (aqui o C++ é
o servidor).

```
alyssad [--echo] [--voice] [--port N]
```

- **`--echo`**: modo de teste do protocolo — NÃO carrega modelo nenhum
  (zero VRAM, boot instantâneo). `say` responde um eco. Serve pra desenvolver
  o frontend sem pagar o load de 5GB a cada iteração.
- **`--voice`**: instancia o Kokoro (blend pf_dora+af_bella). O load real do
  ONNX é lazy — só acontece na primeira frase falada.
- **`--port N`** (ou env `ALYSSAD_PORT`): porta TCP. Default **8377**, sempre
  em `127.0.0.1` (nunca exposto pra rede).

**v0.2 (F5.0): múltiplos clientes simultâneos** — TUI e celular conectados ao
mesmo tempo. Turnos continuam serializados (`say` enquanto pensa devolve
`busy`), e **eventos são broadcast** pra todos os clientes conectados: quem
não pediu o turno vê o mesmo streaming (`res` vai só pro requisitante).

**Auth (opcional):** com a env `ALYSSAD_TOKEN` setada no daemon, todo método
além de `ping`/`auth` exige autenticar primeiro:
```json
{"type":"req","id":0,"method":"auth","params":{"token":"..."}}
```
→ `{"ok":true,"data":{"authed":true}}` (ou `token inválido`). Sem a env,
auth é opcional e no-op. É o preparo pro bind no tailnet (F5): o 8377 segue
SEMPRE em loopback até o Tailscale entrar em cena.

## Envelopes

Cliente → daemon (requests):
```json
{"type":"req","id":1,"method":"say","params":{"text":"oi"}}
```

Daemon → cliente (resposta da request, mesmo `id`):
```json
{"type":"res","id":1,"ok":true,"data":{...}}
{"type":"res","id":1,"ok":false,"error":"busy"}
```

Daemon → cliente (eventos, empurrados a qualquer momento):
```json
{"type":"event","event":"state","data":{"phase":"thinking"}}
```

`id` é opaco pro daemon (int ou string, o cliente escolhe e ele devolve).

## Métodos

### `ping`
→ `{"ok":true,"data":{"pong":true}}`. Keep-alive / teste de conexão.

### `status`
→ snapshot:
```json
{"ok":true,"data":{
  "echo": false,
  "busy": false,
  "voice_available": true,
  "hormones": {"cortisol":0.2,"dopamine":0.5,"oxytocin":0.3,"serotonin":0.4,"adrenaline":0.1},
  "emotional_state": "calm",
  "ambient": "sexta, 22:14 · janela ativa: \"...\" · usuário presente · CPU 12% · RAM 54%"
}}
```
`ambient` só vem quando o cérebro está ocioso (a fonte de métricas não é
thread-safe contra a inferência). `hormones`/`emotional_state` só fora do
modo echo.

### `say`
`params`: `{"text": "...", "tts": true|false}` — `tts` é opcional; default é
falar quando o daemon subiu com `--voice`. Pedir `tts:true` sem `--voice`
degrada pra texto (o `response` diz o que aconteceu de fato).

Resposta imediata: `{"ok":true,"data":{"accepted":true}}` (ou `busy`).
O turno roda em background e emite, nesta ordem:

1. `event state` — `{"phase":"thinking"}`
2. `event token` — `{"text":"pedaço"}`, zero ou mais vezes, conforme a
   resposta é gerada. O cliente acumula os pedaços pra exibir streaming.
3. `event response` — `{"text":"...","latency_ms":1234,"tts":false}`. O
   `text` é a resposta FINAL (pós tool calls e strip de `[RESPOSTA]`) e
   **substitui** o que foi acumulado via `token` — os pedaços crus podem
   conter marcações intermediárias.
4. `event hormones` — perfil pós-turno (mesmo formato do `status`)
5. `event state` — `{"phase":"idle"}`

No modo `--echo` o streaming é simulado palavra a palavra (~30ms), pra
desenvolver a UI de token sem carregar modelo.

Falha no turno emite `event error` (`{"message":"..."}`) antes do
`state idle`.

### `consolidate` (v0.2 — F3)
Dispara a consolidação noturna manualmente (mesmo guard `busy` dos turnos).
Sem `params`. Resposta imediata `{"accepted":true}` (ou `busy` / erro no modo
echo); ao terminar emite:

1. `event state` — `{"phase":"consolidating"}`
2. `event consolidation` — stats: `{"ok",summary_chars,facts,agenda_added,
   reflection,opinions_dropped,pruned,ms}` (ou `{"ok":false,"error":...}`)
3. `event state` — `{"phase":"idle"}`

O disparo AUTOMÁTICO acontece no daemon: a partir das 04:00 locais, com 30min
de silêncio e no máximo uma vez por dia (gate em
`self.last_consolidation_date` do state/self.json). O `status` expõe um
resumo do self: `{"self":{opinions,goals,agenda,last_consolidation_date,
has_yesterday_summary}}`.

### `listen` (v0.2 — voice-in)
`params`: `{"enabled": true|false}`. Liga/desliga o OUVIDO dela: mic → VAD
(Silero) → Whisper turbo → o transcript vira um turno normal. O modelo
Whisper carrega lazy no primeiro on e SAI da VRAM no off. Broadcast
`event listening {enabled}` a cada mudança; `status` expõe `voice_in`.
Cada enunciado emite `event heard {text}` e, se o cérebro estiver livre, um
turno com `event user_text {text, client:"voz"}` + o streaming normal.
Ocupada = descarta com `event error` ("ouvi mas estava ocupada").

### `shutdown`
→ `{"ok":true}`. Espera o turno em andamento terminar e encerra o processo.

## Eventos

| event      | data                                   | quando                         |
|------------|----------------------------------------|--------------------------------|
| `state`    | `{"phase":"idle"\|"thinking"\|"consolidating"}` | em volta de cada turno/ciclo |
| `token`    | `{"text"}`                             | cada pedaço gerado da resposta |
| `response` | `{"text","latency_ms","tts"}`          | resposta pronta (substitui os tokens) |
| `hormones` | `{cortisol,...,"emotional_state"}`     | após cada turno                |
| `consolidation` | stats do ciclo                    | consolidação terminou          |
| `user_text` | `{"text","client"}`                   | say aceito (dedup: ignore o próprio `client`) |
| `heard`    | `{"text"}`                             | voice-in transcreveu um enunciado |
| `listening`| `{"enabled"}`                          | voice-in ligado/desligado      |
| `error`    | `{"message"}`                          | turno falhou / request inválida|

O `status.self` (F2/F3) traz os ITENS: `opinions[{topic,stance,confidence}]`,
`goals[{desc,progress,priority}]`, `agenda[{bring_up,reason}]`,
`yesterday_summary`, `last_consolidation_date` — a aba 🪞 Self da TUI.

## Reservado (v0.2+, nomes já fixados pra não quebrar cliente)

- `event proactive` — mensagens espontâneas (mover o loop do ProactivityEngine
  dos CLIs pra dentro do daemon).
- `event presence` / `event vram` / `event log` — painéis de percepção,
  scheduler e logs estruturados.
- `method interrupt` — cortar a geração no meio (precisa de flag de cancel
  no AlyssaCore antes).
- `method listen` — voice-in (Whisper/VAD) controlado por protocolo; é o
  passo que aposenta o Alyssa_CLI_WITH_VOICE de vez.

## Exemplo de sessão (modo echo)

```
C: {"type":"req","id":1,"method":"ping"}
S: {"type":"res","id":1,"ok":true,"data":{"pong":true}}
C: {"type":"req","id":2,"method":"say","params":{"text":"oi Alyssa"}}
S: {"type":"res","id":2,"ok":true,"data":{"accepted":true}}
S: {"type":"event","event":"state","data":{"phase":"thinking"}}
S: {"type":"event","event":"response","data":{"text":"[eco] oi Alyssa","latency_ms":0,"tts":false}}
S: {"type":"event","event":"state","data":{"phase":"idle"}}
C: {"type":"req","id":3,"method":"shutdown"}
S: {"type":"res","id":3,"ok":true,"data":{}}
```
