# AlyssaNet TODO & Feature Roadmap

## 🎯 High Priority - Core Improvements

### Phase 1: Tool System & Proactivity (Foundation)

#### 1.1 Simple Tool Registry System ✅ DONE
- **Goal**: Allow tools without exploding context; easy to maintain
- **Approach**: Create `config/tools_registry.json` with:
  ```json
  {
    "tools": [
      {
        "name": "screenshot",
        "description": "Capture current screen",
        "params": [
          {"name": "filename", "type": "string", "required": false}
        ]
      },
      {
        "name": "read_file",
        "description": "Read file contents",
        "params": [
          {"name": "path", "type": "string", "required": true},
          {"name": "max_lines", "type": "int", "required": false, "default": 100}
        ]
      },
      {
        "name": "web_search",
        "description": "Search web for info",
        "params": [
          {"name": "query", "type": "string", "required": true}
        ]
      }
    ]
  }
  ```
- **Implementation**: 
  - Load registry on startup
  - Add compact tool descriptions to expert prompts (not full context)
  - Create ToolExecutor class to dispatch calls
- **Files**: `tools_registry.json`, `ToolExecutor.hpp/cpp`
- **Effort**: 4-6h

#### 1.2 Tool Execution & Response Integration ✅ DONE
- **Goal**: Alyssa can invoke tools and use results in responses
- **Approach**:
  - Parse LLM output for `[TOOL_CALL] name(param1=value1) [/TOOL_CALL]` blocks
  - Execute via ToolExecutor
  - Feed results back to LLM for context-aware response
  - Handle timeouts & failures gracefully
- **Files**: Modify `CoreIntegration.cpp`, add tool parsing
- **Effort**: 5-7h

---

### Phase 2: Personality & Proactivity

#### 2.1 Personality System ✅ DONE
- **Goal**: Move from generic AI to "normal girl personality"
- **Approach**:
  - Define personality traits in `config/personality.json`:
    ```json
    {
      "name": "Alyssa",
      "traits": {
        "friendliness": 0.8,
        "curiosity": 0.9,
        "sass": 0.6,
        "energy": "variable",
        "interests": ["technology", "anime", "philosophy"]
      },
      "speech_patterns": {
        "use_contractions": true,
        "emoji_frequency": 0.4,
        "humor_style": "witty_sarcasm"
      }
    }
    ```
  - Store hormones + personality in system prompt
  - Experts reference personality for tone adjustment
- **Files**: `config/personality.json`, update expert prompts
- **Effort**: 3-4h (mostly prompt tuning)

#### 2.2 Proactive Behavior ✅ DONE (parcial: gatilho de visão fica pra Fase 3)
- **Goal**: Alyssa initiates conversation when bored/sees interesting things
- **Approach**:
  - Add `ProactivityEngine` that triggers every N seconds (configurable, e.g., 60s)
  - Checks:
    - **Boredom**: If no input for 5+ min + endocrine state allows → generate observation/question
    - **Interesting Event**: Vision detects change (new window, file appeared) → comment on it
    - **Mood-based**: High dopamine → suggest activity; high cortisol → check on user
  - Examples of proactive outputs:
    - "Hey, I noticed you've been quiet... what's up?"
    - "Dude, did you see that? That notification looked important"
    - "I'm bored, wanna talk about something interesting?"
- **Files**: `ProactivityEngine.hpp/cpp`, modify `Alyssa_CLI.cpp` main loop
- **Effort**: 6-8h

---

## 🔧 Medium Priority - Architecture & Performance

### Phase 3: Vision & Latency

#### 3.1 Vision Caching + Change Detection ✅ DONE
- **Goal**: Reduce 500ms latency by only reprocessing changed frames
- **Implementado**: classe `FrameCache` em VisionEnhancer.hpp/cpp + `FovealVision::analyze_cached()`. Insight-chave: o resultado foveal depende SÓ do crop 64×64 ao redor do cursor, então o diff parcial compara apenas essa região — mudança no resto da tela não invalida o cache (só atualiza `last_global_change_ratio()`, que é o hook pro gatilho de proatividade por visão). Cursor com jitter < 32px mantém o cache. Bench sintético: caminho cacheado ~11x mais rápido. `VisionEnhancer.cpp` agora está no COMMON_SOURCES (antes nem compilava).
- **Approach**:
  - Store frame hash + processed data
  - Compare new frame hash; skip processing if identical
  - For small changes: partial processing (detect diffs only)
  - Reprocess fully only if change > threshold (50 pixel diff?)
