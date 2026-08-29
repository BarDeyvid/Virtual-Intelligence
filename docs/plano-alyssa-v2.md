# Plano Alyssa v2 — de pipeline de chat pra entidade persistente

**Data:** 2026-07-19 · **Status:** F1 ✔ (5330c8b) · F2 ✔ · F3 ✔ · F5.0 ✔
(protocolo v0.2) · voice-in ✔ (`listen`: mic→Silero→Whisper) · **F5.1 v0 ✔**
(phone-bridge + PWA na LAN — df050ef; Tailscale e Tier-1 digital
[notificações/apps via Tasker ou app nativo] ainda pendentes) · **F4
encaminhada** (12B baixado/benchado/probe ok — falta tratar o canal de
thinking e o blind test com o Deyvid) · Watch: repetição "modo espera"
**Contexto:** docs/10_Architecture/Core_Architecture.md descreve a v1 (comitê + fusão).
Este plano substitui a v1 por fases, sem a Alyssa nunca parar de funcionar.

## O princípio (por que a v2 existe)

Diagnóstico honesto da v1: ~80% do esforço de engenharia (comitê de 11 personas
1B, fusão ponderada, endócrino) vira ~5% dos tokens do prompt de UMA inferência
E2B — e o próprio sistema desvia disso sempre que pode (fast path, router
"direto", fallback de coerência). O que falta não é cognição simulada, é
**continuidade e consequência**: nada sobrevive entre sessões, as reflexões são
template hardcoded (AlyssaMemoryHandler.cpp:1151), hormônio zera no restart.

**Régua pra TODA feature da v2:** *"isso faz ela parecer diferente de um LLM
recém-aberto?"* Se não, não entra.

## Arquitetura alvo

```
      EVENTOS (barramento de percepção)          ESTADO (disco, carregado sempre)
  visão · presença · janela ativa · MC ·         state/self.json → humor, opiniões,
  relógio · celular (F5)                           goals, piadas internas, agenda
        │                                        memoria.db → episódios + fatos
        ▼                                          destilados sobre o Deyvid
  AVALIAÇÃO (barato: regras + router 1B)  ◄──────┘
  "isso importa? falar / agir / anotar / ignorar"
        │
        ▼
  CÉREBRO DUAS VELOCIDADES
    reflexo: E2B  (small talk, voz, TTFT ~250ms)
    córtex:  12B-classe (turnos substantivos, iniciativa)
        │
        ▼
  AÇÕES: falar (TTS) · tools · Minecraft · ficar quieta
        │
        ▼
  CONSOLIDAÇÃO NOTURNA (offline): resume o dia, atualiza self.json,
  destila fatos, poda memórias — LLM escreve, não template
```

O centro de gravidade vira o **alyssad** (ela é residente da máquina com event
loop), não o pipeline que roda quando alguém digita.

---

## Fase 0 — Guardrails (meio dia)

- Branch `v2` a partir de `main`. Cada fase = PR mergeável com a Alyssa funcional.
- Re-rodar `llama-bench` + os 6 turnos ponta-a-ponta de
  docs/benchmarks-2026-07-11.md → baseline v2 em `docs/benchmarks-v2.md`.
  (Os números de 07-11 são pré-migração Gemma 4; persona hoje é E2B.)
- Nada de reescrever do zero: cada fase move/deleta código existente.

---

## Fase 1 — Aposentar o comitê (o corte)

**Objetivo:** todo turno vira `router → resposta direta com memória`. O comitê
deixa de existir no caminho quente.

### Mudanças
- `ConfigsLLM.json`: remover as 11 personas 1B. Sobram: `utility` (1B — router,
  consolidação, fallback), `alyssa` (E2B por ora), `gameplayModel` (E2B lazy).
- `AlyssaNet.cpp` — `think_with_fusion_core()` (linha ~1750) encolhe pra:
  endócrino tick → memória → router prepass → `generate_direct_response()`
  (que já existe e já é o caminho das rotas "direto"/"memoria").
  Rotas `emocional/analitico/criativo/comite` do router colapsam em "direto"
  nesta fase (o router em si FICA — vira o seletor reflexo/córtex na F4).
