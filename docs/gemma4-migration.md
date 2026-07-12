# Migrar tudo pra Gemma 4? — análise (2026-07-12)

Pergunta do Deyvid: vale migrar TODOS os modelos pra 4ª geração?
Resposta curta: **persona e gameplay sim (feito); comitê "depende"; embedder
não tem substituto; e o futuro interessante é o áudio nativo.**

## Estado por modelo

| papel | hoje | Gemma 4? |
|---|---|---|
| alyssa (persona) | ~~gemma-3-4b~~ → **gemma-4-E2B** ✅ | feito 2026-07-12 |
| gameplayModel | gemma-4-E2B ✅ (agora funciona de verdade — ver fix de template) | já era |
| comitê ×11 | gemma-3-1b | **sem equivalente**: não existe Gemma 4 1B |
| embedder | embeddinggemma-300M | linha separada, sem sucessor Gemma 4 |
| STT | whisper large-v3-turbo q8 | Gemma 4 E2B/E4B têm ASR nativo (ver abaixo) |

## Comitê: as 3 opções

1. **Manter gemma-3-1b (recomendado por ora).** Os experts produzem sinais
   curtos de formato fixo; 110 t/s e 0.93GiB de pesos. O elo fraco do comitê
   hoje não é o modelo, é o parsing/aderência — que LoRA ou few-shot no
   role_instruction resolvem mais barato que 3× mais VRAM.
2. **E2B compartilhado pra TUDO (experimento que vale a pena).** O pool de
   contextos já compartilha pesos (`AlyssaCore(model, ctx, n_batch)`); dava
   pra base = E2B e a alyssa usar o MESMO modelo (hoje ela tem core próprio
   com pesos duplicados… aliás, quando o Minecraft liga, o E2B carrega DUAS
   vezes — persona e gameplay; unificar economiza 3.1GiB). Zoo inteiro num
   único GGUF: ~3.1GiB de pesos + KVs. Custo: comitê ~30-40% mais lento
   (78 vs 110 t/s, prefill 8.2k vs 18.4k). Ganho: experts MUITO mais espertos
   (MMLU-Pro 60% vs ~20-30% do 1B) e -0.9GiB de pesos vs manter o 1B junto.
3. **E4B como persona** se a E2B decepcionar na conversa longa (MMLU-Pro
   69.4% vs 60.0%): arquivo Q4_0 ~4.4GiB, ainda cabe. Não baixado ainda.

## Áudio nativo — TESTADO E FUNCIONA (2026-07-12)

E2B/E4B/12B aceitam áudio de até 30s nativamente. **Testado neste PC**:
`llama-mtmd-cli` + `models/mmproj-gemma-4-E2B-it-Q8_0.gguf` (557MB, ggml-org)
transcreveu o áudio pt-BR de teste PERFEITAMENTE (idêntico ao Whisper
large-v3, mesma grafia "Alissa"). Custo por enunciado com modelo quente:
**<1s para 7.9s de áudio** (total frio 3.3s, sendo ~2.3s de load).

Receita que funciona (CORRIGIDA após o teste de microfone do Deyvid):
```
# o template Gemma 4 mínimo vai inline via --jinja --chat-template
# (é o mesmo turn format do ExpertBase::format_gemma4_prompt)
TMPL='{{ bos_token }}{% for m in messages %}<|turn>{{ "model" if m["role"] == "assistant" else m["role"] }}{{ "\n" }}{{ m["content"] | trim }}<turn|>{{ "\n" }}{% endfor %}{% if add_generation_prompt %}<|turn>model{{ "\n" }}{% endif %}'
llama-mtmd-cli -m models/gemma-4-E2B_q4_0-it.gguf \
  --mmproj models/mmproj-gemma-4-E2B-it-Q8_0.gguf \
  --audio fala.wav --temp 0 --jinja --chat-template "$TMPL" \
  -p "Transcribe the following speech segment in Portuguese into Portuguese text. Only output the transcription, with no newlines. <__media__>"
```
(O test_asr_ab passa o template por env var — LLAMA_ARG_JINJA=1 +
LLAMA_ARG_CHAT_TEMPLATE — que o subprocesso herda sem briga com o cmd.exe.)