- **Expected gain**: ~60-70% latency reduction for static scenes
- **Files**: `VisionEnhancer.hpp/cpp`, new `FrameCache` class
- **Effort**: 5-6h

#### 3.2 Expert Parallelization ✅ DONE
- **Goal**: Run 3 experts in parallel instead of sequential
- **Implementado**: pool de 3 contextos llama.cpp sobre o MESMO modelo 1B (sem duplicar pesos em RAM/VRAM; `AlyssaCore` ganhou construtor de contexto compartilhado). Experts do comitê rodam via `std::async` em lotes do tamanho do pool, cada task com contexto exclusivo + `clear_kv()`. `Embedder` ganhou mutex interno (contexto/batch únicos não eram thread-safe). Fallback automático pro modo sequencial se o pool falhar na criação (ex.: sem VRAM). Log `[MoE Execution] Comitê ... concluído em Xms (paralelo|sequencial)` pra medir o ganho real.
- **Approach**:
  - Use thread pool or async tasks for emotionalModel, analyticalModel, memoryModel
  - Gather results with `std::future`
  - Maintain `brain_mtx` for final LLM call (keep serialized)
- **Expected gain**: ~2-3x faster inference (if expert models are small)
- **Files**: `WeightedFusion/WeightedFusion.hpp/cpp`, add ThreadPool
- **Effort**: 4-5h

---

### Phase 4: Memory & Context Management

#### 4.1 Dynamic Context Window Sizing
- **Goal**: Adjust `n_ctx` based on actual usage instead of fixed 8192
- **Approach**:
  - Track token usage per conversation segment
  - If avg usage < 4000 → reduce to 4096 (faster)
  - If avg usage > 6000 → keep at 8192
  - Adjust `SimpleModelConfig` on session start
- **Files**: `CoreIntegration.cpp`, add token counter
- **Effort**: 2-3h

#### 4.2 Memory Compression & Long-Term Context ✅ DONE
- **Goal**: Keep conversation history without context bloat
- **Implementado**: resumo ROLANTE em `manage_dynamic_history()` — quando o histórico passa do limite (50 msgs), as 8 mensagens mais antigas são (a) arquivadas brutas na LTM (comportamento original) e (b) resumidas pelo modelo 1B (pool slot 0, timeout 15s) num bloco `[RESUMO DA CONVERSA ANTERIOR]` reinjetado no início do histórico. Resumo anterior entra no novo resumo (um bloco carrega toda a história). Falha no resumo degrada pro comportamento antigo sem perder o resumo prévio. Resumos também vão pra LTM com tag `conversation_summary`.
- **Approach**:
  - Store full history in SQLite
  - Periodically (every 50 messages) summarize older messages via LLM
  - Replace old messages with: `[SUMMARY] date: msg1 + msg2 + msg3 → summary_text [/SUMMARY]`
  - On context load: include summaries, only full recent messages
- **Files**: `AlyssaMemoryHandler.cpp`, add summarization logic
- **Effort**: 6-7h

#### 4.3 Multi-Model Fallback & Resilience ✅ DONE (adaptado)
- **Goal**: Load 2-3 models; use smaller/faster if primary timeouts
- **Implementado (sem carregar modelos extras — o 1B já carregado é o fallback)**:
  1. **Timeout de geração**: `SimpleModelParameters.timeout_ms` (config `parametros.timeout_ms`; Alyssa = 45s) — `generate_raw()` devolve resposta parcial em vez de travar a UI.
  2. **Fallback de load**: 4B falhou ao carregar → Alyssa roda no modelo 1B compartilhado (contexto extra, qualidade reduzida, sistema vivo).
  3. **Fallback de resposta**: resposta vazia/erro do 4B → `generate_fallback_response()` tenta o 1B; falhou tudo → mensagem fixa de desculpa. Eventos logados com `[Fallback]`.
- **Approach**:
  - Load model list: `["gemma-3-4b-it-q4_0.gguf", "mistral-7b-q4_0.gguf", "tinyllama-q4_0.gguf"]`
  - Try primary (largest), timeout after 10s
  - Fall back to next model if timeout
  - Log fallback events for debugging
