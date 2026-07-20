# Benchmarks v2 — F1 (comitê) + F2 (self) + F3 (consolidação)

## F3 — aceite RODADO 2026-07-20 06:11 ✔

`@consolidate` no E2B: **3.4s** → resumo do dia em 1ª pessoa (339 chars,
fiel: açaí, entrevista 14h, boxe, projeto de sexta), **5 fatos** duráveis na
tabela `facts`, **2 itens de agenda** (o "te lembro" que falhava como tool
call na F2 agora é garantido pela consolidação), reflexão real na tabela
`reflections` (o template "Notei que me senti..." morreu). Após kill+restart:
*"Bom dia! Lembro sim. A gente tava falando sobre o ritual do café da manhã e
eu decidi que açaí com morango é melhor, né?"* — direto do bloco [ONTEM].
`test_self_state`: 31/31.

### A saga do debug (1 noite, 4 bugs reais — ver tests/test_utility_gen.cpp)
1. **Sem chat template**: summarize_history_chunk/consolidação mandavam
   instrução CRUA pro 1B → sopa de token. Fix: turn format por família
   (gemma3 `<start_of_turn>`, gemma4 `<|turn>`).
2. **repeat_penalty 1.3/64 HARDCODED** no generate_raw (AlyssaCore.hpp).
   Agora é parâmetro (default preserva a persona; sumarização usa 1.05).
3. **Auto-envenenamento**: o resumo podre do dia 1 entrava no corpus do dia 2
   e o modelo só continuava o padrão dele. Fix: day_summary fora do corpus +
   guarda de degeneração (resumo/reflexão podre é DESCARTADO, nunca gravado).
4. **O gguf do 1B é um quant quebrado**: "Q4_0" a 7.98 BPW que colapsa em
   prompts ≥ ~1k tokens — reproduzido no llama-cli PURO, em CPU e GPU, com
   corpus limpo; o 4B no mesmo harness resume perfeito. Por isso a
   consolidação roda no **E2B da persona** (já na VRAM, ocioso às 4h) com o
   1B só de fallback — que é o desenho do córtex da F4 chegando mais cedo.

### Watch-list F3
- **Trocar o gguf do 1B** por um quant são (Q4_K_M oficial) — ele segue no
  router (prompt curto + grammar funciona) e como fallback, mas está doente.
- **`top_p` NUNCA entrou na sampler chain** (generate_raw calcula e não usa —
  variável morta desde sempre; o `top_p: 0.9` da persona é ignorado). Não
  corrigi de madrugada: mudar sampling da persona pede A/B acordado.
- Fato-ruído ("uso médio de recursos do PC 21.85%") entrou na tabela facts —
  o ranking por reforço enterra, mas a consolidação pode filtrar métricas.
- `swa_full=true` ligado nos contextos (custo irrisório; era suspeito durante
  o debug, mantido como proteção pra históricos longos).

---


## F2 — aceite RODADO 2026-07-19 21:34 ✔

Morta às 21:33:32, renascida ~21:34, perguntada "açaí puro ou com morango?":
**"Açaí com morango, claro. O puro é só pra quem não sabe apreciar a vida."**
— opinião formada pela vida anterior (save_opinion chamado POR ELA, confidence
0.9), afirmada sem retrieval de memória, direto do bloco [EU].

- Boot da 2ª vida: `[Self] Carregado: 1 opinião, 1 agenda` +
  `Hormônios restaurados (decay offline 0.0h)` + `Humor do Dia: já aplicado
  hoje` (o bug de re-somar offsets a cada restart no mesmo dia morreu junto).
- `test_self_state`: 22/22 (roundtrip, decay exponencial TAU=8h, dedup,
  poda de agenda, caps do render).
- Gate hormonal de max_tokens ativo (energia/humor baixos cortam tokens).