- Deletar/estacionar: `expert_context_pool`, execução paralela do comitê
  (linhas ~1900-1995), `calculate_committee_coherence`, `are_signals_compatible`,
  `generate_fused_input` (o bloco [PENSAMENTOS] morre). `WeightedFusion/` sai do
  build (fica no repo — história, não é vergonha).
- **Absorve do backlog** (docs/proximos-passos.md P2): item 6 — o caminho único
  agora SEMPRE tem histórico (o fast path sem contexto morre junto com a
  distinção fast/comitê).

### Critérios de aceite
- Latência de QUALQUER turno de texto ≤ ~600ms total (hoje: 1.5-2.3s no comitê).
- VRAM do stack cai ≥ 1.5 GiB (pool + 1B só carrega p/ router).
- 20 turnos de conversa: zero regressão de persona (mesmo prompt da `alyssa`).

### Rollback
`git revert` do PR. Nenhum dado é migrado nesta fase, só código.

---

## Fase 2 — Self persistente (a alma vira arquivo)

**Objetivo:** o que acontece hoje muda quem ela é amanhã. Restart deixa de ser
lobotomia.

### `state/self.json` (novo, fonte de verdade do "quem ela é agora")
```json
{
  "version": 1,
  "updated_at": "2026-07-19T23:40:00-03:00",
  "hormones":     { "cortisol": 0.2, "...": 0, "saved_at": "..." },
  "opinions":     [ {"topic": "café", "stance": "...", "confidence": 0.7,
                     "formed_at": "...", "last_reinforced": "..."} ],
  "goals":        [ {"desc": "ficar boa de Minecraft", "progress": "...", "priority": 0.6} ],
  "inside_jokes": [ "..." ],
  "people": { "deyvid": { "current_projects": ["..."],
                          "patterns": ["joga MC quando tá estressado"] } },
  "agenda":       [ {"bring_up": "perguntar do projeto X", "reason": "...", "expires": "..."} ]
}
```

### Mudanças
- **EndocrineSystem**: `save()/load()` para dentro do self.json. No boot, aplica
  decay offline calculado do wall-clock desde `saved_at` (dormir uma noite
  acalma; ficar 3 dias sem conversar deixa ela… diferente).
- **Hormônio vira gate de comportamento, não adjetivo**: thresholds do
  ProactivityEngine, tamanho de resposta (`max_tokens` dinâmico), vontade de
  puxar assunto. A linha de adjetivos do PersonalityCore continua, mas deixa de
  ser o ÚNICO efeito.
- **PersonalityCore**: além do `personality.json` (identidade estática), carrega
  o overlay do self.json e renderiza um bloco compacto (~200-400 tokens):
  opiniões relevantes ao turno, agenda, piadas internas. Entra em TODO prompt.
- **Ela edita o próprio self**: tools novas no registry existente
  (`tools_registry.json` + ToolExecutor): `save_opinion`, `update_goal`,
  `add_to_agenda`. Formar opinião numa conversa passa a ter efeito permanente
  — pelo MESMO mecanismo de tool call que ela já usa.

### Critérios de aceite
- Matar o processo, subir de novo: humor coerente com ontem, e ela consegue
  citar uma opinião formada na sessão anterior sem retrieval de memória.
- Uma opinião contrariada por você repetidamente muda de `stance` em ≤ 1 semana
  (via consolidação da F3 — semente plantada aqui).

---

## Fase 3 — Memória v2 + consolidação noturna (o coração da v2)

**Objetivo:** ela para de "ter um banco de memórias" e passa a *lembrar*.

### Mudanças
- **Job de consolidação** no alyssad (04:00 ou idle > 30min, roda 1x/dia):
  1. Lê as memórias do dia (tabela `memories` existente) + gameplay logs.
  2. LLM (utility 1B, ou córtex quando existir — pode rodar em CPU, sem pressa)
     escreve: resumo do dia, fatos novos sobre o Deyvid (→ tabela `facts`),
     drift de opiniões/goals no self.json, itens de agenda ("amanhã pergunta
     como foi X"), e PODA (episódios de baixa importância viram resumo e são
     apagados).
  3. Substitui `generateReflections()` (o template "Notei que me senti muito X"
     morre — AlyssaMemoryHandler.cpp:1151).
