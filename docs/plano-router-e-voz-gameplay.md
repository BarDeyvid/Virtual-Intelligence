# Plano: Router Adaptativo (A) + Voz direto no Gameplay (B)

Aprovado pelo Deyvid em 2026-07-12 ("vamo nos dois"). Executar A primeiro
(menor, ganho imediato de latência), depois B. Contexto de decisões:
docs/gemma4-migration.md + memória night-session-2026-07-12.

---

## A. Router Adaptativo — E2B decide QUANDO consultar o comitê

**Ideia**: hoje todo turno não-trivial paga o comitê inteiro (~1s+ dos ~2s).
Vira computação adaptativa: um pre-pass barato decide FAST / DIRETO /
CONSULTAR{experts}. A E2B responde sozinha na maioria dos casos; comitê só
quando agrega.

### A1. Grammar do router — `config/grammars/router.gbnf` (novo)
```
root ::= "[ROTA] " rota "\n"
rota ::= "direto" | "emocional" | "analitico" | "memoria" | "criativo" | "comite"
```
~10 tokens de saída → ~150ms na E2B (78 t/s) + prefill curto (~300 tokens de
system enxuto, sem persona) ≈ **~300-400ms de overhead** vs ~1s+ de comitê.

### A2. Config — `config/ConfigsLLM.json`
- Novo bloco `"router"` (ou entry no models com id "router"): reusa o
  MESMO core da alyssa (E2B), só muda system_prompt + grammar_file +
  max_tokens 12. NÃO carregar outro modelo.
- Flag global `"router_mode": "committee" | "adaptive"` — rollback de 1 linha.

### A3. AlyssaNet — `think_with_fusion_core` (AlyssaNet.cpp ~1620)
- Antes do bloco do comitê: se `router_mode==adaptive` e não caiu no fast
  path, roda o pre-pass (run_expert no core da alyssa com grammar do router;
  ATENÇÃO: clear_own_kv_cache antes e depois — o run() re-prefila tudo).
- `direto` → pula pro caminho [MODO DIRETO] já existente (~linha 1848).
- `emocional/analitico/criativo` → roda SÓ esse(s) expert(s) do 1B + fusão.
- `memoria` → NÃO consulta memoryModel: usa embedder + AlyssaMemoryHandler
  (busca semântica já existe) e injeta os hits direto no prompt final. O 1B
  "lembrando" é a parte mais fraca do comitê hoje.
- `comite` → fluxo atual completo (fallback de segurança).
- Logar cada decisão: `logs/router_decisions.jsonl` {ts, input_len, rota,
  latency_total_ms} — pra avaliar depois se o router acerta.

### A4. Critérios de aceite
- 6 turnos do bench (scratchpad bench_turns.mjs) com GPU ociosa:
  mediana do turno completo cai de ~2s pra ≤1.2s nos casos "direto".
- Nenhuma regressão de persona (mesma verificação de sempre: responde a
  pergunta, sem assistentês, sem 3ª pessoa).
- `router_mode: committee` devolve o comportamento antigo byte a byte.

---

## B. Voz direto no gameplayModel (Minecraft ouve o Deyvid)

**Ideia**: enunciado do mic vira `<__media__>` no prompt do tick — UMA
inferência E2B faz ASR+decisão, grammar continua garantindo [AÇÃO] válida.
Whisper fica intocado no chat (transcript pra memória SQLite).

### B1. VoicePipeline: modo VAD-only (VoicePipeline.hpp/.cpp)
- `Options.vad_only = false` (novo): quando true, `_whisper_worker_func` NÃO
  transcreve (nem carrega Whisper) — só dispara `on_segment(audio, "", 0)`.
  O gameplay não precisa de texto, só do áudio cortado.

### B2. AlyssaCore: geração com áudio (AlyssaCore.hpp)
- Extrair o loop de geração (linhas ~358-401) num método privado
  `_generation_loop(smpl, params, stream_callback)` reutilizável.
- Novo método `generate_with_audio(prompt_text, audio_16k, params, ...)`:
  1. `mtmd_init_from_file(mmproj_path, model, ...)` — lazy, 1x, guardado
     como membro (`mtmd_ctx`); mmproj path entra por setter/config.
  2. `mtmd_tokenize` do texto com marcador `<__media__>` + bitmap do áudio.
  3. `mtmd_helper_eval_chunks` no ctx (substitui o llama_decode do prefill).
  4. Mesmo `_generation_loop` de sempre (grammar idem).
- **Template**: usar o formato `<|turn>` do format_gemma4_prompt (ExpertBase)
  — NUNCA o template do GGUF (minja 0xC0000409) nem o "gemma" do Gemma 3
  (devolve vazio em áudio real — provado no teste de sotaque).

### B3. CMake + DLLs
- Include: `llama.cpp/tools/mtmd`. Link: `mtmd.lib` (em
  build_cuda/tools/mtmd/Release/). O copy_directory já leva mtmd.dll junto.
- **RISCO CONHECIDO**: build/Release roda com ggml do whisper.cpp
  (sobrescrito — ver forense em benchmarks-2026-07-11.md). mtmd.dll foi
  buildado contra o ggml do llama.cpp. Se der 0xC0000409 silencioso no app:
  sincronizar os snapshots de ggml dos dois submódulos (hardening que uma
  hora vai ter que acontecer de qualquer jeito).