Pegadinhas descobertas (perdemos uma manhã nelas):
- O minja (Jinja do common/) dá fail-fast **0xC0000409** no template de 16KB
  embutido no GGUF — mesmo root cause do fix `format_gemma4_prompt()` do
  ExpertBase, vestido de crash silencioso. NUNCA rodar sem template explícito.
- **`--chat-template gemma` (formato Gemma 3) NÃO serve**: funciona no áudio
  sintético limpo da Kokoro mas devolve VAZIO (EOG imediato) na maioria dos
  áudios reais de microfone. Descoberto no teste de sotaque do Deyvid
  (2026-07-12): 8 de 12 enunciados vazios; com o turn format correto do
  Gemma 4, 12 de 12 transcrevem. O formato `<|turn>` importa DE VERDADE.
- O marcador `<__media__>` vai NO FIM do prompt (model card: áudio depois do
  texto); sem ele o mtmd-cli enfia no começo.
- O `ggml-cuda.dll` do llama.cpp/build_cuda estava sem SASS sm_120 (ver
  forense em benchmarks-2026-07-11.md) — rebuildado com `120-real;120-virtual`.

## Veredito do teste de sotaque (colônia sulista, 12 enunciados, 2026-07-12)

- **Whisper turbo-q8: 12/12**, 96-229ms. Nenhum erro no sotaque. (Nota: com
  `language=pt` forçado ele TRADUZ fala em inglês pra português.)
- **Gemma E2B** (template correto): transcreve tudo, mas com erros reais em
  ~2 de 5 re-testados ("Testando a minha voz" → "Estes estão na minha voz";
  perdeu "se"/"ia ser bem melhor" no enunciado longo). Curiosidade: registra
  fala informal com mais fidelidade ("tô" onde o Whisper normaliza "estou") e
  transcreve inglês como inglês em vez de traduzir.
- **Decisão**: Whisper turbo-q8 continua o STT do chat. E2B direto é bom o
  bastante pra COMANDO de gameplay (intenção > ortografia) — protótipo do
  áudio-direto no Minecraft segue viável.
- Bônus: o mic do Deyvid tá com ganho BAIXO (peak 4-8% do full scale) —
  subir o nível de entrada no Windows ajuda os dois motores.
- Os WAVs de cada sessão ficam em build/Release/asr_ab_wavs/ (agora com
  prefixo de sessão) — semente do dataset pro fine-tune de sotaque.

Direção pro "gameplayModel ouvindo voz direto" (ideia do Deyvid): viável —
integrar libmtmd no core do gameplay (mmproj residente ao lado do E2B,
+557MB), VAD captura o enunciado e anexa ao prompt do tick com `<__media__>`;
a grammar continua garantindo `[AÇÃO]` válida. UMA inferência faz
ASR+decisão. Recomendação: manter Whisper no CHAT (fidelidade da transcrição
importa — vai pra memória SQLite), experimentar áudio direto no MINECRAFT
(pior caso é uma ação errada, não uma memória corrompida). Validar o sotaque
real no microfone antes — o teste usou voz sintética limpa da Kokoro.

## Thinking mode

`<|think|>` existe na E2B mas fica DESLIGADO na persona (latência de
conversa). Possível uso pontual: ligar só no analyticalModel/memoryModel
num futuro "modo reflexão" offline (sonho dela? consolidação de memória de
madrugada?). Fica a ideia.

## Gotchas de infra descobertos na migração

- A API C `llama_chat_apply_template` NÃO entende o Jinja do Gemma 4 →
  `ExpertBase::format_gemma4_prompt()` renderiza na mão. Se um dia o
  submódulo llama.cpp ganhar suporte nativo a "gemma4" em llama-chat.cpp,
  o fallback manual pode sair.
- Sampling do card (temp 1.0 / top_p 0.95 / top_k 64): a chain do AlyssaCore
  só tem min_p+temp+penalty — apliquei temp 1.0; o `top_p` do JSON é lido
  mas NUNCA entra na chain (variável morta em AlyssaCore.hpp:321). Se quiser
  fidelidade total ao card, adicionar top_k/top_p na chain — mas min_p 0.05
  é geralmente superior; deixei quieto.
- Penalty de repetição 1.3/64 é agressiva pros padrões Gemma 4; se a persona
  E2B soar "travada", esse é o primeiro botão a afrouxar (1.1-1.15).
