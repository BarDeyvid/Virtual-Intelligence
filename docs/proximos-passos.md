# Próximos passos — handoff 2026-07-12 (Fable → Sonnet)

Estado completo: memória `night-session-2026-07-12` + docs/plano-router-e-voz-gameplay.md
+ docs/gemma4-migration.md + docs/benchmarks-2026-07-11.md. NADA COMMITADO.

## P1 — Validações pendentes (o Deyvid testa, a IA corrige)
- alyssa_cli com visão: deve subir sem congelar, logar "[VisionManager] Câmera aberta".
  Se crashar ainda: o fix foi cascade vazio + dupla captura (ver memória).
- Minecraft retest: kick sumiu? (atacar recusava drops), "[Gaia ouviu]" aparece e obedece?
  Goals avançam? ("madeira" primeiro — ela comemora no chat a cada objetivo)
- Whisper turbo-q8 no chat de voz normal (sem --minecraft): sotaque ok? "Alyssa" sai
  certo agora (nome no initial_prompt)? Se sim, pode apagar ggml-large-v3.bin (3GB).

## P2 — Próximas features (ordem de valor)
1. Treino solo: server offline-mode=false sem ninguém + goals ligados + horas rodando
   → analisar logs/gameplay*.jsonl (taxa de sucesso por objetivo, loops)
2. Unificar câmera: PresenceDetector + VisionManager num pipeline só (hoje a presença
   desliga quando a visão liga)
3. FaceRecognizer real: ArcFace/FaceNet ONNX (o atual é placeholder: rosto grande="Deyvid")
4. Auditar logs/router_decisions.jsonl (~1 semana de uso) → se rotas ruins, prompt melhor
   ou upgrade pro E2B; encurtar o prompt do router (~500ms → ~200ms)
5. E2B compartilhado: persona+gameplay carregam o MESMO gguf 2x quando MC liga
   (~3GB desperdiçados) — compartilhar llama_model como o pool faz
6. Fast path com histórico (últimas 2-3 trocas; hoje responde sem contexto)
7. Destilação 270M (gate: 10k+ ticks bons no GameplayLog — deixa acumular)

## Gotchas pro próximo modelo (NÃO redescobrir)
- Template Gemma 4: NUNCA llama_chat_apply_template (não reconhece) nem minja com o
  Jinja do GGUF (0xC0000409). Usar ExpertBase::format_gemma4_prompt / env var
  LLAMA_ARG_CHAT_TEMPLATE (receita em docs/gemma4-migration.md)
- Grammar GBNF no vocab 262K = ~70ms/token. Gameplay roda SEM grammar (removida do
  config; rollback = devolver grammar_file)
- DLLs: llama.cpp/build_cuda rebuildado com CUDA_ARCHITECTURES=120-real;120-virtual;
  build/Release funciona porque a cópia do whisper sobrescreve o ggml (gambiarra OK)
- Webcam tem UM dono: PresenceDetector OU VisionManager (vision_pipeline_running())
- Câmera/webcam: abrir sempre em worker thread (DSHOW trava a main por segundos)
- test_asr_ab / test_gameplay_audio / test_kokoro --say = harness de teste sem mic