- **Files**: `CoreIntegration.hpp/cpp`, modify model loading
- **Effort**: 4-5h

---

## ✨ Nice-to-Have - UX & Observability

### Phase 5: UI & Debugging

#### 5.1 Hormone Timeline + Event Annotations
- **Goal**: Visual history of endocrine state changes
- **Approach**:
  - Store hormone samples every 10s in memory
  - Add UI tab "Hormones Timeline": line graph (last 30min)
  - Mark events: "harsh input", "good response", "boredom spike"
  - Color-code by dominant hormone
- **Files**: `Alyssa_CLI.cpp`, new UI component
- **Effort**: 5-6h

#### 5.2 Tool Call Logging & Replay
- **Goal**: Debug tool execution; see what tools were called and results
- **Approach**:
  - Log all tool calls: `[TOOL_CALL] name(params) → result [/TOOL_CALL]`
  - UI tab "Tool Calls": list with timestamps, status, results preview
  - Export logs to JSON
- **Files**: `Alyssa_CLI.cpp`, modify ToolExecutor
- **Effort**: 3-4h

#### 5.3 Personality Profile Display
- **Goal**: Show user what personality mode Alyssa is in
- **Approach**:
  - UI corner: current personality traits (sass level 6/10, energy: tired, interests: tech)
  - Update dynamically based on hormones + time of day
- **Files**: `Alyssa_CLI.cpp`, personality rendering
- **Effort**: 2-3h

---

### Phase 6: Advanced MoE Features

#### 6.1 Expert Specialization by Question Type
- **Goal**: Route questions to relevant experts; reduce redundancy
- **Approach**:
  - Classify input: "factual" / "emotional" / "creative" / "meta"
  - Select subset of experts:
    - Factual → memoryModel only
    - Emotional → emotionalModel + memoryModel
    - Creative → analyticalModel + emotionalModel
    - Meta → all
  - Reduces processing overhead
- **Files**: `CoreIntegration.cpp`, add QueryClassifier
- **Effort**: 4-5h

#### 6.2 Adaptive MoE Weights (Self-Tuning)
- **Goal**: Weights learn from user feedback
- **Approach**:
  - Track response acceptance (user continues vs. rejects)
  - If user response is "good" (replied positively) → boost active experts' weights
  - If "bad" (corrected, ignored) → reduce weights
  - Use exponential moving average: `w_new = 0.9 * w_old + 0.1 * adjustment`
  - Log weight evolution for debugging
- **Files**: `WeightedFusion/WeightedFusion.hpp/cpp`
- **Effort**: 5-6h

#### 6.3 Expert Confidence Scoring
- **Goal**: Know when expert outputs are unreliable
- **Approach**:
  - Experts output structured: `[SINAL] value [CONFIANÇA] 0.8 [CONTEXTO] details [/SINAL]`
  - WeightedFusion uses confidence as multiplier
  - Low confidence (< 0.3) → override with safer fallback
  - Log low-confidence signals for retraining
- **Files**: `ExpertBase.hpp`, `WeightedFusion.cpp`
- **Effort**: 2-3h

---

## 🚀 Quick Wins (1-2h each)

- **[ ] Config Hot-Reload**: Watch `config/` files; reload on change without restart
- **[ ] Silence Detection**: Skip LLM if input is just noise (whisper confidence < 0.3)
- **[ ] Health Check Endpoint**: Simple HTTP endpoint `/health` → JSON status
- **[ ] Prompt Templates**: Move all system prompts to `config/prompts.json` (easy tuning)
- **[ ] Voice Streaming**: Start TTS while LLM still generating (chunked output)
- **[ ] Usage Stats**: Count tokens/latency per session; log to JSON
- **[ ] Input Validation**: Sanitize tool params before execution
- **[ ] Error Recovery**: Graceful degradation if LLM/tools fail (fall back to generic response)

---

## 📋 Execution Order (Recommended)

### Week 1-2: Foundation (Phases 1-2)
1. ✅ Tool Registry + Execution (1.1, 1.2)
2. ✅ Personality System (2.1)
3. ✅ Proactivity Engine (2.2)
- **Result**: Alyssa can use tools, has personality, talks first

### Week 3: Performance (Phase 3)
4. ✅ Vision Caching (3.1)
5. ✅ Expert Parallelization (3.2)
- **Result**: 3x faster inference, less vision latency