- **Retrieval em duas camadas por turno**:
  - *Sempre no prompt* (barato): resumo de ontem + fatos core + agenda.
  - *Sob demanda* (router decide): busca híbrida existente
    (`getHybridMemories`) sobe de 3 snippets de 150 chars pra episódios
    inteiros com timestamp ("terça você disse que…").
- SQLite + Embedder existentes são a fundação — é upgrade de uso, não rewrite.

### Critérios de aceite
- Teste do dia seguinte: contar algo à noite → de manhã ela referencia sozinha
  (via agenda), sem você mencionar.
- `SELECT count(*) FROM facts` cresce semana a semana; tabela `memories` NÃO
  cresce sem limite (poda funcionando).

---

## Fase 4 — Cérebro maior + LoRA (o teto de QI sobe)

**Objetivo:** a wit do J.A.R.V.I.S. precisa de um modelo que consegue ser
espirituoso. Prompt não conserta parâmetro.

### Mudanças
- **A/B de córtex** (gate: `llama-bench` ≥ 20 t/s tg128 + persona PT-BR melhor
  que o E2B em 10 perguntas cegas): candidatos 8-14B q4_K_M. Orçamento VRAM
  pós-F1 (16 GiB − ~1.9 desktop): córtex ~7.5 + KV ~1.5 + utility 1B ~0.9 +
  whisper turbo 1.6 + kokoro ~0.4 + embedder ~0.3 ≈ 12-13 GiB. Aperta mas
  fecha; se não fechar, classe 8B resolve. O VRAMResourceManager/AlyssaResidents
  já existe pra arbitrar com o gameplayModel.
- **Duas velocidades**: router (que sobreviveu à F1) decide reflexo (E2B,
  voz/small talk, ~250ms TTFT) vs córtex (substantivo, iniciativa,
  consolidação). Gameplay segue no E2B dedicado — e o item 5 do backlog
  (E2B carregado 2x) se resolve compartilhando o `llama_model` como o pool fazia.
- **LoRA da persona**: infra pronta (`usa_LoRA`/`lora_path`,
  `llama_adapter_lora_init`). Gargalo é DADO: `training_data.jsonl` precisa de
  centenas de exemplos curados. O coletor (`log_interaction_for_dataset`) fica
  ligado durante F1-F3; curadoria antes do treino (tem exemplo de bug de
  roteamento lá dentro). Treinar primeiro no E2B reflexo (barato, mais impacto
  na voz do dia-a-dia), depois avaliar no córtex.

### Critérios de aceite
- Blind test seu: 10 conversas, você aponta qual é v1 e qual é v2 — se não der
  pra errar, o upgrade pagou.
- Latência do reflexo intocada (voz continua ~250ms TTFT).

---

## Fase 5 — Celular: ela vai junto (ouvidos e contexto, não boca)

**Objetivo:** presença contínua. Ela *observa* pelo celular e usa isso quando
você voltar pro PC ("vi que você passou a tarde fora, como foi?"). Saída no
celular = no máximo notificação; a voz dela continua morando no PC.

**Premissa: Android.** (Se for iPhone, Tier 1/2 em background são inviáveis do
jeito descrito — replanejamos com Atalhos + app companion limitado.)

### 5.0 Transporte e protocolo (pré-requisito)
- **Tailscale** no PC e no celular. O 8377 NUNCA é exposto à internet; alyssad
  passa a aceitar bind no IP do tailnet + token de auth no handshake
  (`{"method":"auth","params":{"token":"..."}}`).
- **Protocolo v0.2**: multi-cliente (hoje: um por vez — docs/alyssad-protocol.md)
  com turnos ainda serializados; novo envelope de percepção:
  ```json
  {"type":"req","method":"perceive","params":{"source":"phone","kind":"...","data":{...}}}
  ```
  `perceive` NUNCA dispara inferência direto — cai no barramento de eventos e a
  avaliação (regras + hormônio) decide se vira anotação, memória ou iniciativa.