### B4. MinecraftSession (MinecraftSession.hpp/.cpp)
- `set_pending_voice(std::vector<float> audio)` (thread-safe, 1 slot;
  enunciado novo substitui o antigo — comando falado é perecível).
- No tick_loop: se tem áudio pendente, prompt ganha
  `"[VOZ DO JOGADOR] <__media__>\n"` e o tick roda via caminho com áudio
  (novo `run_gameplay_tick_audio(prompt, audio)` no CoreIntegration →
  run_expert variante → generate_with_audio). Sem áudio → fluxo atual.
- Fiação no Alyssa_CLI_WITH_VOICE: com sessão MC ativa, segmentos do VAD vão
  pro set_pending_voice EM VEZ do Whisper→chat (PoC; depois pode virar
  "os dois").

### B5. Teste sem servidor Minecraft — `tests/test_gameplay_audio.cpp`
- Alimenta WAV real (asr_ab_wavs/ tem os do Deyvid!) + world state fake com
  rótulos (B1=oak_log etc.) → espera [AÇÃO] válida da grammar.
- Casos: "vai até a árvore" → mover B1; "ataca o zumbi" → atacar E1;
  silêncio/ruído → esperar (não pode alucinar ação).

### B6. Critérios de aceite
- test_gameplay_audio: 3 casos acima passando com WAVs de mic reais.
- Tick com voz ≤ 2s no total (áudio encode ~300ms + prefill + 80 tokens).
- VRAM com MC ligado: medir de novo (esperado ~+0.6GiB do mmproj).

---

## C. Camada de reflexos + destilação futura (ideia Gemini/Deyvid, 2026-07-12)

Arquitetura híbrida reflexo/estrategista — versão pragmática:

### C1. Reflexos determinísticos no bridge (AGORA, zero modelo)
Em `minecraft-bridge/index.js`, um loop de ~250ms local:
- Mob hostil a <3 blocos → `bot.attack` (se vida ok) ou fugir na direção
  oposta (setGoal) — sem esperar o tick do LLM.
- Vida <6/20 → fugir de qualquer hostil + comer se tiver comida.
- Fome <6/20 e comida no inventário → comer.
- Broadcast `event reflex` pro C++ logar (GameplayLog) — o estrategista
  (E2B) fica sabendo o que o reflexo fez via prompt do próximo tick
  ("[REFLEXO] fugi do zombie") pra não decidir às cegas.
- Reflexo tem PRIORIDADE sobre ação pendente do LLM (cancela pathfinder).

### C2. Destilação 270M (Aluno) — FASE FUTURA, gate de dados
- Professor = E2B jogando (só começou a funcionar de verdade com o fix de
  template de 2026-07-12 — destilar antes de ter gameplay BOA = destilar
  incompetência).
- Dataset já se acumula sozinho: GameplayLog grava state→prompt→signal→
  result de cada tick. Quando houver ~10k+ ticks de qualidade (filtrar por
  action_result ok + sobrevivência), treinar gemma-3-270m com LoRA no
  formato [ESTADO]→[AÇÃO] (a grammar garante o formato do dataset).
- Aluno rodaria o tick a cada ~1s com latência ~50ms; E2B vira o "diretor"
  chamado a cada N ticks ou em eventos (chat, morte, objetivo novo).
- Gate: NÃO começar antes do dataset existir. Revisitar quando
  logs/gameplay*.jsonl passar de ~10k ticks bons.

## Ordem de execução e estado atual — EXECUTADO 2026-07-12

1. [x] A1-A4: router adaptativo LIGADO (`router_mode: adaptive`). Pre-pass no
   1B ~500ms; turnos roteados 2.2s→1.3s. Decisões auditáveis em
   logs/router_decisions.jsonl. Rollback: `router_mode: "committee"`.
2. [x] B1: `Options.vad_only` no VoicePipeline.
3. [x] B2-B3: `AlyssaCore::init_audio/generate_with_audio` em
   includes/AlyssaCoreAudio.cpp (mtmd.lib no COMMON_LIBS).
4. [x] B4-B5: `set_pending_voice` + `run_gameplay_tick_audio` +
   `alyssa_cli_with_voice --minecraft` (mic→Gaia). test_gameplay_audio:
   "ataca o zumbi"→atacar E1, "vai até a árvore"→mover B1, fala aleatória→
   ação válida autônoma.
5. [x] C1: reflexos no bridge (250ms: fugir/atacar/comer) + [REFLEXO] no
   prompt + adrenalina no endocrine.
6. Commits: NADA commitado ainda (Deyvid revisa).

**DESCOBERTA GRANDE do aceite B6**: o sampler de GRAMMAR custava ~70ms/token
no vocab de 262K do Gemma 4 — tick de gameplay levava 4-5s POR CAUSA DELA.
Com o turn format correto o E2B segura o [AÇÃO] sozinho: grammar removida do
gameplayModel (ConfigsLLM.json), tick de voz caiu pra **0.65-1.0s** (6x) e o
ActionExecutor já ignora malformado. max_tokens do gameplay 80→48. A grammar
(config/grammars/gameplay_action.gbnf) fica no repo como rollback.

Como testar ao vivo: `node index.js` no minecraft-bridge, depois
`alyssa_cli_with_voice --minecraft` e falar comandos no mic.