### Week 4: Memory (Phase 4)
6. ✅ Memory Compression (4.2)
7. ✅ Multi-Model Fallback (4.3)
- **Result**: Unlimited history, resilient

### Week 5+: Polish (Phases 5-6)
8. ✅ Hormone Timeline (5.1)
9. ✅ Adaptive MoE (6.2)
10. ✅ Quick wins as time permits

---

## 🔗 Dependencies Between Features

```
Tools (1.1, 1.2)
    ↓
Personality (2.1) ← uses tool results for context
    ↓
Proactivity (2.2) ← uses personality + tools
    ↓
Vision Caching (3.1) ← proactivity can trigger vision checks
    ↓
Memory Compression (4.2) ← tools generate logs to compress
    ↓
Adaptive MoE (6.2) ← learns from tool success rates
```

---

## 📝 Notes

- **Tool Execution**: Consider sandboxing/security for arbitrary file/web access
- **Personality Tuning**: Will need A/B testing with actual use; prompts are iterative
- **Proactivity Balance**: Too aggressive = annoying; monitor user reactions
- **Context Limits**: Even with compression, track total tokens to prevent overflow
- **Hormone Feedback**: Proactivity + tools may change endocrine dynamics; monitor stability

---

## 🌙 Night Shift (extras fora do roadmap original)

- [x] **Presença via webcam** (`PresenceDetector.hpp/cpp`): Haar cascade (copiado pro `config/`), webcam aberta só por check (~1s, LED pisca e apaga — privacidade). Tools novas: `webcam_check` (tá alguém aí?) e `webcam_photo`. Proatividade consciente de presença: gatilho `UserReturned` dá boas-vindas quando você volta depois de 3+ min fora (ignora cooldown; oxitocina sobe via `trigger_social_response`), e **nenhum** gatilho fala com a cadeira vazia. Config: `presence_detection`, `presence_check_interval_s` (90s), `min_away_for_welcome_s` (180s) no proactivity.json.
- [x] **Hot-reload de personalidade**: `personality.json` é recarregado por mtime a cada turno — tuning de tom sem reiniciar (quick win "Config Hot-Reload", versão personalidade).
- [x] **Humor do dia**: offsets determinísticos (hash da data, ±0.08) nos baselines hormonais na inicialização — a Alyssa acorda um pouco diferente a cada dia.
- [x] **Período do dia**: bloco [PERSONALIDADE] agora inclui madrugada/manhã/tarde/noite — ela sabe que horas são sem chamar tool.
- [x] **Tool errors amigáveis**: `list_dir` normaliza paths vagos do LLM ("essa pasta", "aqui" → `.`) e os erros trazem dica de correção (`use path=.`) pro modelo se recuperar sozinho.
- [x] **Paths humanos**: `~` expande pra pasta do usuário; política de acesso = pasta do projeto + home (sistema continua bloqueado). "lista minha pasta ~" funciona.
- [x] **Retry de tool**: quando uma ferramenta falha e ainda há rodada disponível, o feedback instrui explicitamente a corrigir os parâmetros e chamar de novo — modelo 4B não infere isso sozinho.
- [x] **Away Leisure** 🎉: usuário fora há 4+ min (webcam) → Alyssa escolhe algo dos interesses dela e abre no navegador via tool `open_url` (allowlist http/https + charset rígido), comentando consigo mesma. Uma vez por ausência. Quando o usuário volta, o welcome-back menciona o que ela estava fazendo. Config: `away_leisure`, `leisure_after_away_s`.

## 🤝 Companion Mode (pedidos do Deyvid)