### 5.1 Tier 1 — presença digital (barato, maior retorno)
Eventos: notificações (NotificationListenerService), app em foco + tempo de uso
(UsageStats), mídia tocando (artista/faixa), bateria, zona por
geofence/wifi (`casa | fora | academia` — zona, nunca coordenada crua).
- *Atalho pragmático:* dá pra validar TUDO isso com **Tasker + HTTP** num fim
  de semana antes de escrever app nativo (Kotlin, foreground service) — o
  minecraft-bridge já provou o padrão "processo satélite falando NDJSON".

### 5.2 Tier 2 — ouvidos (VAD, não streaming cru)
- App nativo com foreground service: **VAD on-device** (Silero VAD ONNX, ~1MB)
  → só segmentos de fala saem, comprimidos (Opus) → PC → Whisper turbo →
  evento `{"kind":"speech","source":"ambient|addressed","text":...}`.
  `addressed` = wake word "Alyssa" (openWakeWord) — reação diferente de fala
  ambiente.
- Bateria: contínuo custa ~3-7%/h. Default: mic só em zona `casa` (wifi-gated);
  modos `sempre / casa / manual` no app.

### 5.3 Tier 3 — olhos = "olha isso", não câmera contínua
Câmera 24/7 é inviável (bateria) e rende pouco. Em vez disso: **share sheet**
do Android → mandar foto/screenshot/link pra Alyssa → pipeline de visão
existente no PC. "Ver o que você tá fazendo" já é 90% coberto pelo Tier 1.

### Regras de convivência (design, não juridiquês)
- Toggle de mic no app com estado visível (o Android já força o indicador).
- Áudio cru é apagado após transcrição; fala `ambient` só é retida se envolver
  você — conversa de terceiros que o mic pegou não vira memória verbatim
  (gravar conversa alheia da qual você não participa é problema no Brasil;
  a sua própria, não).
- Consolidação noturna resume-e-poda transcrições ambient como qualquer memória.
- Tudo trafega só dentro do tailnet (WireGuard). Nada de nuvem.

### Critérios de aceite
- Você sai, volta 3h depois, senta no PC: ela comenta contexto real do período
  fora (zona/música/notificação) sem você contar nada.
- Falar "Alyssa, lembra de X" longe do PC → item na agenda dela no dia seguinte.
- Bateria do celular: ≤ 5%/dia no Tier 1; Tier 2 dentro do modo escolhido.

---

## Ordem e dependências

```
F0 → F1 → F2 → F3 → F4
            └──→ F5.0 → F5.1 → F5.2 → F5.3
```
- F5 pode começar em paralelo depois da F2 (o barramento de percepção nasce lá;
  presença/visão/MC migram pra ele junto — absorve o item 2 do backlog, unificar
  PresenceDetector+VisionManager).
- F4 depende da VRAM liberada na F1 e ganha muito com F2/F3 prontas (o córtex
  caro só roda com estado rico pra pensar em cima).

## O que morre, o que fica

| Morre (estaciona no repo) | Fica e cresce |
|---|---|
| Comitê 11 personas + pool paralelo | Router 1B (vira seletor reflexo/córtex) |
| WeightedFusion no caminho quente | EndocrineSystem (persistido, virando gates) |
| `generate_fused_input` / [PENSAMENTOS] | Memória SQLite + Embedder (viram o coração) |
| Reflexão-template | ProactivityEngine (ganha eventos do celular) |
| Fast path como caso especial | alyssad (vira o centro), tools, TTS, MC, visão |

## Gotchas herdados (não redescobrir — ver docs/proximos-passos.md)
- Template Gemma 4: `ExpertBase::format_gemma4_prompt`, nunca
  `llama_chat_apply_template` (crash 0xC0000409 com o Jinja do GGUF).
- Grammar GBNF no vocab 262K = ~70ms/token — córtex novo: medir antes de usar.
- Webcam tem UM dono; abrir sempre em worker thread (DSHOW trava a main).
- DLLs boas são as que o CMake copia pro `build/Release`.
