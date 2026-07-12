# Benchmarks — 2026-07-11 (madrugada, sessão autônoma)

Hardware: RTX 5060 Ti 16GB (cc 12.0), Ryzen 7 5800X. llama.cpp build 399739d5c (9443), CUDA.
Medido com `llama-bench` (copiado para `build/Release` — os DLLs de
`llama.cpp/build_cuda/bin/Release` estão VELHOS/quebrados, dão `CUDA error`;
os DLLs bons são os que o CMake copia pro `build/Release`).

## llama-bench (pesos + tok/s, GPU cheia)

| modelo | tamanho | pp512 | pp2048 | tg128 |
|---|---:|---:|---:|---:|
| gemma3 **4B** Q4_0 (alyssa) | 2.93 GiB | 6 338 t/s | 6 433 t/s | **62.4 t/s** |
| gemma3 **1B** Q4_0 (comitê ×11) | 0.93 GiB | 17 348 t/s | 18 369 t/s | **110.3 t/s** |
| gemma **E2B** Q4_0 (gameplay) | 3.10 GiB | 7 610 t/s | 8 180 t/s | **77.8 t/s** |

Observações:
- **E2B é ~25% mais rápido que o 4B** em geração e ~27% em prefill, apesar de
  maior em disco (MoE: ~2B ativos). Candidato a A/B como modelo da persona.
- Prefill nunca é gargalo (>6k t/s): o custo de re-prefilar o prompt inteiro
  a cada turno (KV clear) é de ~100-300ms nos prompts atuais.

## Ponta a ponta (alyssad real, sem TTS, 6 turnos)

| tipo | exemplo | total | TTFT |
|---|---|---:|---:|
| fast path (small talk) | "oi" | 330-370 ms | ~250 ms |
| comitê + fusão | "qual sua opinião sobre café de manhã?" | 1.5 s | 0.9 s |
| comitê + fusão (longos) | "tô meio cansado hoje..." | 2.0-2.3 s | ~0.95 s |

- VRAM com o stack carregado: **~8.1 GiB** (1B + pool ×3 + 4B + embedder;
  total do sistema 10.0 GiB com ~1.9 GiB de desktop).
- gameplayModel (E2B) agora é **lazy**: os ~3.2 GiB só entram na VRAM no
  primeiro `/mc start` (antes carregava em todo boot).
- 6 turnos seguidos sem "Falha ao decodificar prompt chunk", sem vazamento de
  "(Emoção: ...)", sem terceira pessoa — as mudanças de KV clear + histórico
  enxuto do run_expert aguentaram.

## Recomendações (por ordem de retorno/esforço)

1. **Whisper large-v3 → large-v3-turbo** (`ggml-large-v3-turbo.bin`, ~1.6GB).
   Decoder 4 camadas vs 32: ~5-8× mais rápido no mesmo hardware, perda de
   qualidade pequena. Diminui MUITO a janela de JIT do scheduler (menos VRAM
   e menos tempo carregado). Baixar de ggerganov/whisper.cpp no HF.
2. **A/B do E2B como persona** (é mais rápido que o 4B). Rodar as mesmas
   perguntas nos dois e comparar persona/PT-BR. Se o E2B segurar a persona,
   é upgrade de latência de graça (mas +0.2GiB de VRAM).
3. **LoRA da persona no 4B**: a infra JÁ existe (`usa_LoRA`/`lora_path` no
   config, `llama_adapter_lora_init` no ExpertBase). O que falta é dado:
   `training_data.jsonl` tem só 16 exemplos — e vários são o bug de roteamento
   corrigido hoje (pergunta caindo no fast path). Deixar o coletor rodando
   umas semanas, curar ~300-1000 pares bons, e treinar QLoRA r=16 fora
   (unsloth/llama-factory) → carregar via config sem recompilar nada.
   Alvos do fine-tune: aderência de persona (zero assistente), respostas
   curtas, e o formato [SINAL]/[CONFIANÇA] dos experts do comitê (hoje
   depende de degradação graciosa do parser).
4. **Fast path com histórico**: o fast path não vê as últimas mensagens —
   "bom dia" depois de 10 turnos responde como se fosse a primeira fala do
   dia. Injetar as 2-3 últimas trocas no prompt do fast path é barato
   (prefill é rápido) e melhora continuidade.