- [x] **Jogo da velha** (`TicTacToe.hpp` + tool `jogo_da_velha`): lógica 100% em C++ (vencer > bloquear > centro > canto) — o 4B só transporta jogadas, não tem como trapacear. "vamos jogar velha" → ela chama a tool, tabuleiro ASCII no chat.
- [x] **Memória de gostos** (`UserPrefs.hpp` + tools `save_preference`/`list_preferences`): persistido em `user_prefs.json` (dedup por valor). System prompt instrui a salvar quando o usuário demonstra gostar de algo. O modo lazer injeta `[MEMÓRIA DE GOSTOS]` no prompt — ela abre o que aprendeu que você gosta.
- [x] **Fast path de small talk**: `is_small_talk()` → pula comitê, gating, memória e fusão; uma única chamada 4B com personalidade+tools. Log `[Fast Path] Resposta em Xms` pra medir. Maior corte de latência disponível sem tocar em modelo.
- [x] **Logs na aba certa**: `UILogStreambuf` redireciona std::cout/cerr pra aba Logs do FTXUI (antes só llama.cpp e presença apareciam; o resto vazava por baixo da UI). printf não passa (raro, aceitável).
- [x] **Dataset pra LoRA** (`training_data.jsonl`): toda interação vira uma linha {timestamp, input, response, mode, emotional_state} — quando chegar a hora do fine-tune do Gemma, a matéria-prima já cresceu sozinha. Best-effort, nunca derruba turno.
- [x] **Pós-tool concreto**: o prompt de feedback exige dados reais dos resultados (nomes, números, 5-8 itens + total em listas longas) — fim do "tem um monte de coisa aí".
- [x] **Mãos da Alyssa** (`InputController.hpp/cpp` via SendInput): tools `screen_info`, `mouse_move`, `mouse_click`, `keyboard_type` (Unicode, acentos ok, máx. 300 chars), `keyboard_key` (teclas e combos tipo ctrl+s/alt+tab). Coordenadas clampadas à tela; combo com tecla desconhecida = erro. Remover as entradas do registry desativa tudo. Linux = stub com erro amigável (ydotool fica pra depois).
- [x] **Aba 🛠 Tools na UI** (Fase 5.2): log de chamadas com timestamp, OK/ERR colorido, duração e preview do resultado — mais recente primeiro.
- [x] **Personality display na UI** (Fase 5.3): aba Endocrine mostra a linha de estado atual ("energia alta, de bom humor, madrugada"), atualizada a cada turno.

## Status Tracking

- [x] Phase 1 Complete (Tools) — `ToolExecutor.hpp/cpp` + `config/tools_registry.json`; tools: get_datetime, read_file, list_dir, system_metrics, screenshot, web_search. Testes em `tests/test_tool_executor.cpp` (47 casos). Limitação conhecida: no caminho com voz, a resposta intermediária contendo `[TOOL_CALL]` é falada pelo TTS antes da resolução (resolver junto com Voice Streaming).
- [x] Phase 2 Complete (Personality + Proactivity)
  - 2.1: `PersonalityCore.hpp` + `config/personality.json`; bloco [PERSONALIDADE] injetado no prompt fundido com estado atual modulado pelos hormônios (cortisol→paciência, dopamina/adrenalina→energia, oxitocina→afeto). System prompt da Alyssa reescrito (garota normal); bloco de "consciência térmica" removido do prompt — se quiser de volta, virou caso de uso da tool system_metrics.
  - 2.2: `ProactivityEngine.hpp` + `config/proactivity.json`; gatilhos de tédio (idle 5min+), cortisol alto (checa o usuário) e dopamina alta (sugere atividade), com cooldown de 10min. Thread no Alyssa_CLI usa try_lock no brain_mtx (nunca interrompe inferência). `generate_proactive_message()` pula o comitê MoE e não grava na LTM. Gatilho de "interesting event" via visão fica pra quando a Fase 3.1 baratear o pipeline de visão.
- [x] Phase 3 Complete (Performance) — 3.1 FrameCache com diff parcial na região do crop (testes 16/16, ~11x no bench); 3.2 pool de contextos paralelos (ver acima). Falta medir o ganho real do comitê paralelo em uso (log `[MoE Execution] ... Xms`).
- [x] Phase 4 Complete (Memory) — 4.2 resumo rolante + 4.3 resiliência (timeout/load/resposta). 4.1 (Dynamic Context Window) pulado de propósito: não está na ordem recomendada, o contexto-base dos experts já opera em 4096 na prática, e mexer no n_ctx da Alyssa (32k) merece medição de VRAM antes.
- [x] Phase 5 Complete (UX) — 5.1 timeline de hormônios na aba Endocrine (graphs FTXUI, amostra 10s, janela 30min); 5.2 aba 🛠 Tools com log de chamadas; 5.3 personality display. Falta só marcar eventos na timeline ("harsh input" etc.) — fica como polish futuro.
- [ ] Phase 6 Complete (Advanced MoE)