Watch-list F2:
- add_to_agenda só disparou com pedido explícito (a promessa verbal "te
  lembro" não virou tool call sozinha). Caminho robusto é a consolidação da
  F3 escrever agenda a partir do dia — o tool fica como via expressa.
- Mini-leak: "Salvei isso aí" no fim da resposta pós-restart (consciência do
  bloco [EU]). Se repetir, reforçar o "nunca cite" no system prompt.

---


**Branch:** v2 · **Data:** 2026-07-19
**Referência v1:** docs/benchmarks-2026-07-11.md (pré-migração Gemma 4; os números
ponta-a-ponta de lá são a baseline do comitê: fast path 330-370ms, comitê 1.5-2.3s).

## O que mudou na F1

- 11 personas 1B removidas do ConfigsLLM.json → sobrou `utility` (1B),
  `alyssa` (E2B) e `gameplayModel` (E2B lazy).
- `think_with_fusion_core`: caminho único memória → `generate_direct_response`
  com histórico. Sem gating, sem pool paralelo, sem fusão, sem coerência.
- Router prepass FORA do caminho quente (volta na F4 como seletor
  reflexo/córtex). `WeightedFusion.cpp` fora do build.
- Fallback 1B (resposta vazia/erro) agora cobre TODO turno, não só o comitê.
- Small talk continua com atalho (pula busca de memória), mas SEMPRE com
  histórico — o fast path sem contexto morreu.
- Hormônios agora reagem à troca real (input+resposta). Na v1 analisavam os
  sinais do comitê em PT-BR contra keywords EN — quase nunca disparava.

## Resultados — RODADO 2026-07-19 21:18 (alyssad real, commit 5330c8b)

Driver: `node tools/alyssad_bench.js 8377 -- ...` (serializa turnos, mede
say→response e TTFT via evento `token`).

| turno | total | TTFT | obs |
|---|---:|---:|---|
| "oi" | 525 ms | 333 ms | small talk, AGORA com histórico+persona |
| opinião sobre café | 749 ms | 184 ms | opinião própria, tom certo |
| "tô meio cansado..." | 755 ms | 211 ms | empatia sem assistentês |
| "que horas são?" (tool) | 988 ms | 223 ms | tool real (21h18) + **puxou o "cansado" do turno anterior** — histórico funcionando |
| "lembra do projeto?" | 567 ms | 232 ms | DB fresco → resposta honesta |
| "boa noite" | 570 ms | 209 ms | ⚠ derail: respondeu ao turno 5, não à despedida (ver watch-list) |

| métrica | v1 (07-11) | v2/F1 | alvo F1 | veredito |
|---|---:|---:|---:|---|
| small talk ("oi") | 330-370 ms | 525 ms | ≤ 500 ms | ~ok: +160ms É o preço do histórico/persona que o fast path v1 não tinha |
| turno substantivo | 1500-2300 ms | 567-988 ms | ≤ 600 ms | parcial: mediana ~750ms, 2-3× mais rápido que v1; o excedente é DECODE (resposta mais longa), não maquinário |
| VRAM stack | ~8.1 GiB | **~3.9 GiB** (7414−3483 MiB) | ≥ 1.5 GiB a menos | ✔ folgado: ~4 GiB livres pro córtex da F4 |
| persona | — | intacta nos 6 turnos | sem regressão | ✔ (1 derail, ver abaixo) |

### Watch-list
- **Derail no turno 6**: respondeu à pergunta anterior em vez da despedida.
  Cara de limite do E2B (temp 1.0 + histórico crescendo), não de bug de
  template — mas se repetir, investigar a ordem das mensagens no
  run_expert/format_gemma4_prompt. É literalmente o argumento do córtex (F4).
- Se quiser sub-600ms já: encurtar resposta média (max_tokens 200 → ~120 ou
  instrução de brevidade no style hint). Não fiz — resposta mais rica > 150ms.

## Smoke test estrutural (sem modelos) — RODADO 2026-07-19 ✔

- Build Release limpo (alyssa_cli, alyssad, test_fusion_utils): zero erros,
  só os warnings C4267/C4244 pré-existentes (dívida da Fase 4 std::string).
  Gotcha novo: `models/` sumido quebra o POST-BUILD copy do CMake com
  MSB3073 — o diretório precisa existir mesmo vazio (recriado com README).
- `alyssad --echo` + driver NDJSON (node): ping 2ms, status ok
  (`echo:true`), say → eventos `state:thinking` → `token` → `response` →
  `state:idle`. `say` durante turno devolveu `busy` como documentado.
- `test_fusion_utils`: 33/33 passando (helpers header-only fora do caminho
  quente; ExpertContribution segue como tipo da interface IExpert).