5. **llama-bench**: manter uma cópia em `build/Release` (feito hoje) e medir
   depois de cada update do submódulo llama.cpp — os DLLs de
   `llama.cpp/build_cuda/bin/Release` estão dessincronizados dos do app.

---

# Adendo 2026-07-12 (manhã)

## Whisper large-v3-turbo (novos .bin baixados pelo Deyvid)

whisper-cli, GPU, beam 5, áudio pt-BR de 7.3s (audition da Kokoro), regime
quente (a 1ª rodada de cada quantização paga JIT de kernel CUDA p/ cc 12.0
— ignorar a 1ª medição):

| modelo | arquivo | load | transcrição | total |
|---|---:|---:|---:|---:|
| large-v3 (antigo default) | 3.0GB | 2803ms | ~890ms | 3696ms |
| turbo f16 | 1.6GB | 1660ms | ~415ms | 2076ms |
| **turbo q8_0 (novo default)** | 874MB | **925ms** | ~415ms | 1339ms |
| turbo q5_0 | 574MB | 723ms | ~425ms | 1146ms |

Saída idêntica e correta nos 4. Como o Whisper é TIER2_JIT (carrega durante a
fala), o load 3x menor é o ganho que importa. Default trocado em
`Alyssa_CLI_WITH_VOICE.cpp` p/ **q8_0** (fidelidade mais perto do f16 que o
q5 por só +200ms de load); reserva do WhisperResident 3468→1100 MiB.
Validar com microfone real (sotaque) antes de apagar o large-v3.

## Persona 4B → E2B (aprovado pelo Deyvid)

- `config/ConfigsLLM.json`: alyssa → `gemma-4-E2B_q4_0-it.gguf`, temp 1.0
  (recomendação do model card; top_p/top_k do card não existem na chain do
  AlyssaCore — o min_p 0.05 cobre).
- **Fix necessário**: o GGUF Gemma 4 embute Jinja de 16KB que a API C
  `llama_chat_apply_template` não reconhece → TODO turno morria em "Erro ao
  processar template de conversa" (isso também quebrava o gameplayModel E2B
  em runtime!). `ExpertBase::format_gemma4_prompt()` agora renderiza o turn
  format na mão (`<|turn>role\n…<turn|>\n`, assistant→model, generation
  prompt `<|turn>model\n`) quando o template contém `<|turn>`.
- Qualidade: visivelmente melhor em persona — responde a pergunta feita
  ("Bom dia, dormi. Dormiu bem você?"), empatia natural, zero assistentês.
- Latência: medição da manhã INVALIDADA (Elden Ring usando 72% da GPU e
  ~4.8GiB). Re-medir com GPU ociosa; llama-bench prevê E2B ~25% mais rápido
  que o 4B.

## Forense dos DLLs (resolve o mistério do "CUDA error")

Descoberto testando o ASR do E2B (llama-mtmd-cli crashava silencioso):

1. O `ggml-cuda.dll` do `llama.cpp/build_cuda` (jun/2026) **não tem SASS
   sm_120** (Blackwell/5060 Ti): o cache dizia `90-real;...;120-real` mas o
   DLL nunca foi rebuildado depois desse configure — e sem `-virtual` no
   alvo não há PTX pra JIT. Sintomas: `no kernel image is available` no
   MUL_MAT e `cudaFuncSetAttribute` falhando no flash-attn MMA.
2. O app só funciona por um acidente feliz da gambiarra de cópia do
   CMakeLists: a cópia do whisper.cpp (que roda DEPOIS da do llama.cpp)
   sobrescreve os `ggml*.dll` em `build/Release` com o build do whisper —
   que FOI compilado certo pra Blackwell. `llama.dll` (do llama.cpp) +
   `ggml-cuda.dll` (do whisper.cpp) por coincidência de ABI.
3. llama-bench "funcionava" em build/Release e falhava em build_cuda por
   isso; llama-cli/mtmd-cli pisavam nos kernels quebrados dos dois jeitos.

**Fix aplicado 2026-07-12**: `build_cuda` reconfigurado com
`CMAKE_CUDA_ARCHITECTURES="120-real;120-virtual"` (só a GPU desta máquina +
PTX; os alvos 90/100 eram datacenter e sextuplicavam o build) e backend CUDA
rebuildado. Depois disso `llama.cpp/build_cuda/bin/Release` vira um conjunto
consistente e correto — e a cópia do app passa a levar DLLs bons dos dois
lados (o overwrite do whisper continua, inofensivo).

